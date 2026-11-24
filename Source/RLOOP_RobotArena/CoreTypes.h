#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <string>

namespace rloop {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

enum class ObstacleType { Fixed, Movable };

struct ObstacleStateV1 {
    std::uint32_t id = 0;
    ObstacleType type = ObstacleType::Fixed;
    Vec2 center;
    Vec2 halfExtent;
};

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
struct RobotConfig {
    Vec2 spawn;
    RobotType type = RobotType::Tank;
};

struct ArenaConfig {
    // Provisional test fixture, not final gameplay balance.
    double width = 1300.0;
    double height = 1100.0;
    std::uint32_t tickRateHz = 60;
    std::uint64_t maxTicks = 3600;
    std::array<RobotTypeConfig, 3> robotTypes{{
        {200*hpScale, 20*hpScale, 500, 450, 300, 5*hpScale, 25},
        {120*hpScale, 35*hpScale, 1800, 300, 420, 15*hpScale, 25},
        {90*hpScale, 15*hpScale, 800, 750, 280, 3*hpScale, 25}
    }};
    std::array<RobotConfig, 2> robots{{{{-390.0, 0.0}, RobotType::Tank},
                                      {{390.0, 0.0}, RobotType::Scout}}};
    std::vector<ObstacleStateV1> obstacles;
    double pushSpeedMultiplier = 0.5;
    double boxSpeedCap = 150.0; // provisional cm/s, independent of robot type
    std::uint32_t outOfCombatDelayMs = 3000; // provisional
};

// Legacy low-level vector intent (v1). Discrete v2 is the controller contract.
struct ActionV1 {
    static constexpr std::uint32_t schemaVersion = 1;
    Vec2 movement;
    bool attack = false;
};
using ActionsV1 = std::array<ActionV1, 2>;

// Discrete movement IDs: stop,N,NE,E,SE,S,SW,W,NW; north = +Y.
struct ActionV2 {
    static constexpr std::uint32_t schemaVersion = 2;
    int value = 0;
};
using ActionsV2 = std::array<ActionV2, 2>;
bool decodeAction(ActionV2 action, ActionV1& decoded) noexcept;
ActionV2 encodeAction(int x, int y, bool attack) noexcept;

struct RobotStateV2 {
    Vec2 position;
    RobotType type = RobotType::Tank;
    Hp hp = 0;
    Hp maxHp = 0;
    double halfExtent = 0;
    std::uint64_t cooldownTicks = 0;
    std::uint64_t quietTicks = 0;
    Hp regenRemainder = 0;
};

enum class MatchResult { NotStarted, Running, Player0Win, Player1Win, Draw };
enum class StepStatus { Advanced, NotInitialized, InvalidAction, Terminal };
enum class ResetStatus { Ready, InvalidConfig };

struct WorldStateV2 {
    static constexpr std::uint32_t schemaVersion = 2;
    std::uint64_t seed = 0;
    std::uint64_t tick = 0;
    std::uint32_t tickRateHz = 0;
    std::uint64_t maxTicks = 0;
    double width = 0.0;
    double height = 0.0;
    std::array<RobotStateV2, 2> robots{};
    std::vector<ObstacleStateV1> obstacles;
    MatchResult result = MatchResult::NotStarted;
};

enum class EventKind { Moved, MatchEnded, ObstacleMoved, Attack, Healed, MovementLimited };
enum class AttackResult { None, OnCooldown, OutOfRange, Blocked, Reduced, Hit };
struct EventV2 {
    static constexpr std::uint32_t schemaVersion = 2;
    static constexpr std::uint32_t noPlayer = 2;
    EventKind kind = EventKind::Moved;
    std::uint32_t player = noPlayer;
    Vec2 from;
    Vec2 to;
    MatchResult result = MatchResult::Running;
    AttackResult resultCode = AttackResult::None;
    Hp actualDamage = 0;
    Hp healed = 0;
    std::uint32_t obstacleId = 0;
};

struct StepResultV2 {
    static constexpr std::uint32_t schemaVersion = 2;
    StepStatus status = StepStatus::NotInitialized;
    WorldStateV2 state;
    std::vector<EventV2> events;
};

} // namespace rloop
