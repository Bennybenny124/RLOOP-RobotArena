#pragma once

#include <array>
#include <cstdint>
#include <vector>

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
    double moveSpeed = 0.0; // cm/s
};

struct ArenaConfig {
    // Provisional test fixture, not final gameplay balance.
    double width = 1300.0;
    double height = 1100.0;
    std::uint32_t tickRateHz = 60;
    std::uint64_t maxTicks = 3600;
    std::array<RobotConfig, 2> robots{{{{-390.0, 0.0}, 300.0},
                                       {{390.0, 0.0}, 300.0}}};
    std::vector<ObstacleStateV1> obstacles;
};

// Movement intent is a vector in the unit disk; #4 supplies the discrete decoder.
// Attack is reserved for #6 and has no effect in the minimal simulation.
struct ActionV1 {
    static constexpr std::uint32_t schemaVersion = 1;
    Vec2 movement;
    bool attack = false;
};
using ActionsV1 = std::array<ActionV1, 2>;

struct RobotStateV1 {
    Vec2 position;
};

enum class MatchResult { NotStarted, Running, Player0Win, Player1Win, Draw };
enum class StepStatus { Advanced, NotInitialized, InvalidAction, Terminal };
enum class ResetStatus { Ready, InvalidConfig };

struct WorldStateV1 {
    static constexpr std::uint32_t schemaVersion = 1;
    std::uint64_t seed = 0;
    std::uint64_t tick = 0;
    std::uint32_t tickRateHz = 0;
    std::uint64_t maxTicks = 0;
    double width = 0.0;
    double height = 0.0;
    std::array<RobotStateV1, 2> robots{};
    std::vector<ObstacleStateV1> obstacles;
    MatchResult result = MatchResult::NotStarted;
};

enum class EventKind { Moved, MatchEnded };
struct EventV1 {
    static constexpr std::uint32_t schemaVersion = 1;
    static constexpr std::uint32_t noPlayer = 2;
    EventKind kind = EventKind::Moved;
    std::uint32_t player = noPlayer;
    Vec2 from;
    Vec2 to;
    MatchResult result = MatchResult::Running;
};

struct StepResultV1 {
    static constexpr std::uint32_t schemaVersion = 1;
    StepStatus status = StepStatus::NotInitialized;
    WorldStateV1 state;
    std::vector<EventV1> events;
};

} // namespace rloop
