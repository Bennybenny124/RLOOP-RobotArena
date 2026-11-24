#include "CoreRobot.h"

namespace rloop {

void CoreRobot::reset(const RobotConfig& config) noexcept {
    config_ = config;
    state_.position = config.spawn;
}

Vec2 CoreRobot::proposedPosition(Vec2 movement, double dt) const noexcept {
    const double distance = config_.moveSpeed * dt;
    return {state_.position.x + movement.x * distance,
            state_.position.y + movement.y * distance};
}

void CoreRobot::commitPosition(Vec2 position) noexcept {
    state_.position = position;
}

RobotStateV1 CoreRobot::state() const noexcept {
    return state_;
}

} // namespace rloop
