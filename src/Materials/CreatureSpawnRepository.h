#ifndef SELFBOTRPG_CREATURE_SPAWN_REPOSITORY_H
#define SELFBOTRPG_CREATURE_SPAWN_REPOSITORY_H

#include <cstdint>
#include <vector>

class Player;

namespace Sbrpg::Materials
{
    struct CreatureSpawn
    {
        uint32_t guid = 0;
        uint32_t creatureEntry = 0;
        uint32_t mapId = 0;
        uint32_t zoneId = 0;
        uint32_t areaId = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float wanderDistance = 0.0f;
        uint32_t respawnSeconds = 0;
    };

    class CreatureSpawnRepository
    {
    public:
        static std::vector<CreatureSpawn> Load(Player* player, std::vector<uint32_t> const& creatureEntries,
            bool currentZone = true);
    };
}

#endif
