#include "CoreArena.h"
#include "CoreKinematics.h"
#include "CoreCombat.h"

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

std::string configError(const ArenaConfig& config) {
    if (!std::isfinite(config.width) || config.width <= 0.0 ||
        !std::isfinite(config.height) || config.height <= 0.0 ||
        config.tickRateHz == 0 || config.tickRateHz > 1000 || config.maxTicks == 0) {
        return "arena dimensions/tickRateHz/maxTicks invalid";
    }
    for (std::size_t i = 0; i < config.robotTypes.size(); ++i) {
        const auto& t = config.robotTypes[i];
        const std::string prefix = "robotTypes[" + std::to_string(i) + "].";
        if (t.maxHp <= 0 || t.maxHp > maxHpUnits) return prefix + "maxHp invalid or missing";
        if (t.attackDamage <= 0 || t.attackDamage > maxHpUnits) return prefix + "attackDamage invalid or missing";
        if (t.cooldownMs == 0) return prefix + "cooldownMs invalid or missing";
        if (!std::isfinite(t.attackRange) || t.attackRange <= 0) return prefix + "attackRange invalid or missing";
        if (!std::isfinite(t.moveSpeed) || t.moveSpeed < 0 || t.moveSpeed > 1e7) return prefix + "moveSpeed invalid or missing";
        if (t.regenPerSecond < 0 || t.regenPerSecond > maxHpUnits) return prefix + "regenPerSecond invalid or missing";
        if (!std::isfinite(t.halfExtent) || t.halfExtent <= 0) return prefix + "halfExtent invalid or missing";
    }
    if (config.width > 1e7 || config.height > 1e7 || config.obstacles.size() > 256 ||
        !std::isfinite(config.pushSpeedMultiplier) || config.pushSpeedMultiplier <= 0 || config.pushSpeedMultiplier > 1 ||
        !std::isfinite(config.boxSpeedCap) || config.boxSpeedCap <= 0 || config.boxSpeedCap > 1e7)
        return "arena/pushSpeedMultiplier/boxSpeedCap/obstacle count invalid";
    const double hw = config.width / 2.0;
    const double hh = config.height / 2.0;
    for (const auto& robot : config.robots) {
        const auto index = static_cast<std::size_t>(robot.type);
        if (index >= config.robotTypes.size()) return "robot type invalid";
        const double h = config.robotTypes[index].halfExtent;
        if (h > hw || h > hh || !withinArena(robot.spawn, hw-h, hh-h)) return "robot spawn footprint outside arena";
    }
    for (std::size_t i = 0; i < config.obstacles.size(); ++i) {
        const auto& o = config.obstacles[i];
        if ((o.type != ObstacleType::Fixed && o.type != ObstacleType::Movable) ||
            !finiteVec(o.halfExtent) || o.halfExtent.x <= 0.0 || o.halfExtent.y <= 0.0 ||
            !withinArena(o.center, hw, hh) ||
            o.halfExtent.x > hw - std::abs(o.center.x) ||
            o.halfExtent.y > hh - std::abs(o.center.y)) {
            return "obstacle bounds/type invalid";
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (config.obstacles[j].id == o.id) return "duplicate obstacle ID";
            if (overlaps(o.center,o.halfExtent,config.obstacles[j].center,config.obstacles[j].halfExtent))
                return "overlapping obstacles";
        }
    }
    const auto& a=config.robots[0]; const auto& b=config.robots[1];
    const double ah=config.robotTypes[static_cast<std::size_t>(a.type)].halfExtent;
    const double bh=config.robotTypes[static_cast<std::size_t>(b.type)].halfExtent;
    if (overlaps(a.spawn,{ah,ah},b.spawn,{bh,bh})) return "overlapping robot spawns";
    for (const auto& robot:config.robots) {
        const double h=config.robotTypes[static_cast<std::size_t>(robot.type)].halfExtent;
        for (const auto& o:config.obstacles)
            if (overlaps(robot.spawn,{h,h},o.center,o.halfExtent)) return "robot spawn overlaps obstacle";
    }
    return {};
}

} // namespace

ResetStatus CoreArena::reset(std::uint64_t seed, const ArenaConfig& config) {
    lastError_ = configError(config);
    if (!lastError_.empty()) return ResetStatus::InvalidConfig;
    config_ = config;
    obstacles_ = config.obstacles;
    seed_ = seed; // Reserved for future seeded layout generation; no random draws in v1.
    tick_ = 0;
    result_ = MatchResult::Running;
    for (std::size_t i = 0; i < robots_.size(); ++i) robots_[i].reset(config_.robots[i], config_.robotTypes[static_cast<std::size_t>(config_.robots[i].type)]);
    return ResetStatus::Ready;
}

