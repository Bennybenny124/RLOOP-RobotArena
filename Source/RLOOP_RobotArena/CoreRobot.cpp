#include "CoreRobot.h"

namespace rloop {

void CoreRobot::reset(const RobotConfig& config, const RobotTypeConfig& type) noexcept {
    config_ = type;
    state_ = {config.spawn, config.type, type.maxHp, type.maxHp, type.halfExtent, 0, 0, 0};
}

Vec2 CoreRobot::proposedPosition(Vec2 movement, double dt) const noexcept {
    const double distance = config_.moveSpeed * dt;
    return {state_.position.x + movement.x * distance,
            state_.position.y + movement.y * distance};
}

void CoreRobot::commitPosition(Vec2 position) noexcept {
    state_.position = position;
}

RobotStateV2 CoreRobot::state() const noexcept {
    return state_;
}

} // namespace rloop

// Controller encoding lives in the shared Core, never in UE or Python.
namespace rloop {
bool decodeAction(ActionV2 action, ActionV1& decoded) noexcept {
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
