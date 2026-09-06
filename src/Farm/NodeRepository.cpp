#include "NodeRepository.h"

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
    std::vector<RoutePoint> NodeRepository::LoadRoute(Player* player, FarmState const& state,
        std::function<bool(uint32)> const& isGatheringEntry)
    {
        std::vector<RoutePoint> route;
        if (!player || state.entries.empty())
            return route;

        std::ostringstream ids;
        for (size_t i = 0; i < state.entries.size(); ++i)
        {
            if (i) ids << ',';
            ids << state.entries[i];
        }
        QueryResult rows = WorldDatabase.Query(
            "SELECT guid, id, position_x, position_y, position_z FROM gameobject WHERE map = {} AND id IN ({})",
            player->GetMapId(), ids.str());
        if (!rows)
            return route;

        do
        {
            Field* fields = rows->Fetch();
            float const x = fields[2].Get<float>(), y = fields[3].Get<float>(), z = fields[4].Get<float>();
            if ((!state.stayInCurrentZone || player->GetMap()->GetZoneId(player->GetPhaseMask(), x, y, z) == state.zoneId) &&
                isGatheringEntry(fields[1].Get<uint32>()))
                route.push_back({ fields[0].Get<uint32>(), fields[1].Get<uint32>(), x, y, z, false });
        } while (rows->NextRow());
        return route;
    }

    std::vector<LiveNodeObservation> NodeRepository::ScanLive(Player* player, FarmState const& state, float radius)
    {
        std::vector<LiveNodeObservation> nodes;
        if (!player)
            return nodes;
        std::list<GameObject*> scanned;
        AnyGameObjectInObjectRangeCheck check(player, radius);
        Acore::GameObjectListSearcher<AnyGameObjectInObjectRangeCheck> searcher(player, scanned, check);
        Cell::VisitObjects(player, searcher, radius);
        for (GameObject* go : scanned)
        {
            if (!go || !go->IsInWorld() ||
                std::find(state.entries.begin(), state.entries.end(), go->GetEntry()) == state.entries.end())
                continue;
            LiveNodeObservation observation;
            observation.guid = go->GetGUID();
            observation.entry = go->GetEntry();
            observation.spawn = go->GetSpawnId();
            observation.mapId = go->GetMapId();
            observation.x = go->GetPositionX();
            observation.y = go->GetPositionY();
            observation.z = go->GetPositionZ();
            observation.spawned = go->isSpawned();
            observation.selectable = !go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE);
            nodes.push_back(observation);
        }
        return nodes;
    }

    void NodeRepository::UpdateLiveAssociations(Player* player, FarmState& state, float radius, uint32 now)
    {
        if (!player)
            return;

        for (RoutePoint& point : state.route)
        {
            point.liveGuid = ObjectGuid::Empty;
            point.liveX = point.x;
            point.liveY = point.y;
            point.liveZ = point.z;
            point.observedMs = 0;
            point.observation = NodeObservationState::Unknown;
            if (state.blacklistedUntilMs.find(point.spawn) != state.blacklistedUntilMs.end())
                point.observation = NodeObservationState::TemporarilySkipped;
        }

        for (LiveNodeObservation const& live : state.liveCache.Observations())
        {
            RoutePoint* match = nullptr;
            if (live.spawn != 0)
            {
                auto it = std::find_if(state.route.begin(), state.route.end(),
                    [&live](RoutePoint const& point) { return point.spawn == live.spawn && point.entry == live.entry; });
                if (it != state.route.end())
                    match = &*it;
            }
            if (!match)
            {
                float bestDistance = 8.0f * 8.0f;
                bool ambiguous = false;
                for (RoutePoint& point : state.route)
                {
                    if (point.entry != live.entry)
                        continue;
                    float const dx = point.x - live.x, dy = point.y - live.y, dz = point.z - live.z;
                    float const distance = dx * dx + dy * dy + dz * dz;
                    if (distance >= 8.0f * 8.0f)
                        continue;
                    if (!match || distance < bestDistance - 0.01f)
                    {
                        match = &point;
                        bestDistance = distance;
                        ambiguous = false;
                    }
                    else if (std::abs(distance - bestDistance) <= 0.01f)
                        ambiguous = true;
                }
                if (ambiguous)
                    match = nullptr;
            }
            if (match && live.spawned && live.selectable)
            {
                match->liveGuid = live.guid;
                match->liveX = live.x;
                match->liveY = live.y;
                match->liveZ = live.z;
                match->observation = NodeObservationState::Available;
                match->observedMs = live.observedMs ? live.observedMs : now;
            }
        }

        float const px = player->GetPositionX(), py = player->GetPositionY(), pz = player->GetPositionZ();
        float const radiusSquared = radius * radius;
        for (RoutePoint& point : state.route)
        {
            if (point.observation != NodeObservationState::Unknown)
                continue;
            float const dx = point.x - px, dy = point.y - py, dz = point.z - pz;
            if (dx * dx + dy * dy + dz * dz <= radiusSquared)
            {
                point.observation = NodeObservationState::Unavailable;
                point.observedMs = now;
            }
        }
    }

    bool NodeRepository::SelectNextRoute(Player* /*player*/, FarmState& state, uint32& spawn, float& x, float& y, float& z)
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
    }
}
