#include "CreatureSpawnRepository.h"

#include "DatabaseEnv.h"
#include "Map.h"
#include "Player.h"
#include "QueryResult.h"
#include "StringFormat.h"

#include <algorithm>
#include <sstream>

namespace Sbrpg::Materials
{
    std::vector<CreatureSpawn> CreatureSpawnRepository::Load(Player* player,
        std::vector<uint32_t> const& creatureEntries, bool currentZone)
    {
        std::vector<CreatureSpawn> spawns;
        if (!player || creatureEntries.empty())
            return spawns;

        std::vector<uint32_t> entries = creatureEntries;
        std::sort(entries.begin(), entries.end());
        entries.erase(std::unique(entries.begin(), entries.end()), entries.end());

        std::ostringstream ids;
        for (size_t index = 0; index < entries.size(); ++index)
        {
            if (index != 0)
                ids << ',';
            ids << entries[index];
        }

        std::string const query = Acore::StringFormat(
            "SELECT guid, id, map, zoneId, areaId, position_x, position_y, position_z, wander_distance, spawntimesecs "
            "FROM creature WHERE map = {} AND id IN ({}) ORDER BY guid",
            player->GetMapId(), ids.str());

        QueryResult result = WorldDatabase.Query(query);
        if (!result)
            return spawns;

        Map* map = player->GetMap();
        do
        {
            Field* fields = result->Fetch();
            CreatureSpawn spawn{
                fields[0].Get<uint32>(), fields[1].Get<uint32>(), fields[2].Get<uint32>(),
                fields[3].Get<uint32>(), fields[4].Get<uint32>(), fields[5].Get<float>(),
                fields[6].Get<float>(), fields[7].Get<float>(), fields[8].Get<float>(),
                fields[9].Get<uint32>()};

            // Some world databases leave creature.zoneId/areaId at zero.
            // Resolve scope from the authoritative map coordinate system.
            if (map)
                map->GetZoneAndAreaId(player->GetPhaseMask(), spawn.zoneId, spawn.areaId,
                    spawn.x, spawn.y, spawn.z);
            if (!currentZone || spawn.zoneId == player->GetZoneId())
                spawns.push_back(spawn);
        } while (result->NextRow());
        return spawns;
    }
}
