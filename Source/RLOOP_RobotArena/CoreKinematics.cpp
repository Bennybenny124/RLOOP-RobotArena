#include "CoreKinematics.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace rloop {
namespace {
constexpr double eps = contactToleranceCm;
Vec2 add(Vec2 a, Vec2 b) { return {a.x+b.x,a.y+b.y}; }
Vec2 sub(Vec2 a, Vec2 b) { return {a.x-b.x,a.y-b.y}; }
Vec2 mul(Vec2 a, double k) { return {a.x*k,a.y*k}; }
bool moving(Vec2 a) { return a.x != 0 || a.y != 0; }
enum class BodyKind { Robot, Fixed, Box };
struct Body { Vec2 p,h; BodyKind kind; };
struct Gap { double x,y; };
Gap gap(const Body& a,const Body& b) {
    return {std::abs(b.p.x-a.p.x)-a.h.x-b.h.x,
            std::abs(b.p.y-a.p.y)-a.h.y-b.h.y};
}
bool touching(const Body& a,const Body& b) {
    const auto g=gap(a,b);
    return g.x<=eps && g.y<=eps;
}
double sign(double x) { return x>=0 ? 1.0 : -1.0; }
// True only when the two bodies would penetrate if their velocities continued.
// A corner requires approach on both boundary axes; tangential contact is safe.
bool closing(const Body& a,const Body& b,Vec2 av,Vec2 bv) {
    if (!touching(a,b)) return false;
    const auto g=gap(a,b); const auto d=sub(b.p,a.p); const auto v=sub(av,bv);
    return (g.x < -eps || v.x*sign(d.x)>0) &&
           (g.y < -eps || v.y*sign(d.y)>0);
}
// Remove only velocity components directed through an arena wall.
Vec2 projectWallVelocity(const Body& b,Vec2 v,Vec2 half) {
    if ((v.x>0 && b.p.x+b.h.x>=half.x-eps) ||
        (v.x<0 && b.p.x-b.h.x<=-half.x+eps)) v.x=0;
    if ((v.y>0 && b.p.y+b.h.y>=half.y-eps) ||
        (v.y<0 && b.p.y-b.h.y<=-half.y+eps)) v.y=0;
    return v;
}
bool wallClosing(const Body& b,Vec2 v,Vec2 half) {
    const auto projected=projectWallVelocity(b,v,half);
    return projected.x!=v.x || projected.y!=v.y;
}
// A static contact blocks inward boundary axes, never tangential ones.
// For an exact corner with inward motion on both axes, block both
// symmetrically; no arbitrary axis or obstacle-ID preference.
Vec2 projectFixedVelocity(const Body& body,const std::vector<Body>& bodies,Vec2 v) {
    Vec2 projected=v;
    for(const auto& fixed:bodies) {
        if(fixed.kind!=BodyKind::Fixed || !closing(body,fixed,v,{})) continue;
        const auto g=gap(body,fixed);
        if(g.x>=-eps) projected.x=0;
        if(g.y>=-eps) projected.y=0;
    }
    return projected;
}
// Combine independent restrictions on the same original velocity without
// changing its sign or allowing obstacle traversal order to affect results.
Vec2 restrictVelocity(Vec2 a,Vec2 b) {
    return {std::abs(a.x)<=std::abs(b.x)?a.x:b.x,
            std::abs(a.y)<=std::abs(b.y)?a.y:b.y};
}
double wallTime(const Body& b,Vec2 v,Vec2 half,double t) {
    if (v.x>0) t=std::min(t,(half.x-b.h.x-b.p.x)/v.x);
    if (v.x<0) t=std::min(t,(-half.x+b.h.x-b.p.x)/v.x);
    if (v.y>0) t=std::min(t,(half.y-b.h.y-b.p.y)/v.y);
    if (v.y<0) t=std::min(t,(-half.y+b.h.y-b.p.y)/v.y);
    return t;
}
// Relative swept AABB. Strict interior overlap, so grazing a corner is safe.
double impact(const Body& a,const Body& b,Vec2 av,Vec2 bv,double limit) {
    if (touching(a,b)) return limit; // contact constraints are already solved
    const auto p=sub(a.p,b.p), v=sub(av,bv), h=add(a.h,b.h);
    double enter=0, exit=limit;
    auto slab=[&](double pos,double vel,double extent) {
        if (vel==0) return std::abs(pos)<extent;
        double lo=(-extent-pos)/vel, hi=(extent-pos)/vel;
        if (lo>hi) std::swap(lo,hi);
        enter=std::max(enter,lo); exit=std::min(exit,hi);
        return enter<exit;
    };
    if (!slab(p.x,v.x,h.x) || !slab(p.y,v.y,h.y)) return limit;
    return enter>=0 ? enter : limit;
}
std::size_t root(const std::vector<std::size_t>& parent,std::size_t i) {
    while(parent[i]!=i) i=parent[i];
    return i;
}
void unite(std::vector<std::size_t>& p,std::size_t a,std::size_t b) {
    a=root(p,a); b=root(p,b); if(a!=b) p[std::max(a,b)]=std::min(a,b);
}
// Restrict the closing relative normal speed symmetrically, without
// adding velocity to either robot. Opposing inward components become zero;
// for same-direction travel, only the faster follower is limited.
// Tangential components and separating motion are left untouched.
void constrainRobots(const Body& a,const Body& b,Vec2 av,Vec2 bv,
                     Vec2& aLimit,Vec2& bLimit) {
    const auto g=gap(a,b); const auto d=sub(b.p,a.p);
    auto axis=[&](double gapAxis,double dir,double va,double vb,double& la,double& lb) {
        if(gapAxis < -eps) return;
        const double normal=sign(dir);
        va*=normal; vb*=normal;
        if(va<=vb) return;
        const double nextA=std::min(va,std::max(0.0,vb))*normal;
        const double nextB=std::max(vb,std::min(0.0,va))*normal;
        if(std::abs(nextA)<std::abs(la)) la=nextA;
        if(std::abs(nextB)<std::abs(lb)) lb=nextB;
    };
    axis(g.x,d.x,av.x,bv.x,aLimit.x,bLimit.x);
    axis(g.y,d.y,av.y,bv.y,aLimit.y,bLimit.y);
}
// A robot/robot face contact can end while the robots slide past each
// other. Recompute desired velocity at that instant instead of retaining
// the blocked normal until the next tick. At a corner, closing() already
// permits tangential/separating motion, so do not create a zero-time event.
double robotContactRelease(const Body& a,const Body& b,Vec2 av,Vec2 bv,double limit) {
    if(!touching(a,b)) return limit;
    const auto g=gap(a,b);
    const auto p=sub(b.p,a.p), v=sub(bv,av), h=add(a.h,b.h);
    auto exit=[&](double pos,double speed,double extent) {
        if(speed==0) return;
        const double t=(sign(speed)*extent-pos)/speed;
        if(t>0 && t<limit) limit=t;
    };
    if(g.x>=-eps && g.y < -eps && v.x==0) exit(p.y,v.y,h.y);
    if(g.y>=-eps && g.x < -eps && v.y==0) exit(p.x,v.x,h.x);
    return limit;
}
// Follow the box only as far as each inward relative velocity permits.
// A blocked normal must not cancel the robot's independent tangential
// motion. A box moving into a non-following robot still stops its group.
void constrainRobotBox(const Body& robot,const Body& box,Vec2 rv,Vec2 bv,
                       Vec2& limit,bool& stopBox) {
    const auto g=gap(robot,box); const auto d=sub(box.p,robot.p);
    auto axis=[&](double gapAxis,double dir,double r,double b,double& allowed) {
        if(gapAxis < -eps) return;
        const double normal=sign(dir);
        r*=normal; b*=normal;
        if(b<0) stopBox=true;
        else if(r>b && r>0) {
            const double candidate=b*normal;
            if(std::abs(candidate)<std::abs(allowed)) allowed=candidate;
        }
    };
    axis(g.x,d.x,rv.x,bv.x,limit.x); axis(g.y,d.y,rv.y,bv.y,limit.y);
}
}
bool overlaps(Vec2 a,Vec2 ah,Vec2 b,Vec2 bh) noexcept {
    return std::abs(a.x-b.x)<ah.x+bh.x-eps && std::abs(a.y-b.y)<ah.y+bh.y-eps;
}
Vec2 clampPushVelocity(Vec2 sum,double cap) noexcept {
    const double length=std::hypot(sum.x,sum.y);
    return length>cap ? mul(sum,cap/length) : sum;
}
MovementResult resolveMovement(const ArenaConfig& config,
                               const std::array<RobotStateV2,2>& robots,
                               std::vector<ObstacleStateV1>& obstacles,
                               const DecodedActions& actions) {
    std::vector<Body> bodies;
    std::array<Vec2,2> desired{};
    for(std::size_t i=0;i<2;++i) {
        bodies.push_back({robots[i].position,{robots[i].halfExtent,robots[i].halfExtent},BodyKind::Robot});
        desired[i]=mul(actions[i].movement,config.robotTypes[static_cast<std::size_t>(robots[i].type)].moveSpeed);
    }
    for(const auto& o:obstacles) bodies.push_back({o.center,o.halfExtent,
        o.type==ObstacleType::Fixed ? BodyKind::Fixed : BodyKind::Box});
    const std::size_t n=bodies.size();
    const Vec2 half{config.map.width/2,config.map.height/2};
    double remaining=1.0/static_cast<double>(config.tickRateHz);
    const std::size_t budget=16+8*n;
    std::size_t iteration=0;
    bool limited=false;
    for(;remaining>0 && iteration<budget;++iteration) {
        std::vector<std::size_t> groups(n);
        std::iota(groups.begin(),groups.end(),std::size_t{0});
        for(std::size_t i=2;i<n;++i) if(bodies[i].kind==BodyKind::Box)
            for(std::size_t j=i+1;j<n;++j)
                if(bodies[j].kind==BodyKind::Box && touching(bodies[i],bodies[j])) unite(groups,i,j);
        for(std::size_t i=0;i<n;++i) groups[i]=root(groups,i);
        // Reproject at every swept contact, including walls reached mid-tick.
        // Keep the original diagonal component; do not renormalize.
        std::array<Vec2,2> intent{};
        for(std::size_t r=0;r<2;++r)
            intent[r]=projectFixedVelocity(bodies[r],bodies,projectWallVelocity(bodies[r],desired[r],half));
        std::vector<std::array<bool,2>> pushes(n);
        std::array<bool,2> pushing{};
        for(std::size_t i=2;i<n;++i) if(bodies[i].kind==BodyKind::Box)
            for(std::size_t r=0;r<2;++r) if(closing(bodies[r],bodies[i],intent[r],{})) {
                pushes[groups[i]][r]=true; pushing[r]=true;
            }
        std::vector<Vec2> vel(n), groupVel(n);
        for(std::size_t r=0;r<2;++r)
            vel[r]=mul(intent[r],pushing[r]?config.pushSpeedMultiplier:1.0);
        for(std::size_t i=2;i<n;++i) if(groups[i]==i && bodies[i].kind==BodyKind::Box) {
            for(std::size_t r=0;r<2;++r) if(pushes[i][r]) groupVel[i]=add(groupVel[i],vel[r]);
            groupVel[i]=clampPushVelocity(groupVel[i],config.boxSpeedCap);
        }
        // A push group must translate as one rigid collection of AABBs.
        // Project the summed, norm-clamped velocity against every member's
        // static contacts. Union all blocked axes, not whole-velocity stops.
        // Separate wall and fixed passes so every member sees the same
        // wall-projected group velocity regardless of obstacle order.
        auto wallProjected=groupVel;
        for(std::size_t i=2;i<n;++i) if(bodies[i].kind==BodyKind::Box) {
            const auto g=groups[i];
            wallProjected[g]=restrictVelocity(wallProjected[g],projectWallVelocity(bodies[i],groupVel[g],half));
        }
        auto staticProjected=wallProjected;
        for(std::size_t i=2;i<n;++i) if(bodies[i].kind==BodyKind::Box) {
            const auto g=groups[i];
            staticProjected[g]=restrictVelocity(staticProjected[g],projectFixedVelocity(bodies[i],bodies,wallProjected[g]));
        }
        for(std::size_t i=2;i<n;++i) if(bodies[i].kind==BodyKind::Box) vel[i]=staticProjected[groups[i]];
        // Static constraints are solved before dynamic contacts so the
        // robot can follow a box's projected tangential velocity. The
        // remaining synchronous passes only reduce components.
        bool resolved=false;
        for(std::size_t pass=0;pass<4*n+8;++pass) {
            std::vector<bool> stop(n,false);
            std::array<Vec2,2> limit{vel[0],vel[1]};
            for(std::size_t i=0;i<n;++i) for(std::size_t j=i+1;j<n;++j) {
                if(groups[i]==groups[j] || !closing(bodies[i],bodies[j],vel[i],vel[j])) continue;
                if(bodies[i].kind==BodyKind::Robot && bodies[j].kind==BodyKind::Box) {
                    bool halt=false;
                    constrainRobotBox(bodies[i],bodies[j],vel[i],vel[j],limit[i],halt);
                    if(halt) stop[groups[j]]=true;
                } else if(bodies[i].kind==BodyKind::Robot && bodies[j].kind==BodyKind::Robot) {
                    constrainRobots(bodies[i],bodies[j],vel[i],vel[j],limit[i],limit[j]);
                } else if(bodies[i].kind!=BodyKind::Fixed && bodies[j].kind!=BodyKind::Fixed) {
                    stop[groups[i]]=true;
                    stop[groups[j]]=true;
                }
            }
            bool changed=false;
            for(std::size_t i=0;i<n;++i) {
                const Vec2 old=vel[i];
                if(stop[groups[i]]) vel[i]={};
                else if(i<2) vel[i]=limit[i];
                changed=changed || old.x!=vel[i].x || old.y!=vel[i].y;
            }
            if(!changed) {
                resolved=true;
                // No unresolved inward contact may advance.
                for(std::size_t i=0;i<n;++i) {
                    if(wallClosing(bodies[i],vel[i],half)) resolved=false;
                    for(std::size_t j=i+1;j<n;++j)
                        if(groups[i]!=groups[j] && closing(bodies[i],bodies[j],vel[i],vel[j])) resolved=false;
                }
                break;
            }
        }
        if(!resolved) { limited=true; break; }
        if(std::none_of(vel.begin(),vel.end(),moving)) { remaining=0; break; }
        double t=robotContactRelease(bodies[0],bodies[1],vel[0],vel[1],remaining);
        for(std::size_t i=0;i<n;++i) {
            t=wallTime(bodies[i],vel[i],half,t);
            for(std::size_t j=i+1;j<n;++j)
                if(groups[i]!=groups[j]) t=impact(bodies[i],bodies[j],vel[i],vel[j],t);
        }
        if(!(t>0)) break;
        for(std::size_t i=0;i<n;++i) bodies[i].p=add(bodies[i].p,mul(vel[i],t));
        remaining-=t;
    }
    for(std::size_t i=2;i<n;++i) obstacles[i-2].center=bodies[i].p;
    return {{bodies[0].p,bodies[1].p},limited || remaining>0};
}
}
