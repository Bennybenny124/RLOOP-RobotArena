#pragma once

#include "CoreRobot.h"

namespace rloop {

// Headless authoritative world. No Unreal types, callbacks or wall-clock time.
class CoreArena {
public:
    ResetStatus reset(std::uint64_t seed, const ArenaConfig& config);
    StepResultV1 step(const ActionsV1& actions);
    WorldStateV1 state() const;
    bool isTerminal() const noexcept;

private:
    ArenaConfig config_{};
    std::array<CoreRobot, 2> robots_{};
    std::uint64_t seed_ = 0;
    std::uint64_t tick_ = 0;
    MatchResult result_ = MatchResult::NotStarted;
};

} // namespace rloop