WorldStateV2 CoreArena::state() const {
    WorldStateV2 snapshot;
    if (result_ == MatchResult::NotStarted) return snapshot;
    snapshot.seed = seed_;
    snapshot.tick = tick_;
    snapshot.tickRateHz = config_.tickRateHz;
    snapshot.maxTicks = config_.maxTicks;
    snapshot.width = config_.width;
    snapshot.height = config_.height;
    snapshot.obstacles = obstacles_;
    snapshot.result = result_;
    for (std::size_t i = 0; i < robots_.size(); ++i) snapshot.robots[i] = robots_[i].state();
    return snapshot;
}

bool CoreArena::isTerminal() const noexcept {
    return result_ == MatchResult::Draw || result_ == MatchResult::Player0Win ||
           result_ == MatchResult::Player1Win;
}

StepResultV2 CoreArena::step(const ActionsV2& discrete) {
    if (result_ == MatchResult::NotStarted) return {StepStatus::NotInitialized, state(), {}};
    if (isTerminal()) return {StepStatus::Terminal, state(), {}};
    DecodedActions actions{};
    if (!decodeAction(discrete[0], actions[0]) || !decodeAction(discrete[1], actions[1]))
        return {StepStatus::InvalidAction, state(), {}};

    const std::array<RobotStateV2,2> previous{robots_[0].state(),robots_[1].state()};
    const std::array<Vec2,2> before{previous[0].position,previous[1].position};
    const auto beforeObstacles = obstacles_;
    const auto movement = resolveMovement(config_, previous, obstacles_, actions);
    const auto& next = movement.positions;
    for (std::size_t i = 0; i < robots_.size(); ++i) robots_[i].commitPosition(next[i]);

    std::vector<EventV2> events;
    for (std::size_t i=0;i<robots_.size();++i) {
        if(before[i].x!=next[i].x || before[i].y!=next[i].y)
            events.push_back({EventKind::Moved,static_cast<std::uint32_t>(i),before[i],next[i]});
    }
    for(std::size_t i=0;i<obstacles_.size();++i) {
        const auto a=beforeObstacles[i].center,b=obstacles_[i].center;
        if(a.x!=b.x || a.y!=b.y) {
            EventV2 e; e.kind=EventKind::ObstacleMoved; e.from=a; e.to=b; e.obstacleId=obstacles_[i].id;
            events.push_back(e);
        }
    }
    if(movement.exhausted) { EventV2 e; e.kind=EventKind::MovementLimited; events.push_back(e); }

    // Cooldowns elapse at the tick boundary; take one post-movement snapshot.
    for(auto& robot:robots_) robot.advanceCooldown();
    const std::array<RobotStateV2,2> post{robots_[0].state(),robots_[1].state()};
    std::array<AttackDecision,2> attacks{};
    for(std::size_t i=0;i<2;++i)
        attacks[i]=evaluateAttack(post[i],robots_[i].config(),post[1-i],obstacles_,actions[i].attack);
    // Both attack decisions precede any damage/cooldown/regen mutation.
    for(std::size_t i=0;i<2;++i) {
        const auto& shot=attacks[i];
        if(shot.fired) robots_[i].recordShot(config_.tickRateHz);
        if(shot.code!=AttackResult::None) {
            EventV2 e; e.kind=EventKind::Attack; e.player=static_cast<std::uint32_t>(i);
            e.from=shot.from; e.to=shot.to; e.resultCode=shot.code; e.actualDamage=shot.damage;
            events.push_back(e);
        }
    }
    for(std::size_t i=0;i<2;++i) {
        const Hp healed=robots_[i].applyCombat(attacks[1-i].damage,attacks[i].fired,
                                               config_.tickRateHz,config_.outOfCombatDelayMs);
        if(healed>0) {
            EventV2 e; e.kind=EventKind::Healed; e.player=static_cast<std::uint32_t>(i);
            e.from=next[i]; e.to=next[i]; e.healed=healed; events.push_back(e);
        }
    }
    ++tick_;
    result_=resolveMatch({robots_[0].state(),robots_[1].state()},tick_==config_.maxTicks);
    if(isTerminal()) events.push_back({EventKind::MatchEnded,EventV2::noPlayer,{},{},result_});
    return {StepStatus::Advanced,state(),std::move(events)};
}

} // namespace rloop
