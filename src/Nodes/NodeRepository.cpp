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

}
