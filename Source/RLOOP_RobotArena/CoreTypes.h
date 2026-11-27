#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <string>
#include "CoreRobotConfig.h"

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

struct RobotConfig {
    Vec2 spawn;
    RobotType type = RobotType::Tank;
};

// Reusable layout value. Each CoreArena copies it at reset; live positions
// never write back to the selected layout or other simulation instances.
struct MapConfig {
    double width = 1300.0;
    double height = 1100.0;
    std::array<RobotConfig, 2> robots{{{{-390.0, 0.0}, RobotType::Tank},
                                      {{390.0, 0.0}, RobotType::Scout}}};
    std::vector<ObstacleStateV1> obstacles;
};

struct ArenaConfig {
    MapConfig map;
    std::uint32_t tickRateHz = 60;
    std::uint64_t maxTicks = 3600;
    std::array<RobotTypeConfig, 3> robotTypes = defaultRobotTypes();
    double pushSpeedMultiplier = 0.5;
    double boxSpeedCap = 150.0; // provisional cm/s, independent of robot type
    std::uint32_t outOfCombatDelayMs = 3000; // provisional
};

// Internal movement intent produced by the discrete action decoder.
struct DecodedAction {
    Vec2 movement;
    bool attack = false;
};
using DecodedActions = std::array<DecodedAction, 2>;

// Discrete movement IDs: stop,N,NE,E,SE,S,SW,W,NW; north = +Y.
struct ActionV2 {
    static constexpr std::uint32_t schemaVersion = 2;
    int value = 0;
};
using ActionsV2 = std::array<ActionV2, 2>;
bool decodeAction(ActionV2 action, DecodedAction& decoded) noexcept;
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
