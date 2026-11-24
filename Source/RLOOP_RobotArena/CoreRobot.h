#pragma once

#include "CoreTypes.h"

namespace rloop {

// Individual robot state only; CoreArena commits all player transitions.
class CoreRobot {
public:
    void reset(const RobotConfig& config) noexcept;
    Vec2 proposedPosition(Vec2 movement, double dt) const noexcept;
    void commitPosition(Vec2 position) noexcept;
    RobotStateV1 state() const noexcept;

private:
    RobotConfig config_{};
    RobotStateV1 state_{};
};

} // namespace rloop
