#include "CoreCombat.h"
#include <algorithm>
#include <cmath>

namespace rloop {
namespace {
// Closed segment/AABB slabs: boundary contact counts as cover. Returns entry t.
bool segmentBox(Vec2 p,Vec2 q,const ObstacleStateV1& box,double& entry) {
    double lo=0,hi=1;
    auto slab=[&](double start,double end,double center,double half) {
        const double delta=end-start;
        if(delta==0) return start>=center-half && start<=center+half;
        double a=(center-half-start)/delta,b=(center+half-start)/delta;
        if(a>b) std::swap(a,b);
        lo=std::max(lo,a); hi=std::min(hi,b);
        return lo<=hi;
    };
    if(!slab(p.x,q.x,box.center.x,box.halfExtent.x) ||
       !slab(p.y,q.y,box.center.y,box.halfExtent.y)) return false;
    entry=lo; return true;
}
Vec2 lerp(Vec2 a,Vec2 b,double t) { return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t}; }
}
AttackDecision evaluateAttack(const RobotStateV2& shooter,const RobotTypeConfig& config,
                              const RobotStateV2& target,const std::vector<ObstacleStateV1>& obstacles,
                              bool requested) noexcept {
    AttackDecision result;
    result.from=shooter.position; result.to=shooter.position;
    if(!requested) return result;
    if(shooter.cooldownTicks>0) { result.code=AttackResult::OnCooldown; return result; }
    result.fired=true; result.to=target.position;
    const double distance=std::hypot(target.position.x-shooter.position.x,target.position.y-shooter.position.y);
    if(distance>config.attackRange) {
        result.code=AttackResult::OutOfRange;
        result.to=lerp(result.from,result.to,config.attackRange/distance);
        return result;
    }
    double nearest=1; bool blocked=false; unsigned movable=0;
    for(const auto& obstacle:obstacles) {
        double entry=0;
        if(!segmentBox(result.from,result.to,obstacle,entry)) continue;
        if(obstacle.type==ObstacleType::Fixed) { nearest=std::min(nearest,entry); blocked=true; }
        else ++movable;
    }
    if(blocked) {
        result.code=AttackResult::Blocked;
        result.to=lerp(result.from,result.to,nearest);
        return result;
    }
    result.code=movable ? AttackResult::Reduced : AttackResult::Hit;
    Hp damage=config.attackDamage;
    for(unsigned i=0;i<movable && damage>0;++i) damage/=4;
    result.damage=std::min(damage,target.hp);
    return result;
}
MatchResult resolveMatch(const std::array<RobotStateV2,2>& robots,bool timeout) noexcept {
    const auto& a=robots[0]; const auto& b=robots[1];
    if(a.hp==0 && b.hp==0) return MatchResult::Draw;
    if(a.hp==0) return MatchResult::Player1Win;
    if(b.hp==0) return MatchResult::Player0Win;
    if(!timeout) return MatchResult::Running;
    // Validated HP <= 10^9 units, so products <= 10^18 fit exactly in int64.
    const Hp left=a.hp*b.maxHp,right=b.hp*a.maxHp;
    return left==right ? MatchResult::Draw : (left>right ? MatchResult::Player0Win : MatchResult::Player1Win);
}
}
