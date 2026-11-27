#pragma once
#include "CoreTypes.h"

namespace rloop {
inline constexpr double contactToleranceCm = 1e-7;

// Candidate geometry: square robot footprint, AABB boxes; no wall sliding.
bool overlaps(Vec2 a, Vec2 ah, Vec2 b, Vec2 bh) noexcept;
Vec2 clampPushVelocity(Vec2 sum, double cap) noexcept;
struct MovementResult {
    std::array<Vec2,2> positions{};
    bool exhausted = false; // conservative event-budget fallback
};
MovementResult resolveMovement(const ArenaConfig& config,
                               const std::array<RobotStateV2,2>& robots,
                               std::vector<ObstacleStateV1>& obstacles,
                               const DecodedActions& actions);
}
