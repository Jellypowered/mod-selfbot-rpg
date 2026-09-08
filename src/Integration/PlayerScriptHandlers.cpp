#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Core/ActivityReturn.h"
#include "Core/SbrpgLogging.h"
#include "Core/SbrpgConfig.h"
#include "Fishing/FishingEquipment.h"
#include "Materials/MaterialStatus.h"
#include "Materials/MaterialLifecycle.h"
#include "Integration/PlayerScriptHandlers.h"
#include "Protocol/ResponsePublisher.h"

namespace Sbrpg::Runtime
{
    class SelfbotRpgStatusScript final : public PlayerScript
    {
    public:
        SelfbotRpgStatusScript() : PlayerScript("SelfbotRpgStatusScript", { PLAYERHOOK_ON_UPDATE, PLAYERHOOK_ON_LOGOUT }) { }

        void OnPlayerLogout(Player* player) override
        {
            if (!player)
                return;
            Sbrpg::ForceStop(player);
            ForceStopMaterial(player, "logout");
            ActivityRegistry::PendingDisable().erase(player->GetGUID());
            ActivityRegistry::Nodes().erase(player->GetGUID());
            ActivityRegistry::Materials().erase(player->GetGUID());
        }

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
                    ++materialIt->second.statusRevision;
                    if (runtimeSettings.adaptiveOrdering && materialIt->second.hotspotIndex < materialIt->second.hotspots.size())
                        materialIt->second.hotspotEvidence.Observe(materialIt->second.hotspots[materialIt->second.hotspotIndex].id, true, now);
                    Debug(player, Acore::StringFormat("fishing inventory gain: {} cataloged fish (total {})",
                        gained, materialIt->second.gatheredItems));
                    if (materialIt->second.quantityGoal != 0 &&
                        materialIt->second.gatheredItems >= materialIt->second.quantityGoal)
                        Sbrpg::RequestActivityReturn(materialIt->second.session, getMSTime(), "fish quantity goal reached");
                }
                materialIt->second.inventoryCount = current;
                bool const changed = materialIt->second.lastPublishedStatusRevision != materialIt->second.statusRevision;
                bool const longWait = now - materialIt->second.lastStatusPublishMs >= 10000;
                if (changed || longWait)
                    PublishMaterialStatus(player, materialIt->second, longWait);
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
            bool const changed = it->second.revision != it->second.lastPublishedRevision;
            bool const longWait = now - it->second.lastStatusPublishMs >= 10000;
            if (changed || longWait)
                PublishStatus(player);
        }
    };

void RegisterSelfbotRpgStatusScript() { new SelfbotRpgStatusScript(); }

}
