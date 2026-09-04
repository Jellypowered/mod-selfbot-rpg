#ifndef MOD_SELFBOT_RPG_H
#define MOD_SELFBOT_RPG_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Activity/ActivityState.h"
#include "ObjectGuid.h"
#include "Farm/LiveNodeCache.h"
#include "Farm/RouteFollower.h"
#include "Farm/StrategyLease.h"
#include "Session/ActivitySession.h"

class Player;

namespace Sbrpg
{
    enum class Profession : uint8_t { Mining, Herbalism, Both };
    using FarmPhase = ActivityPhase;

    struct RoutePoint
    {
        uint32 spawn = 0;
        uint32 entry = 0;
        float x = 0, y = 0, z = 0;
        bool visited = false;
    };

    struct FarmState : ActivityState
    {
        Profession profession = Profession::Mining;
        std::vector<uint32> entries;
        uint32 harvested = 0;
        uint32 currentSpawn = 0;
        uint32 targetSinceMs = 0;
        float lastTargetDistance = 0.0f;
        uint8 stuckChecks = 0;
        std::unordered_map<uint32, uint32> blacklistedUntilMs;
        ObjectGuid lastGatheredNode;
        uint32 lastGatheredMs = 0;
        StrategyLease lootStrategy;
        StrategyLease gatherStrategy;
        uint32 zoneId = 0;
        uint32 mapId = 0;
        bool startedInside = false;
        ObjectGuid approachNode;
        ObjectGuid pendingGatherNode;
        ObjectGuid activeGatherNode;
        uint32 gatherReadyMs = 0;
        uint32 gatherAttemptedMs = 0;
        uint32 gatherStartedMs = 0;
        uint32 gatheredItemsAtAttempt = 0;
        uint8 gatherRetries = 0;
        uint32 gatheredItems = 0;
        std::string activity = "starting";
        uint32 attemptsBeforeBlacklist = 0;
        uint32 failedBlacklistSeconds = 0;
        uint32 emptyBlacklistSeconds = 0;
        uint32 gatherSettleDelayMs = 0;
        bool stayInCurrentZone = true;
        std::vector<RoutePoint> route;
        uint32 routeIndex = 0;
        std::unordered_map<uint32, float> pathCostCache;
        uint32 lastRoutePlanMs = 0;
        LiveNodeCache liveCache;
        RouteStep step;
        bool stepIssued = false;
        uint32 stepBuiltMs = 0;
    };

    char const* PhaseLabel(FarmPhase phase);
    void Transition(FarmState& state, FarmPhase phase, std::string reason);

    bool Start(Player* player, Profession profession, std::vector<uint32> entries,
               uint32 durationMinutes, std::string* error);
    void Finish(Player* player, std::string reason);
    void Stop(Player* player);
    FarmState const* Get(Player* player);
    std::string Status(Player* player);
    bool IsActive(Player* player);
    bool SetOption(Player* player, std::string const& key, uint32 value, std::string* error);
}

#endif
