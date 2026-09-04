#ifndef SELFBOTRPG_MATERIAL_FARM_STATE_H
#define SELFBOTRPG_MATERIAL_FARM_STATE_H

#include "Activity/ActivityState.h"
#include "Farm/RouteFollower.h"
#include "Farm/StrategyLease.h"
#include "Materials/HotspotPlanner.h"
#include "ObjectGuid.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Sbrpg::Materials
{
    struct MaterialFarmState
    {
        bool active = false;
        ActivitySession session;
        uint32_t itemId = 0;
        uint32_t quantityGoal = 0;
        uint32_t gatheredItems = 0;
        uint32_t inventoryStart = 0;
        uint32_t inventoryCount = 0;
        uint32_t kills = 0;
        uint32_t lootEvents = 0;
        uint32_t corpseTimeouts = 0;
        uint32_t mapId = 0;
        uint32_t zoneId = 0;
        std::vector<uint32_t> creatureEntries;
        bool needsSkinning = false;
        std::vector<Hotspot> hotspots;
        std::size_t hotspotIndex = 0;
        RouteStep hotspotStep;
        uint32_t hotspotStepBuiltMs = 0;
        uint32_t hotspotProgressMs = 0;
        float hotspotLastDistance = 0.0f;
        uint8_t hotspotStalls = 0;
        ObjectGuid target;
        uint32_t targetSinceMs = 0;
        uint32_t corpseWaitSinceMs = 0;
        uint32_t lastCorpseDebugMs = 0;
        uint32_t invalidLootSinceMs = 0;
        bool returnCombatCorpse = false;
        RouteStep returnStep;
        uint32_t returnStepBuiltMs = 0;
        uint32_t returnProgressMs = 0;
        float returnLastDistance = 0.0f;
        uint8_t returnStalls = 0;
        std::string phase = "farming";
        uint32_t lastActionMs = 0;
        bool returnNotified = false;
        StrategyLease lootStrategy;
        StrategyLease materialLootStrategy;
        StrategyLease gatherStrategy;
        std::string previousLootStrategy;
        bool lootStrategyOverridden = false;
        StrategyLease materialStrategy;
        StrategyLease grindStrategy;
        StrategyLease rpgStrategy;
    };
}

#endif
