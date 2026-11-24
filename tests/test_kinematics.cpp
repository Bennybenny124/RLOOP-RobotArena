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
    c.robots={{{{-10,0},RobotType::Tank},{{200,200},RobotType::Tank}}};
    return c;
}
ObstacleStateV1 box(unsigned id,double x,double y=0) { return {id,ObstacleType::Movable,{x,y},{5,5}}; }
int main() {
    const auto v=clampPushVelocity({3+4,4+3},5);
    REQUIRE(near(v.x,5/std::sqrt(2.0)) && near(v.y,5/std::sqrt(2.0)));
    CoreArena a; auto c=fixture(); c.obstacles={box(1,0),box(2,10)};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    auto s=a.stepDiscrete({{{6},{0}}}).state;
    REQUIRE(near(s.robots[0].position.x,-5));
    REQUIRE(near(s.obstacles[0].center.x,5) && near(s.obstacles[1].center.x,15));
    s=a.stepDiscrete({}).state;
    REQUIRE(near(s.obstacles[0].center.x,5));
    c.obstacles.resize(1); REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{6},{0}}}).state; REQUIRE(near(s.robots[0].position.x,-5));
    c.robots[1].spawn={10,0}; REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{6},{14}}}).state;
    REQUIRE(near(s.obstacles[0].center.x,0) && near(s.robots[0].position.x,-10) && near(s.robots[1].position.x,10));
    std::swap(c.robots[0],c.robots[1]); REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{14},{6}}}).state; REQUIRE(near(s.obstacles[0].center.x,0));
    c=fixture(); c.width=40; c.height=500; c.robots[1].spawn={0,200};
    c.obstacles={box(1,0),box(2,10)};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{6},{0}}}).state; REQUIRE(near(s.obstacles[1].center.x,15));
    s=a.stepDiscrete({{{6},{0}}}).state; REQUIRE(near(s.obstacles[1].center.x,15));
    REQUIRE(near(s.robots[0].position.x,-5));
    c=fixture(); c.obstacles={{1,ObstacleType::Fixed,{0,0},{5,5}}};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{6},{0}}}).state; REQUIRE(near(s.robots[0].position.x,-10));
    // High velocity cannot tunnel through a fixed box.
    c.robots[0].spawn={-100,0}; c.robotTypes[0].moveSpeed=500;
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{6},{0}}}).state; REQUIRE(near(s.robots[0].position.x,-10));
    // Corner contact tangent/separation is not a push.
    c=fixture(); c.robots[0].spawn={-10,-10}; c.obstacles={box(1,0)};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{6},{0}}}).state; REQUIRE(near(s.obstacles[0].center.x,0));
    REQUIRE(near(s.robots[0].position.x,0));
    std::cout << "kinematics passed\n";
}
