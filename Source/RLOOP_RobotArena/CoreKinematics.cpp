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
bool wallClosing(const Body& b,Vec2 v,Vec2 half) {
    return (v.x>0 && b.p.x+b.h.x>=half.x-eps) ||
           (v.x<0 && b.p.x-b.h.x<=-half.x+eps) ||
           (v.y>0 && b.p.y+b.h.y>=half.y-eps) ||
           (v.y<0 && b.p.y-b.h.y<=-half.y+eps);
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
// Scale a robot's whole velocity to follow a slower box. If the box itself
// approaches the robot, stop its group rather than displacing the robot.
void constrainRobotBox(const Body& robot,const Body& box,Vec2 rv,Vec2 bv,
                       double& scale,bool& stopBox) {
    const auto g=gap(robot,box); const auto d=sub(box.p,robot.p);
    auto axis=[&](double gapAxis,double dir,double r,double b) {
        if(gapAxis < -eps) return;
        r*=sign(dir); b*=sign(dir);
        if(b<0) stopBox=true;
        else if(r>b && r>0) scale=std::min(scale,b/r);
    };
    axis(g.x,d.x,rv.x,bv.x); axis(g.y,d.y,rv.y,bv.y);
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
    const Vec2 half{config.width/2,config.height/2};
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
        std::vector<std::array<bool,2>> pushes(n);
        std::array<bool,2> pushing{};
        for(std::size_t i=2;i<n;++i) if(bodies[i].kind==BodyKind::Box)
            for(std::size_t r=0;r<2;++r) if(closing(bodies[r],bodies[i],desired[r],{})) {
                pushes[groups[i]][r]=true; pushing[r]=true;
            }
        std::vector<Vec2> vel(n), groupVel(n);
        for(std::size_t r=0;r<2;++r)
            vel[r]=mul(desired[r],pushing[r]?config.pushSpeedMultiplier:1.0);
        for(std::size_t i=2;i<n;++i) if(groups[i]==i && bodies[i].kind==BodyKind::Box) {
            for(std::size_t r=0;r<2;++r) if(pushes[i][r]) groupVel[i]=add(groupVel[i],vel[r]);
            groupVel[i]=clampPushVelocity(groupVel[i],config.boxSpeedCap);
        }
        for(std::size_t i=2;i<n;++i) if(bodies[i].kind==BodyKind::Box) vel[i]=groupVel[groups[i]];
        // Synchronous constraint passes; constraints only reduce speeds. Stop
        // entire box groups at fixed obstacles/walls. No ID-based winner.
        bool resolved=false;
        for(std::size_t pass=0;pass<4*n+8;++pass) {
            std::vector<bool> stop(n,false);
            std::array<double,2> scale{1,1};
            for(std::size_t i=0;i<n;++i)
                if(wallClosing(bodies[i],vel[i],half)) stop[groups[i]]=true;
            for(std::size_t i=0;i<n;++i) for(std::size_t j=i+1;j<n;++j) {
                if(groups[i]==groups[j] || !closing(bodies[i],bodies[j],vel[i],vel[j])) continue;
                if(bodies[i].kind==BodyKind::Robot && bodies[j].kind==BodyKind::Box) {
                    bool halt=false;
                    constrainRobotBox(bodies[i],bodies[j],vel[i],vel[j],scale[i],halt);
                    if(halt) stop[groups[j]]=true;
                } else {
                    if(bodies[i].kind!=BodyKind::Fixed) stop[groups[i]]=true;
                    if(bodies[j].kind!=BodyKind::Fixed) stop[groups[j]]=true;
                }
            }
            bool changed=false;
            for(std::size_t i=0;i<n;++i) {
                const Vec2 old=vel[i];
                if(stop[groups[i]]) vel[i]={};
                else if(i<2) vel[i]=mul(vel[i],scale[i]);
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
        double t=remaining;
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
