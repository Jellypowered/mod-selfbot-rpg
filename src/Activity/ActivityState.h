#ifndef SELFBOTRPG_ACTIVITY_STATE_H
#define SELFBOTRPG_ACTIVITY_STATE_H

#include <cstdint>
#include <string>

#include "Session/ActivitySession.h"

namespace Sbrpg
{
    // Shared lifecycle phases. Mode-specific controllers may add substates,
    // while the activity/session fields below remain mode-independent.
    enum class ActivityPhase : uint8_t
    {
        Stopped, Planning, SelectingNode, BuildingPath, Travelling,
        CombatPaused, ApproachingNode, GatherPending, Looting,
        Returning, Recovering, Waiting, Failed
    };

    struct ActivityState
    {
        bool active = false;
        bool selfBotEnabledBySbrpg = false;
        ActivityPhase phase = ActivityPhase::Stopped;
        uint64_t runId = 0;
        uint32_t revision = 0;
        uint32_t lastPublishedRevision = 0;
        uint32_t lastStatusPublishMs = 0;
        uint32_t lastStateChangeMs = 0;
        uint32_t lastActionMs = 0;
        uint32_t lootWaitSinceMs = 0;
        std::string lastReason;
        ActivitySession session;
    };
}

#endif
