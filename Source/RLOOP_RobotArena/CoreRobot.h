#pragma once

#include "CoreTypes.h"

namespace rloop {

// Individual robot state only; CoreArena commits all player transitions.
class CoreRobot {
public:
    void reset(const RobotConfig& config, const RobotTypeConfig& type) noexcept;
    void commitPosition(Vec2 position) noexcept;
    RobotStateV2 state() const noexcept;
    const RobotTypeConfig& config() const noexcept { return config_; }
    void advanceCooldown() noexcept;
    void recordShot(std::uint32_t tickRateHz) noexcept;
    Hp applyCombat(Hp damage, bool fired, std::uint32_t tickRateHz, std::uint32_t delayMs) noexcept;

private:
    RobotTypeConfig config_{};
    RobotStateV2 state_{};
};

} // namespace rloop
