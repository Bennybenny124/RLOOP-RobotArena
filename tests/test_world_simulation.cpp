#include "CoreArena.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

using namespace rloop;

#define REQUIRE(expr) do { if (!(expr)) { std::cerr << __LINE__ << ": " << #expr << '\n'; std::exit(EXIT_FAILURE); } } while (false)

bool equal(Vec2 a, Vec2 b) { return a.x == b.x && a.y == b.y; }

bool equal(const WorldStateV2& a, const WorldStateV2& b) {
    if (a.seed != b.seed || a.tick != b.tick || a.tickRateHz != b.tickRateHz ||
        a.maxTicks != b.maxTicks || a.width != b.width || a.height != b.height ||
        a.result != b.result || a.obstacles.size() != b.obstacles.size()) return false;
    for (std::size_t i = 0; i < 2; ++i)
        if (!equal(a.robots[i].position, b.robots[i].position)) return false;
    for (std::size_t i = 0; i < a.obstacles.size(); ++i) {
        const auto& x = a.obstacles[i]; const auto& y = b.obstacles[i];
        if (x.id != y.id || x.type != y.type || !equal(x.center, y.center) ||
            !equal(x.halfExtent, y.halfExtent)) return false;
    }
    return true;
}

bool equal(const EventV1& a, const EventV1& b) {
    return a.kind == b.kind && a.player == b.player && equal(a.from, b.from) &&
           equal(a.to, b.to) && a.result == b.result;
}

int main() {
    ArenaConfig config;
    config.robots[1].type = RobotType::Tank;
    for (int i = 0; i < 18; ++i) {
        ActionV1 decoded;
        REQUIRE(decodeAction({i}, decoded));
        REQUIRE(decoded.attack == (i % 2 != 0));
        REQUIRE(std::abs(std::hypot(decoded.movement.x, decoded.movement.y) - (i < 2 ? 0 : 1)) < 1e-12);
        REQUIRE(encodeAction(static_cast<int>(std::round(decoded.movement.x)), static_cast<int>(std::round(decoded.movement.y)), decoded.attack).value == i);
    }
    ActionV1 ignored;
    REQUIRE(!decodeAction({18}, ignored) && !decodeAction({-1}, ignored));
    CoreArena a, b, swapped;
    REQUIRE(a.step({}).status == StepStatus::NotInitialized);
    REQUIRE(a.reset(42, config) == ResetStatus::Ready);
    REQUIRE(b.reset(42, config) == ResetStatus::Ready);
    auto reverseConfig = config;
    std::swap(reverseConfig.robots[0], reverseConfig.robots[1]);
    REQUIRE(swapped.reset(42, reverseConfig) == ResetStatus::Ready);
    const auto initial = a.state();
    auto missing = config; missing.robotTypes[1] = {};
    REQUIRE(a.reset(0, missing) == ResetStatus::InvalidConfig);
    REQUIRE(a.lastError().find("maxHp") != std::string::npos);
    REQUIRE(a.stepDiscrete({{{0},{18}}}).status == StepStatus::InvalidAction);
    REQUIRE(initial.tick == 0 && initial.seed == 42 && initial.result == MatchResult::Running);
    REQUIRE(initial.width == 1300 && initial.height == 1100 && initial.maxTicks == 3600);
    ActionsV1 invalid{};
    invalid[1].movement.x = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(a.step(invalid).status == StepStatus::InvalidAction && equal(a.state(), initial));
    invalid[1].movement = {1, 1};
    REQUIRE(a.step(invalid).status == StepStatus::InvalidAction && equal(a.state(), initial));
    auto bad = config; bad.tickRateHz = 0;
    REQUIRE(a.reset(99, bad) == ResetStatus::InvalidConfig && equal(a.state(), initial));
    for (std::uint64_t t = 0; t < config.maxTicks; ++t) {
        ActionsV1 actions{};
        if (t < 300) { actions[0].movement = {1, 0}; actions[1].movement = {-1, 0}; }
        else if (t < 600) { actions[0].movement = {0, 1}; actions[1].movement = {0, -1}; }
        actions[0].attack = (t % 2 == 0); // Reserved; no combat effect.
        auto reverse = actions; std::swap(reverse[0], reverse[1]);
        const auto x = a.step(actions), y = b.step(actions), z = swapped.step(reverse);
        REQUIRE(x.status == StepStatus::Advanced && y.status == x.status && z.status == x.status);
        REQUIRE(equal(x.state, y.state) && x.events.size() == y.events.size());
        for (std::size_t i = 0; i < x.events.size(); ++i) REQUIRE(equal(x.events[i], y.events[i]));
        REQUIRE(x.state.tick == t + 1 && x.state.result == z.state.result);
        REQUIRE(equal(x.state.robots[0].position, z.state.robots[1].position));
        REQUIRE(equal(x.state.robots[1].position, z.state.robots[0].position));
        if (t == 0) {
            REQUIRE(equal(x.state.robots[0].position, {-385, 0}));
            REQUIRE(x.events.size() == 2 && x.events[0].kind == EventKind::Moved);
        }
        if (t == 299) REQUIRE(equal(x.state.robots[0].position, {650, 0}));
        if (t == 599) REQUIRE(equal(x.state.robots[0].position, {650, 550}));
        if (t == 600) REQUIRE(x.events.empty());
        if (t + 1 < config.maxTicks) REQUIRE(x.state.result == MatchResult::Running);
        else REQUIRE(x.events.size() == 1 && x.events[0].kind == EventKind::MatchEnded &&
                     x.events[0].result == MatchResult::Draw);
    }
    REQUIRE(a.isTerminal() && a.state().result == MatchResult::Draw);
    const auto terminal = a.state(); const auto after = a.step({});
    REQUIRE(after.status == StepStatus::Terminal && after.events.empty() && equal(after.state, terminal));
    REQUIRE(a.reset(42, config) == ResetStatus::Ready && equal(a.state(), initial));
    config.obstacles = {{7, ObstacleType::Fixed, {0, 0}, {50, 60}},
                        {8, ObstacleType::Movable, {200, 100}, {30, 40}}};
    REQUIRE(a.reset(42, config) == ResetStatus::Ready);
    const auto layout = a.state();
    REQUIRE(layout.obstacles.size() == 2 && layout.obstacles[0].id == 7);
    REQUIRE(equal(a.step({}).state.obstacles[1].center, {200, 100}));
    bad = config; bad.obstacles[1].id = 7;
    REQUIRE(a.reset(0, bad) == ResetStatus::InvalidConfig);
    bad = config; bad.obstacles[1].center = {650, 0};
    REQUIRE(a.reset(0, bad) == ResetStatus::InvalidConfig);
    REQUIRE(a.state().tick == 1);
    std::cout << "World simulation: 3600-tick replay, symmetry, lifecycle and config checks passed\n";
}
