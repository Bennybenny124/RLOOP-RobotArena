#include "CoreController.h"
#include <cstdlib>
#include <iostream>
using namespace rloop;
#define REQUIRE(x) do { if(!(x)) { std::cerr<<__LINE__<<": "<<#x<<'\n'; std::exit(1); } } while(false)
struct Scripted : Controller {
    std::uint64_t calls=0;
    std::uint64_t observedTick=0;
    std::size_t index=2;
    ActionV2 next{};
    ActionV2 action(const WorldStateV2& s,std::size_t player) override {
        ++calls; observedTick=s.tick; index=player; return next;
    }
};
int main() {
    CoreArena arena; ArenaConfig c;
    c.robots[1].type=RobotType::Tank;
    c.maxTicks=3;
    Scripted a,b;
    REQUIRE(stepControllers(arena,a,b).status==StepStatus::NotInitialized && a.calls==0);
    REQUIRE(arena.reset(42,c)==ResetStatus::Ready);
    a.next=encodeAction(-1,0,false); b.next=encodeAction(1,0,false);
    auto s=stepControllers(arena,a,b);
    REQUIRE(s.state.tick==1 && a.calls==1 && b.calls==1 && a.observedTick==0 && b.observedTick==0);
    REQUIRE(a.index==0 && b.index==1 && s.state.robots[0].position.x==-395 && s.state.robots[1].position.x==395);
    b.next={18};
    REQUIRE(stepControllers(arena,a,b).status==StepStatus::InvalidAction && arena.state().tick==1);
    b.next={0};
    stepControllers(arena,a,b);
    s=stepControllers(arena,a,b);
    REQUIRE(s.state.result==MatchResult::Draw && s.state.tick==3);
    REQUIRE(stepControllers(arena,a,b).status==StepStatus::Terminal && a.calls==4 && b.calls==4);
    std::cout<<"controllers passed\n";
}
