#ifndef SELFBOTRPG_ACTIVITY_SESSION_H
#define SELFBOTRPG_ACTIVITY_SESSION_H

#include <cstdint>
#include <string>

class Player;

namespace Sbrpg
{
    struct ActivitySession
    {
        uint32_t startedMs = 0;
        uint32_t durationMs = 0;
        uint32_t startMapId = 0;
        float startX = 0.0f, startY = 0.0f, startZ = 0.0f, startO = 0.0f;
        bool returnRequested = false;
        uint32_t returnRequestedMs = 0;
        std::string returnReason;
    };

    void BeginActivitySession(ActivitySession& session, Player* player, uint32_t durationMinutes, uint32_t nowMs);
    bool ActivityTimedOut(ActivitySession const& session, uint32_t nowMs);
    uint32_t ActivityRemainingSeconds(ActivitySession const& session, uint32_t nowMs);
    void RequestActivityReturn(ActivitySession& session, uint32_t nowMs, std::string reason);
    bool IsAtActivityStart(ActivitySession const& session, Player* player, float radius);
}

#endif
