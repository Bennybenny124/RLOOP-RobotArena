#pragma once
#include "CoreArena.h"

namespace rloop {
// External adapters (keyboard, AI, replay) implement one 18-value action
// from the same snapshot. No controller may mutate the simulation.
class Controller {
public:
    virtual ~Controller() = default;
    virtual ActionV2 action(const WorldStateV2& state,std::size_t player) = 0;
};

inline StepResultV2 stepControllers(CoreArena& arena,Controller& p0,Controller& p1) {
    const auto snapshot=arena.state();
    if(snapshot.result!=MatchResult::Running) return arena.stepDiscrete({});
    const ActionV2 a=p0.action(snapshot,0);
    const ActionV2 b=p1.action(snapshot,1);
    return arena.stepDiscrete({a,b});
}
}
