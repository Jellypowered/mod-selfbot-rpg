#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityLifecycle.h"
#include "Core/SbrpgLogging.h"
#include "Nodes/NodeActivity.h"
namespace Sbrpg::Runtime
{
std::optional<bool> SelfbotRpgFarmAction::HandleLoot(Sbrpg::FarmState& mutableState, uint32 now)
{
            // Stock loot always preempts farming. This covers corpses as well
            // as gathering nodes and deliberately does not inspect, clear, or
            // replace the target: playerbots must finish/release the complete
            // loot window using its normal lifecycle.
            LootObject stockLootTarget = AI_VALUE(LootObject, "loot target");
            if (!stockLootTarget.IsEmpty())
            {
                if ((stockLootTarget.skillId == SKILL_MINING ||
                     stockLootTarget.skillId == SKILL_HERBALISM) && bot->IsMounted())
                {
                    bot->Dismount();
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                        "Dismounting before gathering.");
                    return false;
                }
                if (stockLootTarget.skillId == SKILL_MINING &&
                    bot->GetShapeshiftForm() != FORM_NONE)
                {
                    botAI->RemoveShapeshift();
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                        "Leaving shapeshift before mining.");
                    return false;
                }
                WorldObject* stockLootObject = stockLootTarget.GetWorldObject(bot);
                bool const stockLootInRange = stockLootObject &&
                    bot->GetDistance(stockLootObject) <= sPlayerbotAIConfig.contactDistance + 0.5f;
                if (mutableState.lootWaitSinceMs == 0)
                {
                    mutableState.lootWaitSinceMs = now;
                    Debug(bot, Acore::StringFormat("node loot selected: guid {}, in range {}, open {}",
                        stockLootTarget.guid.GetCounter(), stockLootInRange ? "yes" : "no",
                        bot->GetLootGUID().IsEmpty() ? "no" : "yes"));
                }
                if (stockLootObject && !stockLootInRange && stockLootTarget.IsLootPossible(bot) &&
                    !bot->IsInCombat() && !bot->isMoving() && !bot->IsNonMeleeSpellCast(true))
                {
                    // Keep the stock target and loot lifecycle authoritative,
                    // but ensure a selected corpse/vein is actually approached
                    // when the stock movement action has not issued a move.
                    MoveNear(stockLootObject, sPlayerbotAIConfig.contactDistance,
                        MovementPriority::MOVEMENT_NORMAL);
                }
                if ((!stockLootInRange && stockLootTarget.IsLootPossible(bot)) ||
                    !bot->GetLootGUID().IsEmpty() || bot->IsInCombat() ||
                    now - mutableState.lootWaitSinceMs < 10000)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                        mutableState.session.returnRequested ? "Returning home after active loot finishes." : (!bot->GetLootGUID().IsEmpty() ? "Loot opened. Collecting items." : "Waiting for loot to finish."));
                    return false;
                }
                SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "Loot timed out. Continuing route.");
                Debug(bot, Acore::StringFormat("releasing stale loot target after 10s: guid {}",
                    stockLootTarget.guid.GetCounter()));
                AI_VALUE(LootObjectStack*, "available loot")->Remove(stockLootTarget.guid);
                botAI->GetAiObjectContext()->GetValue<LootObject>("loot target")->Set(LootObject());
                mutableState.lootWaitSinceMs = 0;
            }
            else
                mutableState.lootWaitSinceMs = 0;

            // Keep multiple post-combat corpses available until stock loot has
            // selected them. Expire only unselected corpses after 10 seconds;
            // an actively selected/open corpse is handled by the stock-target
            // timeout above.
            for (auto lootIt = mutableState.combatLootSinceMs.begin();
                lootIt != mutableState.combatLootSinceMs.end(); )
            {
                ObjectGuid const guid = lootIt->first;
                LootObject const selected = AI_VALUE(LootObject, "loot target");
                bool const selectedOrOpen = selected.guid == guid || bot->GetLootGUID() == guid;
                if (!selectedOrOpen && now - lootIt->second >= 10000)
                {
                    AI_VALUE(LootObjectStack*, "available loot")->Remove(guid);
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "Corpse loot timed out. Continuing route.");
                    Debug(bot, Acore::StringFormat("combat corpse loot timed out after 10s: guid {}", guid.GetCounter()));
                    lootIt = mutableState.combatLootSinceMs.erase(lootIt);
                }
                else
                    ++lootIt;
            }

            // Available loot can be a nearby chest/gameobject or a combat
            // corpse that has not yet become the selected loot target. Approach
            // it with normal bounded playerbot movement before yielding to the
            // stock strategy for opening and looting.
            if (AI_VALUE(bool, "has available loot"))
            {
                LootObject pendingLoot = AI_VALUE(LootObjectStack*, "available loot")->GetLoot();
                WorldObject* pendingObject = pendingLoot.GetWorldObject(bot);
                if (pendingObject && bot->GetDistance(pendingObject) > sPlayerbotAIConfig.contactDistance + 0.5f)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                        mutableState.session.returnRequested ? "Returning home after nearby loot." : "Moving to corpse to collect loot.");
                    if (!bot->isMoving() && !bot->IsNonMeleeSpellCast(true))
                    {
                        if (mutableState.lastLootDebugMs == 0 || now - mutableState.lastLootDebugMs >= 5000)
                        {
                            mutableState.lastLootDebugMs = now;
                            Debug(bot, Acore::StringFormat("node loot approach: guid {}, distance {:.1f}",
                                pendingLoot.guid.GetCounter(), bot->GetDistance(pendingObject)));
                        }
                        MoveNear(pendingObject, sPlayerbotAIConfig.contactDistance,
                            MovementPriority::MOVEMENT_NORMAL);
                    }
                    return false;
                }
                SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                    mutableState.session.returnRequested ? "Returning home after nearby loot." : "Waiting for playerbot loot to finish.");
                return false;
            }

    return std::nullopt;
}
}
