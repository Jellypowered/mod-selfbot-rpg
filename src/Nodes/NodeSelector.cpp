#include "Nodes/NodeSelector.h"

#include "DatabaseEnv.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Map.h"
#include "NearestGameObjects.h"
#include "Player.h"
#include "Timer.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Sbrpg
{
    bool NodeSelector::SelectNextRoute(Player* /*player*/, FarmState& state, uint32& spawn, float& x, float& y, float& z)
    {
        if (state.route.empty())
            return false;
        uint32 const now = getMSTime();
        for (auto it = state.blacklistedUntilMs.begin(); it != state.blacklistedUntilMs.end(); )
        {
            if (now >= it->second)
                it = state.blacklistedUntilMs.erase(it);
            else
                ++it;
        }
        auto select = [&](bool liveOnly)
        {
            uint32 const start = state.routeIndex;
            uint32 attempts = 0;
            while (attempts < state.route.size())
            {
                RoutePoint& point = state.route[state.routeIndex];
                auto blocked = state.blacklistedUntilMs.find(point.spawn);
                bool const blockedNow = blocked != state.blacklistedUntilMs.end() && now < blocked->second;
                bool const eligible = !point.visited && !blockedNow &&
                    point.observation != NodeObservationState::Unavailable &&
                    (!liveOnly || point.observation == NodeObservationState::Available);
                if (eligible)
                {
                    bool const wasLive = point.observation == NodeObservationState::Available;
                    point.visited = true;
                    point.observation = NodeObservationState::TravelCandidate;
                    spawn = point.spawn;
                    x = wasLive ? point.liveX : point.x;
                    y = wasLive ? point.liveY : point.y;
                    z = wasLive ? point.liveZ : point.z;
                    state.routeIndex = (state.routeIndex + 1) % state.route.size();
                    return true;
                }
                state.routeIndex = (state.routeIndex + 1) % state.route.size();
                if (state.routeIndex == start)
                    break;
                ++attempts;
            }
            return false;
        };
        if (select(true) || select(false))
            return true;
        for (RoutePoint& point : state.route)
            point.visited = false;
        state.routeIndex = 0;
        return false;
    }}
