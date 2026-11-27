#pragma once
#include <array>
#include <cstdint>

namespace rloop {
// HP is stored as integer millionths of one HP; bounded so ratio products
// fit in int64. Role values below are provisional, editable test fixtures.
using Hp = std::int64_t;
inline constexpr Hp hpScale = 1000000;
inline constexpr Hp maxHpUnits = 1000000000;
enum class RobotType : std::uint8_t { Tank, Scout, Ranged };
struct RobotTypeConfig {
    Hp maxHp = 0;
    Hp attackDamage = 0;
    std::uint32_t cooldownMs = 0;
    double attackRange = 0.0; // cm
    double moveSpeed = -1.0; // cm/s
    Hp regenPerSecond = -1;
    double halfExtent = 0.0; // square collision footprint, cm
};

// Single source for provisional archetype defaults; editor assets may override.
inline constexpr std::array<RobotTypeConfig,3> defaultRobotTypes() noexcept {
    return {{
        {200*hpScale, 20*hpScale, 500, 450, 300, 5*hpScale, 25},
        {120*hpScale, 35*hpScale, 1800, 300, 420, 15*hpScale, 25},
        {90*hpScale, 15*hpScale, 800, 750, 280, 3*hpScale, 25}
    }};
}
} // namespace rloop
