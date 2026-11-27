#include "CoreArena.h"
#include "CoreKinematics.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <algorithm>
using namespace rloop;
#define REQUIRE(x) do { if(!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while(false)
bool near(double a,double b) { return std::abs(a-b)<1e-6; }
ArenaConfig fixture() {
    ArenaConfig c; c.tickRateHz=1; c.maxTicks=20; c.boxSpeedCap=5;
    c.robotTypes[0].moveSpeed=10; c.robotTypes[0].halfExtent=5;
    c.map.robots={{{{-10,0},RobotType::Tank},{{200,200},RobotType::Tank}}};
    return c;
}
ObstacleStateV1 box(unsigned id,double x,double y=0) { return {id,ObstacleType::Movable,{x,y},{5,5}}; }
int main() {
    const auto v=clampPushVelocity({3+4,4+3},5);
    REQUIRE(near(v.x,5/std::sqrt(2.0)) && near(v.y,5/std::sqrt(2.0)));
    CoreArena a; auto c=fixture(); c.map.obstacles={box(1,0),box(2,10)};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    auto s=a.step({{{6},{0}}}).state;
    REQUIRE(near(s.robots[0].position.x,-5));
    REQUIRE(near(s.obstacles[0].center.x,5) && near(s.obstacles[1].center.x,15));
    s=a.step({}).state;
    REQUIRE(near(s.obstacles[0].center.x,5));
    c.map.obstacles.resize(1); REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{6},{0}}}).state; REQUIRE(near(s.robots[0].position.x,-5));
    c.map.robots[1].spawn={10,0}; REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{6},{14}}}).state;
    REQUIRE(near(s.obstacles[0].center.x,0) && near(s.robots[0].position.x,-10) && near(s.robots[1].position.x,10));
    std::swap(c.map.robots[0],c.map.robots[1]); REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{14},{6}}}).state; REQUIRE(near(s.obstacles[0].center.x,0));
    c=fixture(); c.map.width=40; c.map.height=500; c.map.robots[1].spawn={0,200};
    c.map.obstacles={box(1,0),box(2,10)};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{6},{0}}}).state; REQUIRE(near(s.obstacles[1].center.x,15));
    s=a.step({{{6},{0}}}).state; REQUIRE(near(s.obstacles[1].center.x,15));
    REQUIRE(near(s.robots[0].position.x,-5));
    c=fixture(); c.map.obstacles={{1,ObstacleType::Fixed,{0,0},{5,5}}};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{6},{0}}}).state; REQUIRE(near(s.robots[0].position.x,-10));
    // High velocity cannot tunnel through a fixed box.
    c.map.robots[0].spawn={-100,0}; c.robotTypes[0].moveSpeed=500;
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{6},{0}}}).state; REQUIRE(near(s.robots[0].position.x,-10));
    // Corner contact tangent/separation is not a push.
    c=fixture(); c.map.robots[0].spawn={-10,-10}; c.map.obstacles={box(1,0)};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{6},{0}}}).state; REQUIRE(near(s.obstacles[0].center.x,0));
    REQUIRE(near(s.robots[0].position.x,0));
    // Two independent pushers: actual (3,4)+(4,3) post-slowdown
    // contribution, clamped by vector norm, not per-axis.
    c=fixture(); c.map.robots[0].spawn={-15,0}; c.map.robots[1].spawn={0,-15};
    c.map.obstacles={{9,ObstacleType::Movable,{0,0},{10,10}}};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    // Exercise the internal solver directly for the exact non-discrete vectors.
    DecodedActions diagonal{{{{0.6,0.8},false},{{0.8,0.6},false}}};
    auto start=a.state();
    auto boxes=start.obstacles;
    resolveMovement(c,start.robots,boxes,diagonal);
    REQUIRE(near(boxes[0].center.x,5/std::sqrt(2.0)));
    REQUIRE(near(boxes[0].center.y,5/std::sqrt(2.0)));
    auto swapped=c; std::swap(swapped.map.robots[0],swapped.map.robots[1]);
    std::swap(diagonal[0],diagonal[1]);
    REQUIRE(a.reset(1,swapped)==ResetStatus::Ready);
    start=a.state();
    auto reverseBoxes=start.obstacles;
    resolveMovement(swapped,start.robots,reverseBoxes,diagonal);
    REQUIRE(near(reverseBoxes[0].center.x,boxes[0].center.x));
    REQUIRE(near(reverseBoxes[0].center.y,boxes[0].center.y));
    auto r=a.state();
    // Same direction contributions can increase speed, but never the cap.
    c=fixture(); c.boxSpeedCap=7; c.map.robots[0].spawn={-15,-5}; c.map.robots[1].spawn={-15,5};
    c.map.obstacles={{9,ObstacleType::Movable,{0,0},{10,10}}};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{6},{6}}}).state;
    REQUIRE(near(s.obstacles[0].center.x,7));
    REQUIRE(near(s.robots[0].position.x,-10) && near(s.robots[1].position.x,-10));
    // A previously separate box joins the chain at a swept contact.
    c=fixture(); c.map.obstacles={box(1,0),box(2,12)};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{6},{0}}}).state;
    REQUIRE(near(s.obstacles[0].center.x,5) && near(s.obstacles[1].center.x,15));
    std::reverse(c.map.obstacles.begin(),c.map.obstacles.end());
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    r=a.step({{{6},{0}}}).state;
    REQUIRE(near(r.obstacles[1].center.x,s.obstacles[0].center.x));
    REQUIRE(near(r.obstacles[0].center.x,s.obstacles[1].center.x));
    // Touching but moving away does not slow the robot or move a box.
    c=fixture(); c.map.obstacles={box(1,0)};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{14},{0}}}).state;
    REQUIRE(near(s.robots[0].position.x,-20) && near(s.obstacles[0].center.x,0));
    // Wall sliding preserves the original diagonal component, not full speed.
    const double component=10/std::sqrt(2.0);
    struct WallCase { Vec2 spawn; int action; Vec2 expected; };
    const WallCase wallCases[] = {
        {{645,0},8,{645,-component}},    // right wall, W+D (+X,-Y)
        {{645,0},4,{645,component}},     // right wall, S+D
        {{-645,0},12,{-645,-component}}, // left wall, W+A
        {{-645,0},16,{-645,component}},
        {{0,-545},8,{component,-545}},
        {{0,-545},12,{-component,-545}},
        {{0,545},4,{component,545}},
        {{0,545},16,{-component,545}}
    };
    for(const auto& wall:wallCases) {
        c=fixture(); c.map.robots[0].spawn=wall.spawn;
        REQUIRE(a.reset(1,c)==ResetStatus::Ready);
        const auto step=a.step({{{wall.action},{0}}});
        REQUIRE(near(step.state.robots[0].position.x,wall.expected.x));
        REQUIRE(near(step.state.robots[0].position.y,wall.expected.y));
        for(const auto& e:step.events) REQUIRE(e.kind!=EventKind::MovementLimited);
    }
    // Contact reached partway through a tick: continue tangentially for
    // the remaining time, including following ticks.
    c=fixture(); c.map.robots[0].spawn={642,0};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{8},{0}}}).state;
    REQUIRE(near(s.robots[0].position.x,645) && near(s.robots[0].position.y,-component));
    s=a.step({{{8},{0}}}).state;
    REQUIRE(near(s.robots[0].position.x,645) && near(s.robots[0].position.y,-2*component));
    // Cardinal movement into the wall stops; moving away is unrestricted.
    s=a.step({{{6},{0}}}).state;
    REQUIRE(near(s.robots[0].position.x,645) && near(s.robots[0].position.y,-2*component));
    s=a.step({{{14},{0}}}).state;
    REQUIRE(near(s.robots[0].position.x,635) && near(s.robots[0].position.y,-2*component));
    // At a corner, both blocked components are removed; one free
    // component can still move away from the corner.
    c=fixture(); c.map.robots[0].spawn={645,-545};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{8},{0}}}).state;
    REQUIRE(near(s.robots[0].position.x,645) && near(s.robots[0].position.y,-545));
    s=a.step({{{4},{0}}}).state;
    REQUIRE(near(s.robots[0].position.x,645) && near(s.robots[0].position.y,-545+component));
    // Wall-blocked robot intent cannot inject a normal push into a box.
    c=fixture(); c.map.robots[0].spawn={645,0};
    c.map.obstacles={box(1,645,-10)};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.step({{{8},{0}}}).state;
    REQUIRE(near(s.robots[0].position.x,645) && near(s.robots[0].position.y,-component/2));
    REQUIRE(near(s.obstacles[0].center.x,645) && near(s.obstacles[0].center.y,-10-component/2));
    std::cout << "kinematics passed\n";
}
