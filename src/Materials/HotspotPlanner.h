#ifndef SELFBOTRPG_HOTSPOT_PLANNER_H
#define SELFBOTRPG_HOTSPOT_PLANNER_H

#include "Materials/CreatureSpawnRepository.h"

#include <cstdint>
#include <vector>

class Player;

namespace Sbrpg::Materials
{
    struct Hotspot
    {
        uint32_t id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        uint32_t spawnCount = 0;
        float averageRespawnSeconds = 0.0f;
        bool reachable = false;
        std::vector<uint32_t> creatureEntries;
        std::vector<uint32_t> spawnGuids;
    };

    class HotspotPlanner
    {
    public:
        static std::vector<Hotspot> Build(std::vector<CreatureSpawn> spawns,
            float cellSize = 80.0f, float mergeRadius = 120.0f);
        static std::vector<Hotspot> Plan(Player* player, std::vector<Hotspot> hotspots);
    };
}

#endif
