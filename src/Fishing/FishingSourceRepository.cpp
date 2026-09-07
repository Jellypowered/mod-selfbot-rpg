#include "Integration/RuntimeDependencies.h"
#include "Fishing/FishingSourceRepository.h"
#include "Fishing/LureAction.h"

namespace Sbrpg::Runtime
{
    std::vector<uint32> ResolveFishingPoolEntriesForItem(uint32 itemId)
    {
        std::vector<uint32> entries;
        QueryResult rows = WorldDatabase.Query(
            "SELECT DISTINCT gt.entry FROM gameobject_template gt "
            "JOIN fishing_loot_template fl ON fl.Entry = gt.Data1 "
            "WHERE gt.type = 25 AND fl.Item = {}", itemId);
        if (!rows)
            return entries;
        do
        {
            uint32 entry = rows->Fetch()[0].Get<uint32>();
            if (std::find(entries.begin(), entries.end(), entry) == entries.end())
                entries.push_back(entry);
        } while (rows->NextRow());
        return entries;
    }

    std::vector<uint32> ResolveFishingPoolEntriesForZone(Player* player)
    {
        std::vector<uint32> entries;
        if (!player)
            return entries;
        QueryResult rows = WorldDatabase.Query(
            "SELECT DISTINCT gt.entry FROM gameobject_template gt "
            "JOIN fishing_loot_template fl ON fl.Entry = gt.Data1 "
            "WHERE gt.type = 25");
        if (!rows)
            return entries;
        do { entries.push_back(rows->Fetch()[0].Get<uint32>()); } while (rows->NextRow());
        return entries;
    }

    std::vector<Sbrpg::Materials::FishingPoolPoint> LoadFishingPools(Player* player,
        std::vector<uint32> const& entries)
    {
        std::vector<Sbrpg::Materials::FishingPoolPoint> pools;
        if (!player || entries.empty())
            return pools;
        std::ostringstream ids;
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            if (i) ids << ',';
            ids << entries[i];
        }
        QueryResult rows = WorldDatabase.Query(
            "SELECT guid, id, position_x, position_y, position_z FROM gameobject "
            "WHERE map = {} AND id IN ({})", player->GetMapId(), ids.str());
        if (!rows)
            return pools;
        do
        {
            Field* fields = rows->Fetch();
            float x = fields[2].Get<float>(), y = fields[3].Get<float>(), z = fields[4].Get<float>();
            if (player->GetMap()->GetZoneId(player->GetPhaseMask(), x, y, z) == player->GetZoneId())
                pools.push_back({fields[0].Get<uint32>(), fields[1].Get<uint32>(), x, y, z, false});
        } while (rows->NextRow());
        return pools;
    }

}
