#pragma once

#include "CoreTypes.h"

namespace rloop {

// Individual robot state only; CoreArena commits all player transitions.
class CoreRobot {
public:
    void reset(const RobotConfig& config, const RobotTypeConfig& type) noexcept;
    Vec2 proposedPosition(Vec2 movement, double dt) const noexcept;
    void commitPosition(Vec2 position) noexcept;
    RobotStateV2 state() const noexcept;
    const RobotTypeConfig& config() const noexcept { return config_; }

private:
    RobotTypeConfig config_{};
    RobotStateV2 state_{};
};

} // namespace rloop
