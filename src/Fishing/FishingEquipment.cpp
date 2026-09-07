#include "Integration/RuntimeDependencies.h"
#include "Fishing/FishingEquipment.h"
#include "Fishing/LureAction.h"

namespace Sbrpg::Runtime
{
    void RestoreFishingEquipment(Player* player, Sbrpg::Materials::MaterialFarmState const& state)
    {
        if (!player || !state.fishingPoleEquipped)
            return;
        if (!state.fishingPreviousMainHand.IsEmpty())
            if (Item* item = player->GetItemByGuid(state.fishingPreviousMainHand))
                player->SwapItem(item->GetPos(), (INVENTORY_SLOT_BAG_0 << 8) | EQUIPMENT_SLOT_MAINHAND);
        if (!state.fishingPreviousOffHand.IsEmpty())
            if (Item* item = player->GetItemByGuid(state.fishingPreviousOffHand))
                player->SwapItem(item->GetPos(), (INVENTORY_SLOT_BAG_0 << 8) | EQUIPMENT_SLOT_OFFHAND);

        // EquipFishingPoleAction uses the normal inventory/equipment swap
        // path. Mirror that path on return; do not RemoveItem/StoreItem an
        // equipped object, which can duplicate or orphan the pole.
        Item* pole = FindEquippedFishingPole(player);
        if (!pole)
            return;
        ItemPosCountVec destination;
        if (player->CanStoreItem(NULL_BAG, NULL_SLOT, destination, pole, false) != EQUIP_ERR_OK || destination.empty())
        {
            if (WorldSession* session = player->GetSession())
                ChatHandler(session).PSendSysMessage("[SBRPG] Could not return the fishing pole to inventory: no free inventory space.");
            return;
        }
        player->SwapItem(pole->GetPos(), destination.front().pos);
    }

    bool IsCatalogFishingItem(uint32 itemId)
    {
        auto const* material = Sbrpg::Materials::Find(itemId);
        return material && std::find(material->methods.begin(), material->methods.end(),
            Sbrpg::Materials::AcquisitionMethod::Fishing) != material->methods.end();
    }

    uint32 FishingInventoryCount(Player* player)
    {
        if (!player)
            return 0;
        uint32 total = 0;
        for (auto const& material : Sbrpg::Materials::Catalog())
            if (IsCatalogFishingItem(material.itemId))
                total += player->GetItemCount(material.itemId, true);
        return total;
    }

    bool HasFishingPole(Player* player)
    {
        return player && (player->HasItemCount(6256, 1) || player->HasItemCount(6365, 1) ||
            player->HasItemCount(6366, 1) || player->HasItemCount(6367, 1) ||
            player->HasItemCount(6368, 1) || player->HasItemCount(19022, 1) ||
            player->HasItemCount(19970, 1) || player->HasItemCount(44050, 1));
    }

    Item* FindEquippedFishingPole(Player* player)
    {
        if (!player)
            return nullptr;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;
            switch (item->GetEntry())
            {
                case 6256: case 6365: case 6366: case 6367: case 6368:
                case 19022: case 19970: case 44050:
                    return item;
                default: break;
            }
        }
        return nullptr;
    }

    Item* FindFishingLure(Player* player)
    {
        if (!player)
            return nullptr;
        // Prefer the strongest/current-expansion lure, then fall back to the
        // classic lures. No lure is mandatory for fishing.
        static uint32 const lureIds[] = { 46006, 34861, 6811, 6533, 6532, 6531, 6530, 6529 };
        for (uint32 lureId : lureIds)
            if (Item* lure = player->GetItemByEntry(lureId))
                return lure;
        return nullptr;
    }


}
