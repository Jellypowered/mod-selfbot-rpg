#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Core/SbrpgLogging.h"
#include "Fishing/FishingEquipment.h"
#include "Integration/PlayerbotIntegration.h"
#include "Materials/MaterialLifecycle.h"

namespace Sbrpg::Runtime
{
    void RequestMaterialStop(Player* player, std::string reason)
    {
        if (!player)
            return;
        auto it = ActivityRegistry::Materials().find(player->GetGUID());
        if (it == ActivityRegistry::Materials().end() || !it->second.active)
            return;
        uint32 const now = getMSTime();
        Sbrpg::RequestActivityReturn(it->second.session, now, std::move(reason));
        it->second.returnNotified = true;
        // Preserve the current creature objective so combat can finish and its
        // corpse can still be looted/skinned before the return route begins.
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
            it->second.gatherStrategy.Suspend(ai);
        player->StopMoving();
        if (WorldSession* session = player->GetSession())
            ChatHandler(session).PSendSysMessage("[SBRPG] Stop requested. Returning to the session start after active loot/combat.");
        Debug(player, "material stop requested; returning to session start");
    }

    void StopMaterial(Player* player, std::string reason)
    {
        if (!player)
            return;
        auto it = ActivityRegistry::Materials().find(player->GetGUID());
        if (it == ActivityRegistry::Materials().end())
            return;
        if (WorldSession* session = player->GetSession(); session && it->second.session.startedMs != 0)
        {
            uint32 const elapsed = std::max(1u, getMSTime() - it->second.session.startedMs);
            ChatHandler(session).PSendSysMessage(
                "[SBRPG] Material summary: {}, {} kills, {} requested items, {} loot events, {} corpse timeouts ({}).",
                FormatDurationMs(elapsed), it->second.kills, it->second.gatheredItems, it->second.lootEvents,
                it->second.corpseTimeouts, reason);
        }
        RestoreFishingEquipment(player, it->second);
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        {
            if (it->second.lootStrategyOverridden)
            {
                ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Set(
                    LootStrategyValue::instance(it->second.previousLootStrategy));
                it->second.lootStrategyOverridden = false;
            }
            it->second.mountStrategy.Release(ai);
            it->second.materialStrategy.Release(ai);
            it->second.materialLootStrategy.Release(ai);
            it->second.lootStrategy.Release(ai);
            it->second.gatherStrategy.Release(ai);
            it->second.grindStrategy.Release(ai);
            it->second.rpgStrategy.Release(ai);
        }
        bool const disableSelfBot = it->second.selfBotEnabledBySbrpg;
        ActivityRegistry::Materials().erase(it);
        DisableOwnedSelfBot(player, disableSelfBot);
    }

}
