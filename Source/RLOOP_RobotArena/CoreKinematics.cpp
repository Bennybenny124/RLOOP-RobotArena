#include "CoreKinematics.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

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
// Restrict the closing relative normal speed symmetrically, without
// adding velocity to either body. Opposing inward components become zero;
// for same-direction travel, only the faster follower is limited.
// Tangential components and separating motion are left untouched.
void constrainRelativeNormal(const Body& a,const Body& b,Vec2 av,Vec2 bv,
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
// Face contacts can end while objects slide past one another. Recompute
// push sources and constraints at that instant, not only at the next tick.
// Do not create zero-time events at corners.
double contactRelease(const Body& a,const Body& b,Vec2 av,Vec2 bv,double limit) {
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
// Each source is carried at most once per axis and box, even when
// multiple contact paths reach the same box. Direct robot pushes retain
// their full vector; box-to-box contacts transmit only inward normals.
using PushSources = std::array<unsigned char,2>;
constexpr unsigned char pushX=1, pushY=2;
Vec2 sourceVelocity(Vec2 drive,unsigned char mask) {
    return {(mask&pushX)?drive.x:0.0,(mask&pushY)?drive.y:0.0};
}
Vec2 boxVelocity(const PushSources& sources,const std::array<Vec2,2>& drive,
                 double cap) {
    return clampPushVelocity(add(sourceVelocity(drive[0],sources[0]),
                                 sourceVelocity(drive[1],sources[1])),cap);
}
// Compute active compression paths synchronously. A separating or purely
// tangential pair cannot start a path. Each propagation round adds only
// source-axis bits, so termination is bounded by four bits per box.
void propagatePushes(const std::vector<Body>& bodies,Vec2 half,double cap,
                     const std::array<Vec2,2>& drive,
                     std::vector<PushSources>& sources,std::vector<Vec2>& vel) {
    std::vector<std::pair<std::size_t,std::size_t>> contacts;
    for(std::size_t i=2;i<bodies.size();++i) if(bodies[i].kind==BodyKind::Box)
        for(std::size_t j=i+1;j<bodies.size();++j)
            if(bodies[j].kind==BodyKind::Box && touching(bodies[i],bodies[j]))
                contacts.emplace_back(i,j);
    auto update=[&]() {
        for(std::size_t i=2;i<bodies.size();++i) if(bodies[i].kind==BodyKind::Box)
            vel[i]=projectFixedVelocity(bodies[i],bodies,
                projectWallVelocity(bodies[i],boxVelocity(sources[i],drive,cap),half));
    };
    for(std::size_t pass=0;pass<=4*bodies.size();++pass) {
        update();
        auto next=sources;
        auto transmit=[&](std::size_t from,std::size_t to) {
            if(!closing(bodies[from],bodies[to],vel[from],vel[to]) ||
               !closing(bodies[from],bodies[to],vel[from],{})) return;
            const auto g=gap(bodies[from],bodies[to]);
            for(std::size_t r=0;r<2;++r) {
                const auto v=sourceVelocity(drive[r],sources[from][r]);
                if(!closing(bodies[from],bodies[to],v,{})) continue;
                unsigned char mask=0;
                if(g.x>=-eps) mask|=pushX;
                if(g.y>=-eps) mask|=pushY;
                next[to][r]|=static_cast<unsigned char>(sources[from][r]&mask);
            }
        };
        for(const auto& pair:contacts) {
            transmit(pair.first,pair.second);
            transmit(pair.second,pair.first);
        }
        if(next==sources) return;
        sources=std::move(next);
    }
    update();
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
        // A contact is not a rigid joint. Rebuild the source graph at
        // each impact/release and discard it when the interval ends.
        std::array<Vec2,2> intent{},drive{};
        for(std::size_t r=0;r<2;++r)
            intent[r]=projectFixedVelocity(bodies[r],bodies,projectWallVelocity(bodies[r],desired[r],half));
        std::vector<PushSources> sources(n);
        std::array<bool,2> pushing{};
        for(std::size_t i=2;i<n;++i) if(bodies[i].kind==BodyKind::Box)
            for(std::size_t r=0;r<2;++r) if(closing(bodies[r],bodies[i],intent[r],{})) {
                sources[i][r]=pushX|pushY;
                pushing[r]=true;
            }
        std::vector<Vec2> vel(n);
        for(std::size_t r=0;r<2;++r)
            vel[r]=drive[r]=mul(intent[r],pushing[r]?config.pushSpeedMultiplier:1.0);
        propagatePushes(bodies,half,config.boxSpeedCap,drive,sources,vel);
        // Solve remaining relative normal constraints simultaneously.
        // Boxes can transmit compression but never tension or frictional
        // drag. Constraints only reduce velocity, not add momentum.
        bool resolved=false;
        for(std::size_t pass=0;pass<4*n+8;++pass) {
            auto limit=vel;
            for(std::size_t i=0;i<n;++i) for(std::size_t j=i+1;j<n;++j) {
                if(bodies[i].kind==BodyKind::Fixed || bodies[j].kind==BodyKind::Fixed ||
                   !closing(bodies[i],bodies[j],vel[i],vel[j])) continue;
                // Robots never receive passive motion from a box. The
                // same component-wise relative limit prevents overlap
                // for robot pairs, robot/box pairs and box pairs.
                constrainRelativeNormal(bodies[i],bodies[j],vel[i],vel[j],limit[i],limit[j]);
            }
            bool changed=false;
            for(std::size_t i=0;i<n;++i) {
                changed=changed || vel[i].x!=limit[i].x || vel[i].y!=limit[i].y;
                vel[i]=limit[i];
            }
            if(!changed) {
                resolved=true;
                // No unresolved inward contact may advance.
                for(std::size_t i=0;i<n;++i) {
                    if(wallClosing(bodies[i],vel[i],half)) resolved=false;
                    for(std::size_t j=i+1;j<n;++j)
                        if(closing(bodies[i],bodies[j],vel[i],vel[j])) resolved=false;
                }
                break;
            }
        }
        if(!resolved) { limited=true; break; }
        if(std::none_of(vel.begin(),vel.end(),moving)) { remaining=0; break; }
        double t=remaining;
        for(std::size_t i=0;i<n;++i) {
            t=wallTime(bodies[i],vel[i],half,t);
            for(std::size_t j=i+1;j<n;++j) {
                t=impact(bodies[i],bodies[j],vel[i],vel[j],t);
                t=contactRelease(bodies[i],bodies[j],vel[i],vel[j],t);
            }
        }
        if(!(t>0)) break;
        for(std::size_t i=0;i<n;++i) bodies[i].p=add(bodies[i].p,mul(vel[i],t));
        remaining-=t;
    }
    for(std::size_t i=2;i<n;++i) obstacles[i-2].center=bodies[i].p;
    return {{bodies[0].p,bodies[1].p},limited || remaining>0};
}
}
