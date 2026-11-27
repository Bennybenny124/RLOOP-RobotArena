#include "CoreRobot.h"
#include <algorithm>
#include <limits>

namespace rloop {

void CoreRobot::reset(const RobotConfig& config, const RobotTypeConfig& type) noexcept {
    config_ = type;
    state_ = {config.spawn, config.type, type.maxHp, type.maxHp, type.halfExtent, 0, 0, 0};
}

void CoreRobot::commitPosition(Vec2 position) noexcept {
    state_.position = position;
}

RobotStateV2 CoreRobot::state() const noexcept {
    return state_;
}

void CoreRobot::advanceCooldown() noexcept {
    if (state_.cooldownTicks) --state_.cooldownTicks;
}

void CoreRobot::recordShot(std::uint32_t hz) noexcept {
    state_.cooldownTicks=(static_cast<std::uint64_t>(config_.cooldownMs)*hz+999)/1000;
}

Hp CoreRobot::applyCombat(Hp damage,bool fired,std::uint32_t hz,std::uint32_t delayMs) noexcept {
    state_.hp=std::max<Hp>(0,state_.hp-damage);
    if(damage>0 || fired) {
        state_.quietTicks=0;
        state_.regenRemainder=0;
        return 0;
    }
    if(state_.hp==0) return 0;
    if(state_.quietTicks<std::numeric_limits<std::uint64_t>::max()) ++state_.quietTicks;
    const std::uint64_t threshold=static_cast<std::uint64_t>(delayMs)*hz;
    // Each tick spans 1000 scaled time units. Credit only the portion after
    // the configured delay (including fractional-tick crossings).
    const std::uint64_t thresholdTick=(threshold+999)/1000;
    std::uint64_t eligible=1000;
    if(state_.quietTicks<thresholdTick) eligible=0;
    else if(state_.quietTicks==thresholdTick) eligible=state_.quietTicks*1000-threshold;
    if(state_.hp==state_.maxHp) { state_.regenRemainder=0; return 0; }
    const Hp denominator=static_cast<Hp>(1000)*hz;
    const Hp numerator=config_.regenPerSecond*static_cast<Hp>(eligible)+state_.regenRemainder;
    const Hp heal=std::min(state_.maxHp-state_.hp,numerator/denominator);
    state_.hp+=heal;
    state_.regenRemainder=state_.hp==state_.maxHp ? 0 : numerator%denominator;
    return heal;
}
} // namespace rloop

// Controller encoding lives in the shared Core, never in UE or Python.
namespace rloop {
bool decodeAction(ActionV2 action, DecodedAction& decoded) noexcept {
    if (action.value < 0 || action.value >= 18) return false;
    constexpr double d = 0.7071067811865475244;
    constexpr Vec2 directions[] = {{0,0},{0,1},{d,d},{1,0},{d,-d},
                                    {0,-1},{-d,-d},{-1,0},{-d,d}};
    decoded = {directions[action.value / 2], action.value % 2 != 0};
    return true;
}

ActionV2 encodeAction(int x, int y, bool attack) noexcept {
    x = (x > 0) - (x < 0);
    y = (y > 0) - (y < 0);
    constexpr int ids[3][3] = {{6,7,8},{5,0,1},{4,3,2}};
    return {ids[x+1][y+1]*2 + (attack ? 1 : 0)};
}
} // namespace rloop
