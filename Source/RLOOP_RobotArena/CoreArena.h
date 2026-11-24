#pragma once

#include "CoreRobot.h"

namespace rloop {

// Headless authoritative world. No Unreal types, callbacks or wall-clock time.
class CoreArena {
public:
    ResetStatus reset(std::uint64_t seed, const ArenaConfig& config);
    StepResultV2 step(const ActionsV1& actions);
    StepResultV2 stepDiscrete(const ActionsV2& actions);
    WorldStateV2 state() const;
    const std::string& lastError() const noexcept { return lastError_; }
    bool isTerminal() const noexcept;

private:
    ArenaConfig config_{};
    std::string lastError_;
    std::array<CoreRobot, 2> robots_{};
    std::uint64_t seed_ = 0;
    std::uint64_t tick_ = 0;
    MatchResult result_ = MatchResult::NotStarted;
};

} // namespace rloop
