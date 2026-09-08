#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Core/SbrpgLogging.h"
#include "Core/SbrpgConfig.h"
#include "Loot/LootEvents.h"
#include "Nodes/NodeResources.h"

namespace Sbrpg::Runtime
{
    class SelfbotRpgLootScript : public PlayerScript
    {
    public:
        SelfbotRpgLootScript() : PlayerScript("SelfbotRpgLootScript", {
            PLAYERHOOK_ON_LOOT_ITEM, PLAYERHOOK_ON_CREATURE_KILL,
            PLAYERHOOK_ON_CREATURE_KILLED_BY_PET }) { }
        void OnPlayerLootItem(Player* player, Item* /*item*/, uint32 count, ObjectGuid lootGuid) override
        {
            auto materialIt = player ? ActivityRegistry::Materials().find(player->GetGUID()) : ActivityRegistry::Materials().end();
            // AzerothCore passes a temporary loot-item buffer here rather than
            // a fully initialized inventory Item. Do not dereference `item` in
            // this hook; the crash report showed GetUInt32Value(index 3) from
            // that invalid representation. Exact material quantity accounting
            // must use a later inventory-safe observation point.
            if (materialIt != ActivityRegistry::Materials().end() && materialIt->second.active &&
                lootGuid.IsCreature())
            {
                ++materialIt->second.lootEvents;
                Debug(player, Acore::StringFormat("material loot event: creature loot count {}", count));
            }

            auto it = player ? ActivityRegistry::Nodes().find(player->GetGUID()) : ActivityRegistry::Nodes().end();
            if (it != ActivityRegistry::Nodes().end() && it->second.active && lootGuid.IsCreature())
                it->second.combatLootSinceMs.erase(lootGuid);
            if (it == ActivityRegistry::Nodes().end() || !it->second.active || !lootGuid.IsGameObject() ||
                !HasEntry(it->second, lootGuid.GetEntry()))
                return;

            uint32 const now = getMSTime();
            // One gathering window may emit several item callbacks. Count all
            // quantities, but count the node once. The same DB spawn may be
            // harvested again after respawn, so the GUID dedupe has a short
            // transaction window rather than lasting for the whole run.
            it->second.gatheredItems += count;
            if (runtimeSettings.adaptiveOrdering)
            {
                if (GameObject* node = player->GetMap() ? player->GetMap()->GetGameObject(lootGuid) : nullptr)
                {
                    uint32 const spawn = node->GetSpawnId();
                    if (spawn) it->second.routeEvidence.Observe(spawn, true, now);
                }
            }
            if (it->second.lastGatheredNode != lootGuid || now - it->second.lastGatheredMs >= 10000)
            {
                ++it->second.harvested;
                it->second.lastGatheredNode = lootGuid;
                Debug(player, Acore::StringFormat("node gather item received: entry {}, count {}", lootGuid.GetEntry(), count));
                Sbrpg::Transition(it->second, Sbrpg::FarmPhase::Looting, "Gather complete. Collecting loot.");
            }
            it->second.lastGatheredMs = now;
        }

        void OnPlayerCreatureKill(Player* player, Creature* killed) override
        {
            RecordMaterialKill(player, killed);
        }

        void OnPlayerCreatureKilledByPet(Player* player, Creature* killed) override
        {
            RecordMaterialKill(player, killed);
        }

    private:
        static void RecordMaterialKill(Player* player, Creature* killed)
        {
            if (!player || !killed)
                return;
            auto nodeIt = ActivityRegistry::Nodes().find(player->GetGUID());
            if (nodeIt != ActivityRegistry::Nodes().end() && nodeIt->second.active)
            {
                // Keep corpse discovery local to the active node run, but let
                // the stock loot strategy retain ownership of opening and
                // looting. Add() is idempotent when stock already queued it.
                if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
                    ai->GetAiObjectContext()->GetValue<LootObjectStack*>("available loot")->Get()->Add(killed->GetGUID());
            }
            auto it = ActivityRegistry::Materials().find(player->GetGUID());
            if (it == ActivityRegistry::Materials().end() || !it->second.active)
                return;
            if (std::find(it->second.creatureEntries.begin(), it->second.creatureEntries.end(),
                killed->GetEntry()) == it->second.creatureEntries.end())
                return;
            ++it->second.kills;
            it->second.target = killed->GetGUID();
            it->second.corpseWaitSinceMs = getMSTime();
            it->second.returnCombatCorpse = it->second.session.returnRequested;
            it->second.phase = it->second.session.returnRequested ?
                "combat corpse pending before return" : "waiting for stock loot";
            if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
                ai->GetAiObjectContext()->GetValue<LootObjectStack*>("available loot")->Get()->Add(killed->GetGUID());
        }
    };

void RegisterSelfbotRpgLootScript() { new SelfbotRpgLootScript(); }

}
