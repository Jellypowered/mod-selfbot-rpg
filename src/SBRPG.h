#ifndef MOD_SELFBOT_RPG_H
#define MOD_SELFBOT_RPG_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "ObjectGuid.h"

class Player;

namespace Sbrpg
{
    enum class Profession : uint8_t { Mining, Herbalism, Both };

    struct RoutePoint
    {
        uint32 spawn = 0;
        uint32 entry = 0;
        float x = 0, y = 0, z = 0;
        bool visited = false;
    };

    struct FarmState
    {
        bool active = false;
        Profession profession = Profession::Mining;
        std::vector<uint32> entries;
        uint32 harvested = 0;
        uint32 currentSpawn = 0;
        uint32 targetSinceMs = 0;
        float lastTargetDistance = 0.0f;
        uint8 stuckChecks = 0;
        std::unordered_map<uint32, uint32> blacklistedUntilMs;
        uint32 startedMs = 0;
        ObjectGuid lastGatheredNode;
        bool addedLootStrategy = false;
        uint32 zoneId = 0;
        ObjectGuid pendingGatherNode;
        ObjectGuid activeGatherNode;
        uint32 gatherReadyMs = 0;
        uint32 gatheredItems = 0;
        uint32 attemptsBeforeBlacklist = 0;
        uint32 failedBlacklistSeconds = 0;
        uint32 emptyBlacklistSeconds = 0;
        uint32 gatherSettleDelayMs = 0;
        bool stayInCurrentZone = true;
        std::vector<RoutePoint> route;
        uint32 routeIndex = 0;
        std::unordered_map<uint32, float> pathCostCache;
        uint32 lastRoutePlanMs = 0;
        uint32 lastLiveScanMs = 0;
        std::vector<ObjectGuid> liveNodes;
    };

    bool Start(Player* player, Profession profession, std::vector<uint32> entries, std::string* error);
    void Stop(Player* player);
    FarmState const* Get(Player* player);
    std::string Status(Player* player);
    bool IsActive(Player* player);
    bool SetOption(Player* player, std::string const& key, uint32 value, std::string* error);
}

#endif
