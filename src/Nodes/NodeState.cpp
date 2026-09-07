#include "SBRPG.h"

#include "Timer.h"

namespace Sbrpg
{
    char const* PhaseLabel(FarmPhase phase)
    {
        switch (phase)
        {
            case FarmPhase::Planning: return "planning route";
            case FarmPhase::SelectingNode: return "selecting node";
            case FarmPhase::BuildingPath: return "building path";
            case FarmPhase::Travelling: return "travelling";
            case FarmPhase::CombatPaused: return "combat paused";
            case FarmPhase::ApproachingNode: return "approaching node";
            case FarmPhase::GatherPending: return "gather pending";
            case FarmPhase::Looting: return "looting";
            case FarmPhase::Returning: return "returning home";
            case FarmPhase::Recovering: return "recovering";
            case FarmPhase::Waiting: return "waiting";
            case FarmPhase::Failed: return "failed";
            default: return "stopped";
        }
    }

    void Transition(FarmState& state, FarmPhase phase, std::string reason)
    {
        if (state.phase == phase && state.lastReason == reason)
            return;
        state.phase = phase;
        state.activity = PhaseLabel(phase);
        state.lastReason = std::move(reason);
        state.lastStateChangeMs = getMSTime();
        ++state.revision;
    }
}
