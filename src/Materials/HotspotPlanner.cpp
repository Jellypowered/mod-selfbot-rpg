#include "HotspotPlanner.h"

#include "Farm/RouteFollower.h"
#include "Player.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Sbrpg::Materials
{
    namespace
    {
        float Distance2d(float leftX, float leftY, float rightX, float rightY)
        {
            float const x = leftX - rightX;
            float const y = leftY - rightY;
            return std::sqrt(x * x + y * y);
        }

        void AddToHotspot(Hotspot& hotspot, CreatureSpawn const& spawn)
        {
            hotspot.spawnGuids.push_back(spawn.guid);
            if (std::find(hotspot.creatureEntries.begin(), hotspot.creatureEntries.end(), spawn.creatureEntry) ==
                hotspot.creatureEntries.end())
                hotspot.creatureEntries.push_back(spawn.creatureEntry);
            ++hotspot.spawnCount;
            hotspot.averageRespawnSeconds += static_cast<float>(spawn.respawnSeconds);
        }

        void ChooseMedoid(Hotspot& hotspot, std::vector<CreatureSpawn> const& members)
        {
            float bestCost = std::numeric_limits<float>::max();
            CreatureSpawn const* best = nullptr;
            for (CreatureSpawn const& candidate : members)
            {
                float cost = 0.0f;
                for (CreatureSpawn const& member : members)
                    cost += Distance2d(candidate.x, candidate.y, member.x, member.y);
                if (cost < bestCost || (cost == bestCost && candidate.guid < (best ? best->guid : 0)))
                {
                    bestCost = cost;
                    best = &candidate;
                }
            }
            if (best)
            {
                hotspot.x = best->x;
                hotspot.y = best->y;
                hotspot.z = best->z;
            }
            if (hotspot.spawnCount != 0)
                hotspot.averageRespawnSeconds /= hotspot.spawnCount;
            std::sort(hotspot.creatureEntries.begin(), hotspot.creatureEntries.end());
            std::sort(hotspot.spawnGuids.begin(), hotspot.spawnGuids.end());
        }
    }

    std::vector<Hotspot> HotspotPlanner::Build(std::vector<CreatureSpawn> spawns,
        float cellSize, float mergeRadius)
    {
        std::vector<Hotspot> hotspots;
        if (spawns.empty() || cellSize <= 0.0f || mergeRadius <= 0.0f)
            return hotspots;

        std::sort(spawns.begin(), spawns.end(), [](CreatureSpawn const& left, CreatureSpawn const& right)
        {
            return left.guid < right.guid;
        });

        std::vector<std::vector<CreatureSpawn>> members;
        for (CreatureSpawn const& spawn : spawns)
        {
            size_t best = members.size();
            float bestDistance = mergeRadius;
            int const spawnCellX = static_cast<int>(std::floor(spawn.x / cellSize));
            int const spawnCellY = static_cast<int>(std::floor(spawn.y / cellSize));
            for (size_t index = 0; index < members.size(); ++index)
            {
                CreatureSpawn const& anchor = members[index].front();
                int const anchorCellX = static_cast<int>(std::floor(anchor.x / cellSize));
                int const anchorCellY = static_cast<int>(std::floor(anchor.y / cellSize));
                if (std::abs(spawnCellX - anchorCellX) > 1 || std::abs(spawnCellY - anchorCellY) > 1)
                    continue;
                float const distance = Distance2d(spawn.x, spawn.y, anchor.x, anchor.y);
                if (distance <= bestDistance)
                {
                    bestDistance = distance;
                    best = index;
                }
            }
            if (best == members.size())
                members.emplace_back();
            members[best].push_back(spawn);
        }

        uint32_t hotspotId = 1;
        for (std::vector<CreatureSpawn> const& cluster : members)
        {
            Hotspot hotspot;
            hotspot.id = hotspotId++;
            for (CreatureSpawn const& spawn : cluster)
                AddToHotspot(hotspot, spawn);
            ChooseMedoid(hotspot, cluster);
            hotspots.push_back(std::move(hotspot));
        }
        return hotspots;
    }

    std::vector<Hotspot> HotspotPlanner::Plan(Player* player, std::vector<Hotspot> hotspots)
    {
        if (!player)
            return hotspots;

        for (Hotspot& hotspot : hotspots)
        {
            RouteStep step;
            hotspot.reachable = BuildRouteStep(player, hotspot.x, hotspot.y, hotspot.z, step);
        }

        std::vector<Hotspot> planned;
        planned.reserve(hotspots.size());
        float currentX = player->GetPositionX();
        float currentY = player->GetPositionY();
        while (!hotspots.empty())
        {
            auto best = hotspots.end();
            float bestDistance = std::numeric_limits<float>::max();
            for (auto it = hotspots.begin(); it != hotspots.end(); ++it)
            {
                if (!it->reachable)
                    continue;
                float const distance = Distance2d(currentX, currentY, it->x, it->y);
                if (distance < bestDistance ||
                    (distance == bestDistance && it->id < (best == hotspots.end() ? 0 : best->id)))
                {
                    bestDistance = distance;
                    best = it;
                }
            }
            if (best == hotspots.end())
                break;
            currentX = best->x;
            currentY = best->y;
            planned.push_back(*best);
            hotspots.erase(best);
        }

        // Preserve unreachable clusters for diagnostics, after reachable ones.
        for (Hotspot const& hotspot : hotspots)
            planned.push_back(hotspot);
        return planned;
    }
}
