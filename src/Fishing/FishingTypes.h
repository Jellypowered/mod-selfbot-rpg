#pragma once
#include "Movement/RouteFollower.h"
#include "ObjectGuid.h"
#include <vector>
#include <cstddef>
namespace Sbrpg::Materials
{
    struct FishingPoolPoint
    {
        uint32_t guid = 0;
        uint32_t entry = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        bool visited = false;
    };

    // Kept in the original namespace to preserve source compatibility.
    struct FishingState
    {
        bool fishing = false;
        bool fishingByZone = false;
        bool fishingOpenWaterOnly = true;
        bool fishingPrioritizePools = false;
        bool fishingPoleEquipped = false;
        ObjectGuid fishingPreviousMainHand;
        ObjectGuid fishingPreviousOffHand;
        uint32 fishingLastCastMs = 0;
        uint32 fishingPoolLastScanMs = 0;
        uint32 fishingLastLureMs = 0;
        uint32 fishingWaterLastReportMs = 0;
        uint32 fishingLureLastReportMs = 0;
        uint32 fishingLastWaterSearchMs = 0;
        uint32 lastAddonStatusMs = 0;
        bool fishingCustomWaterSpot = false;
        uint16 fishingPoolMisses = 0;
        bool fishingPoolMode = false;
        std::vector<FishingPoolPoint> fishingPools;
        std::size_t fishingPoolIndex = 0;
        RouteStep fishingPoolStep;
    };
}
