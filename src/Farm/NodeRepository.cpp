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

    std::vector<ObjectGuid> NodeRepository::ScanLive(Player* player, FarmState const& state, float radius)
    {
        std::vector<ObjectGuid> nodes;
        if (!player)
            return nodes;
        std::list<GameObject*> scanned;
        AnyGameObjectInObjectRangeCheck check(player, radius);
        Acore::GameObjectListSearcher<AnyGameObjectInObjectRangeCheck> searcher(player, scanned, check);
        Cell::VisitObjects(player, searcher, radius);
        for (GameObject* go : scanned)
            if (go && go->isSpawned() && std::find(state.entries.begin(), state.entries.end(), go->GetEntry()) != state.entries.end())
                nodes.push_back(go->GetGUID());
        return nodes;
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
        uint32 const start = state.routeIndex;
        uint32 attempts = 0;
        while (attempts < state.route.size())
        {
            RoutePoint& point = state.route[state.routeIndex];
            if (!point.visited)
            {
                auto blocked = state.blacklistedUntilMs.find(point.spawn);
                if (blocked == state.blacklistedUntilMs.end() || now >= blocked->second)
                {
                    point.visited = true;
                    spawn = point.spawn; x = point.x; y = point.y; z = point.z;
                    state.routeIndex = (state.routeIndex + 1) % state.route.size();
                    return true;
                }
            }
            state.routeIndex = (state.routeIndex + 1) % state.route.size();
            if (state.routeIndex == start)
                break;
            ++attempts;
        }
        for (RoutePoint& point : state.route)
            point.visited = false;
        state.routeIndex = 0;
        return false;
    }
}
