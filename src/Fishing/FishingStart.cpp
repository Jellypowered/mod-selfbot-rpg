#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Core/SbrpgConfig.h"
#include "Fishing/FishingEquipment.h"
#include "Fishing/FishingSourceRepository.h"
#include "Fishing/FishingStart.h"
#include "Integration/PlayerbotIntegration.h"
#include "Fishing/LureAction.h"

namespace Sbrpg::Runtime
{
    bool StartFishingMaterial(Player* player, uint32 itemId, uint32 durationMinutes,
        uint32 quantityGoal, std::string* error, bool byZone,
        bool prioritizePools, bool openWaterOnly)
    {
        if (!RuntimeEnabled())
        { if (error) *error = "SBRPG is disabled by SelfBotRpg.Enable."; return false; }
        if (!player)
        { if (error) *error = "Player is unavailable."; return false; }
        if (Sbrpg::IsActive(player))
        { if (error) *error = "Stop the active node-farming session first."; return false; }
        auto existingMaterial = ActivityRegistry::Materials().find(player->GetGUID());
        if (existingMaterial != ActivityRegistry::Materials().end() && existingMaterial->second.active)
        { if (error) *error = "Stop the active material session first."; return false; }
        bool enabledBySbrpg = false;
        if (!EnsureSelfBot(player, enabledBySbrpg))
        { if (error) *error = "Unable to enable self-bot mode."; return false; }
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai->HasSkill(SKILL_FISHING))
        { DisableOwnedSelfBot(player, enabledBySbrpg); if (error) *error = "This material requires the Fishing skill."; return false; }
        if (!HasFishingPole(player))
        { DisableOwnedSelfBot(player, enabledBySbrpg); if (error) *error = "Fishing requires a fishing pole in the inventory."; return false; }

        Sbrpg::Materials::MaterialFarmState& state = ActivityRegistry::Materials()[player->GetGUID()];
        state = Sbrpg::Materials::MaterialFarmState();
        state.active = true;
        state.selfBotEnabledBySbrpg = enabledBySbrpg;
        state.fishing = true;
        state.fishingByZone = byZone;
        state.fishingPrioritizePools = prioritizePools;
        state.fishingOpenWaterOnly = openWaterOnly;
        state.itemId = itemId;
        state.quantityGoal = quantityGoal;
        state.inventoryStart = byZone ? FishingInventoryCount(player) : player->GetItemCount(itemId, true);
        state.inventoryCount = state.inventoryStart;
        state.mapId = player->GetMapId();
        state.zoneId = player->GetZoneId();
        state.phase = "fishing";
        std::vector<uint32> poolEntries = byZone ? ResolveFishingPoolEntriesForZone(player) : ResolveFishingPoolEntriesForItem(itemId);
        if (state.fishingOpenWaterOnly || !state.fishingPrioritizePools)
            poolEntries.clear();
        state.fishingPools = LoadFishingPools(player, poolEntries);
        state.fishingPoolMode = state.fishingPrioritizePools && !state.fishingOpenWaterOnly && !state.fishingPools.empty();
        Item* mainHand = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        Item* offHand = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
        if (!mainHand || !FindEquippedFishingPole(player))
        {
            state.fishingPreviousMainHand = mainHand ? mainHand->GetGUID() : ObjectGuid::Empty;
            state.fishingPreviousOffHand = offHand ? offHand->GetGUID() : ObjectGuid::Empty;
            EquipFishingPoleAction equipAction(ai);
            if (equipAction.isUseful() && equipAction.Execute(Event("sbrpg fishing equip")))
                state.fishingPoleEquipped = true;
        }
        Sbrpg::BeginActivitySession(state.session, player, durationMinutes, getMSTime());
        state.previousLootStrategy = ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Get()->GetName();
        state.lootStrategy.SetStrategy("loot");
        // Ensure stock loot exists even when selfbot was just enabled; the
        // lease records whether SBRPG added it so it can be restored on stop.
        state.lootStrategy.Acquire(ai);
        state.lootStrategy.Suspend(ai);
        ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Set(
            LootStrategyValue::instance("all"));
        state.lootStrategyOverridden = true;
        state.materialLootStrategy.SetStrategy("sbrpg material loot");
        state.materialLootStrategy.Acquire(ai);
        state.materialStrategy.SetStrategy("sbrpg material");
        state.materialStrategy.Acquire(ai);
        return true;
    }

}
