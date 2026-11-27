#pragma once
#include "CoreTypes.h"
namespace rloop {
struct AttackDecision {
    AttackResult code = AttackResult::None;
    bool fired = false;
    Hp damage = 0;
    Vec2 from;
    Vec2 to;
};
AttackDecision evaluateAttack(const RobotStateV2& shooter,const RobotTypeConfig& config,
                              const RobotStateV2& target,const std::vector<ObstacleStateV1>& obstacles,
                              bool requested) noexcept;
MatchResult resolveMatch(const std::array<RobotStateV2,2>& robots,bool timeout) noexcept;
}
