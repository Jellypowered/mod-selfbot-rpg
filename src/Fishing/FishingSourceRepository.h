#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
std::vector<uint32> ResolveFishingPoolEntriesForItem(uint32 itemId);
std::vector<uint32> ResolveFishingPoolEntriesForZone(Player* player);
std::vector<Sbrpg::Materials::FishingPoolPoint> LoadFishingPools(Player* player,
        std::vector<uint32> const& entries);
}
