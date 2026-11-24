#include "CoreArena.h"
#include "CoreCombat.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
using namespace rloop;
#define REQUIRE(x) do { if(!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while(false)
ArenaConfig fixture() {
    ArenaConfig c; c.tickRateHz=10; c.maxTicks=100; c.outOfCombatDelayMs=150;
    c.robots={{{{-100,0},RobotType::Tank},{{100,0},RobotType::Scout}}};
    for(auto& t:c.robotTypes) t={100*hpScale,16*hpScale,500,250,100,10*hpScale,5};
    return c;
}
const EventV2* attack(const StepResultV2& s,unsigned player) {
    for(const auto& e:s.events) if(e.kind==EventKind::Attack && e.player==player) return &e;
    return nullptr;
}
int main() {
    CoreArena a; auto c=fixture();
    c.obstacles={{1,ObstacleType::Movable,{-30,0},{10,10}},
                 {2,ObstacleType::Movable,{30,0},{10,10}}};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    auto s=a.stepDiscrete({{{1},{0}}});
    REQUIRE(attack(s,0) && attack(s,0)->resultCode==AttackResult::Reduced);
    REQUIRE(attack(s,0)->actualDamage==hpScale && s.state.robots[1].hp==99*hpScale);
    REQUIRE(s.state.robots[0].cooldownTicks==5 && s.state.robots[1].quietTicks==0);
    c.obstacles.push_back({3,ObstacleType::Fixed,{60,0},{5,10}});
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{1},{0}}});
    REQUIRE(attack(s,0)->resultCode==AttackResult::Blocked && attack(s,0)->to.x==55);
    REQUIRE(s.state.robots[1].hp==100*hpScale && s.state.robots[0].cooldownTicks==5);
    c=fixture(); c.robotTypes[0].attackRange=100;
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{1},{1}}});
    REQUIRE(attack(s,0)->resultCode==AttackResult::OutOfRange);
    REQUIRE(s.state.robots[0].hp==84*hpScale && s.state.robots[0].cooldownTicks==5);
    s=a.stepDiscrete({{{1},{0}}});
    REQUIRE(attack(s,0)->resultCode==AttackResult::OnCooldown && s.state.robots[0].quietTicks==1);
    REQUIRE(s.state.robots[0].hp==84*hpScale);
    s=a.stepDiscrete({{{1},{0}}});
    REQUIRE(s.state.robots[0].quietTicks==2 && s.state.robots[0].hp==84500000);
    s=a.stepDiscrete({{{1},{0}}});
    REQUIRE(s.state.robots[0].hp==85500000 && s.state.robots[0].cooldownTicks==2);
    a.stepDiscrete({{{1},{0}}}); s=a.stepDiscrete({{{1},{0}}});
    REQUIRE(attack(s,0)->resultCode==AttackResult::OutOfRange && s.state.robots[0].quietTicks==0);
    c=fixture(); c.robotTypes[0].attackDamage=100*hpScale; c.robotTypes[1].attackDamage=100*hpScale;
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{1},{1}}});
    REQUIRE(s.state.result==MatchResult::Draw && s.state.robots[0].hp==0 && s.state.robots[1].hp==0);
    REQUIRE(attack(s,0) && attack(s,1));
    c=fixture(); c.maxTicks=1; c.robotTypes[0].attackDamage=1;
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{1},{0}}}); REQUIRE(s.state.result==MatchResult::Player0Win);
    std::array<RobotStateV2,2> ratios{};
    ratios[0].hp=50*hpScale; ratios[0].maxHp=100*hpScale;
    ratios[1].hp=60*hpScale; ratios[1].maxHp=120*hpScale;
    REQUIRE(resolveMatch(ratios,true)==MatchResult::Draw);
    --ratios[1].hp; REQUIRE(resolveMatch(ratios,true)==MatchResult::Player0Win);
    // Edge touching cover counts; moving into range is resolved before shooting.
    c=fixture(); c.robotTypes[0].attackRange=190;
    c.obstacles={{1,ObstacleType::Fixed,{0,10},{5,10}}};
    REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{7},{0}}}); REQUIRE(attack(s,0)->resultCode==AttackResult::Blocked);
    c.obstacles.clear(); REQUIRE(a.reset(1,c)==ResetStatus::Ready);
    s=a.stepDiscrete({{{7},{0}}}); REQUIRE(attack(s,0)->resultCode==AttackResult::Hit);
    std::cout<<"combat passed\n";
}
