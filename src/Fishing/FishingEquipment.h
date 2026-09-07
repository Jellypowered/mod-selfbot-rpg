#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
void RestoreFishingEquipment(Player* player, Sbrpg::Materials::MaterialFarmState const& state);
bool IsCatalogFishingItem(uint32 itemId);
uint32 FishingInventoryCount(Player* player);
bool HasFishingPole(Player* player);
Item* FindEquippedFishingPole(Player* player);
Item* FindFishingLure(Player* player);
}
