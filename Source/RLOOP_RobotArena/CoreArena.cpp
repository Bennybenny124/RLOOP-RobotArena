#include "CoreArena.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace rloop {
namespace {

bool finiteVec(Vec2 v) noexcept {
    return std::isfinite(v.x) && std::isfinite(v.y);
}

bool withinArena(Vec2 p, double halfWidth, double halfHeight) noexcept {
    return finiteVec(p) && std::abs(p.x) <= halfWidth && std::abs(p.y) <= halfHeight;
}

bool validConfig(const ArenaConfig& config) noexcept {
    if (!std::isfinite(config.width) || config.width <= 0.0 ||
        !std::isfinite(config.height) || config.height <= 0.0 ||
        config.tickRateHz == 0 || config.maxTicks == 0) {
        return false;
    }
    const double hw = config.width / 2.0;
    const double hh = config.height / 2.0;
    for (const auto& robot : config.robots) {
        if (!withinArena(robot.spawn, hw, hh) ||
            !std::isfinite(robot.moveSpeed) || robot.moveSpeed < 0.0) {
            return false;
        }
    }
    for (std::size_t i = 0; i < config.obstacles.size(); ++i) {
        const auto& o = config.obstacles[i];
        if ((o.type != ObstacleType::Fixed && o.type != ObstacleType::Movable) ||
            !finiteVec(o.halfExtent) || o.halfExtent.x <= 0.0 || o.halfExtent.y <= 0.0 ||
            !withinArena(o.center, hw, hh) ||
            o.halfExtent.x > hw - std::abs(o.center.x) ||
            o.halfExtent.y > hh - std::abs(o.center.y)) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (config.obstacles[j].id == o.id) return false;
        }
    }
    return true;
}

bool validAction(const ActionV1& action) noexcept {
    return finiteVec(action.movement) &&
           std::hypot(action.movement.x, action.movement.y) <= 1.0;
}

} // namespace

ResetStatus CoreArena::reset(std::uint64_t seed, const ArenaConfig& config) {
    if (!validConfig(config)) return ResetStatus::InvalidConfig;
    config_ = config;
    seed_ = seed; // Reserved for future seeded layout generation; no random draws in v1.
    tick_ = 0;
    result_ = MatchResult::Running;
    for (std::size_t i = 0; i < robots_.size(); ++i) robots_[i].reset(config_.robots[i]);
    return ResetStatus::Ready;
}

WorldStateV1 CoreArena::state() const {
    WorldStateV1 snapshot;
    if (result_ == MatchResult::NotStarted) return snapshot;
    snapshot.seed = seed_;
    snapshot.tick = tick_;
    snapshot.tickRateHz = config_.tickRateHz;
    snapshot.maxTicks = config_.maxTicks;
    snapshot.width = config_.width;
    snapshot.height = config_.height;
    snapshot.obstacles = config_.obstacles;
    snapshot.result = result_;
    for (std::size_t i = 0; i < robots_.size(); ++i) snapshot.robots[i] = robots_[i].state();
    return snapshot;
}

bool CoreArena::isTerminal() const noexcept {
    return result_ == MatchResult::Draw || result_ == MatchResult::Player0Win ||
           result_ == MatchResult::Player1Win;
}

StepResultV1 CoreArena::step(const ActionsV1& actions) {
    if (result_ == MatchResult::NotStarted) return {StepStatus::NotInitialized, state(), {}};
    if (isTerminal()) return {StepStatus::Terminal, state(), {}};
    if (!validAction(actions[0]) || !validAction(actions[1])) {
        return {StepStatus::InvalidAction, state(), {}};
    }

    const double dt = 1.0 / static_cast<double>(config_.tickRateHz);
    const double hw = config_.width / 2.0;
    const double hh = config_.height / 2.0;
    std::array<Vec2, 2> before{}, next{};
    // Phase 1: derive both intents from the same pre-tick state. For now only
    // point-vs-arena bounds apply; robot/obstacle collision is #5.
    for (std::size_t i = 0; i < robots_.size(); ++i) {
        before[i] = robots_[i].state().position;
        const Vec2 p = robots_[i].proposedPosition(actions[i].movement, dt);
        next[i] = {std::clamp(p.x, -hw, hw), std::clamp(p.y, -hh, hh)};
    }

    // Phase 2: commit both movement results before any future attack evaluation.
    for (std::size_t i = 0; i < robots_.size(); ++i) robots_[i].commitPosition(next[i]);

    // Phase 3 (#6): evaluate both attacks against the shared post-movement
    // state, then apply damage and recovery simultaneously. No combat yet.
    std::vector<EventV1> events;
    for (std::size_t i = 0; i < robots_.size(); ++i) {
        if (before[i].x != next[i].x || before[i].y != next[i].y) {
            events.push_back({EventKind::Moved, static_cast<std::uint32_t>(i),
                              before[i], next[i], MatchResult::Running});
        }
    }
    ++tick_;
    if (tick_ == config_.maxTicks) {
        result_ = MatchResult::Draw; // Temporary no-combat timeout; #6 compares HP ratios.
        events.push_back({EventKind::MatchEnded, EventV1::noPlayer, {}, {}, result_});
    }
    return {StepStatus::Advanced, state(), std::move(events)};
}

} // namespace rloop
