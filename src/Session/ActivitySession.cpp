#include "ActivitySession.h"

#include "Player.h"

#include <algorithm>
#include <utility>

namespace Sbrpg
{
    void BeginActivitySession(ActivitySession& session, Player* player, uint32_t durationMinutes, uint32_t nowMs)
    {
        session = ActivitySession();
        session.startedMs = nowMs;
        session.durationMs = durationMinutes * 60u * 1000u;
        if (!player)
            return;
        session.startMapId = player->GetMapId();
        session.startX = player->GetPositionX(); session.startY = player->GetPositionY();
        session.startZ = player->GetPositionZ(); session.startO = player->GetOrientation();
    }

    bool ActivityTimedOut(ActivitySession const& session, uint32_t nowMs)
    {
        return session.durationMs != 0 && nowMs - session.startedMs >= session.durationMs;
    }

    uint32_t ActivityRemainingSeconds(ActivitySession const& session, uint32_t nowMs)
    {
        if (session.durationMs == 0)
            return 0;
        uint32_t const elapsed = nowMs - session.startedMs;
        return elapsed >= session.durationMs ? 0 : (session.durationMs - elapsed + 999) / 1000;
    }

    void RequestActivityReturn(ActivitySession& session, uint32_t nowMs, std::string reason)
    {
        if (session.returnRequested)
            return;
        session.returnRequested = true;
        session.returnRequestedMs = nowMs;
        session.returnReason = std::move(reason);
    }

    bool IsAtActivityStart(ActivitySession const& session, Player* player, float radius)
    {
        return player && player->GetMapId() == session.startMapId &&
            player->GetExactDist(session.startX, session.startY, session.startZ) <= radius;
    }
}
