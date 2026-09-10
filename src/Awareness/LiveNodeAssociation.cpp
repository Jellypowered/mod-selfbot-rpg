#include "Awareness/LiveNodeAssociation.h"
#include "Core/SbrpgConfig.h"

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
    std::vector<LiveNodeObservation> LiveNodeAssociation::ScanLive(Player* player, FarmState const& state, float radius)
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
        // Cell visitation order is not a routing policy. Keep the local live
        // list nearest-first so a farther node cannot consume the stock-gather
        // claim window while a closer visible node is available. Spawn ID and
        // entry make equal-distance ordering deterministic; GUID is only a
        // final tie-breaker for dynamic/unspawned records without a spawn ID.
        uint32 const scoreTime = getMSTime();
        std::stable_sort(nodes.begin(), nodes.end(), [player, &state, scoreTime](LiveNodeObservation const& left, LiveNodeObservation const& right)
        {
            float const leftDistance = player->GetExactDist(left.x, left.y, left.z);
            float const rightDistance = player->GetExactDist(right.x, right.y, right.z);
            uint32 const now = scoreTime;
            uint32 const alternates = state.liveCache.Observations().size() > 0 ?
                static_cast<uint32>(state.liveCache.Observations().size() - 1) : 0;
            float const leftGeometry = state.pathGeometryCache.count(left.spawn) ?
                Awareness::GeometryPenalty(state.pathGeometryCache.at(left.spawn), alternates) : 0.0f;
            float const rightGeometry = state.pathGeometryCache.count(right.spawn) ?
                Awareness::GeometryPenalty(state.pathGeometryCache.at(right.spawn), alternates) : 0.0f;
            float const leftCost = Runtime::runtimeSettings.adaptiveOrdering ?
                state.routeEvidence.Cost(left.spawn, leftDistance, left.z - player->GetPositionZ(), now) + leftGeometry : leftDistance;
            float const rightCost = Runtime::runtimeSettings.adaptiveOrdering ?
                state.routeEvidence.Cost(right.spawn, rightDistance, right.z - player->GetPositionZ(), now) + rightGeometry : rightDistance;
            // An epsilon comparator is not transitive and violates sort's contract.
            if (leftCost != rightCost)
                return leftCost < rightCost;
            if (left.spawn != right.spawn)
                return left.spawn < right.spawn;
            if (left.entry != right.entry)
                return left.entry < right.entry;
            return left.guid.GetCounter() < right.guid.GetCounter();
        });
        return nodes;
    }

    void LiveNodeAssociation::UpdateLiveAssociations(Player* player, FarmState& state, float radius, uint32 now)
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

}
