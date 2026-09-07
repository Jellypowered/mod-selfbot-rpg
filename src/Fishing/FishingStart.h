#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
bool StartFishingMaterial(Player* player, uint32 itemId, uint32 durationMinutes,
        uint32 quantityGoal, std::string* error, bool byZone = false,
        bool prioritizePools = false, bool openWaterOnly = true);
}
