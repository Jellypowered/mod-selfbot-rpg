#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Core/ActivityReturn.h"
#include "Core/SbrpgLogging.h"
#include "Fishing/FishingEquipment.h"
#include "Integration/PlayerScriptHandlers.h"
#include "Protocol/ResponsePublisher.h"

namespace Sbrpg::Runtime
{
    class SelfbotRpgStatusScript final : public PlayerScript
    {
    public:
        SelfbotRpgStatusScript() : PlayerScript("SelfbotRpgStatusScript", { PLAYERHOOK_ON_UPDATE }) { }

        void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
        {
            if (player)
            {
                auto pending = ActivityRegistry::PendingDisable().find(player->GetGUID());
                bool const hasActiveNode = Sbrpg::IsActive(player);
                auto material = ActivityRegistry::Materials().find(player->GetGUID());
                bool const hasActiveMaterial = material != ActivityRegistry::Materials().end() && material->second.active;
                if (pending != ActivityRegistry::PendingDisable().end() && !hasActiveNode && !hasActiveMaterial)
                {
                    ActivityRegistry::PendingDisable().erase(pending);
                    if (IsSelfBot(player))
                        if (PlayerbotMgr* manager = GET_PLAYERBOT_MGR(player))
                            manager->HandlePlayerbotCommand("self", player);
                }
            }
            auto materialIt = player ? ActivityRegistry::Materials().find(player->GetGUID()) : ActivityRegistry::Materials().end();
            uint32 const now = getMSTime();
            if (materialIt != ActivityRegistry::Materials().end() && materialIt->second.active)
            {
                uint32 const current = materialIt->second.fishingByZone ? FishingInventoryCount(player) :
                    player->GetItemCount(materialIt->second.itemId, true);
                if (current > materialIt->second.inventoryCount)
                {
                    uint32 const gained = current - materialIt->second.inventoryCount;
                    materialIt->second.gatheredItems += gained;
                    Debug(player, Acore::StringFormat("fishing inventory gain: {} cataloged fish (total {})",
                        gained, materialIt->second.gatheredItems));
                    if (materialIt->second.quantityGoal != 0 &&
                        materialIt->second.gatheredItems >= materialIt->second.quantityGoal)
                        Sbrpg::RequestActivityReturn(materialIt->second.session, getMSTime(), "fish quantity goal reached");
                }
                materialIt->second.inventoryCount = current;
                if (now - materialIt->second.lastAddonStatusMs >= 2000)
                {
                    materialIt->second.lastAddonStatusMs = now;
                    SendAddon(player, CHAT_MSG_WHISPER, Sbrpg::Protocol::Build("MATERIAL_STATUS", {
                        "0", "1", std::to_string(materialIt->second.itemId),
                        std::to_string(materialIt->second.gatheredItems),
                        std::to_string(materialIt->second.quantityGoal),
                        std::to_string(materialIt->second.kills),
                        std::to_string(Sbrpg::ActivityRemainingSeconds(materialIt->second.session, now)),
                        materialIt->second.harvestSkills.empty() ? "0" : "1",
                        materialIt->second.phase
                    }));
                }
            }
            UpdateMaterialReturn(player);

            auto it = player ? ActivityRegistry::Nodes().find(player->GetGUID()) : ActivityRegistry::Nodes().end();
            if (it == ActivityRegistry::Nodes().end() || !it->second.active)
                return;
            if (it->second.targetItemId != 0)
            {
                uint32 const current = player->GetItemCount(it->second.targetItemId, true);
                if (current > it->second.inventoryCount)
                    it->second.gatheredItems += current - it->second.inventoryCount;
                it->second.inventoryCount = current;
                if (it->second.quantityGoal != 0 &&
                    it->second.gatheredItems >= it->second.quantityGoal)
                    Sbrpg::RequestActivityReturn(it->second.session, getMSTime(), "quantity goal reached");
            }
            if (it->second.revision != it->second.lastPublishedRevision ||
                (it->second.session.durationMs != 0 && now - it->second.lastStatusPublishMs >= 5000))
                PublishStatus(player);
        }
    };

void RegisterSelfbotRpgStatusScript() { new SelfbotRpgStatusScript(); }

}
