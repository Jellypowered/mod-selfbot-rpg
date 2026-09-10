#include "Fishing/FishingController.h"
#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Core/ActivityReturn.h"
#include "Core/SbrpgConfig.h"
#include "Core/SbrpgLogging.h"
#include "Materials/MaterialLifecycle.h"
#include "Materials/MaterialStatus.h"
#include "Safety/EligibilityPolicy.h"
#include "Materials/MaterialActivity.h"

namespace Sbrpg::Runtime
{
bool SelfbotMaterialAttackAction::Execute(Event /*event*/)
{
            auto it = ActivityRegistry::Materials().find(bot->GetGUID());
            if (it == ActivityRegistry::Materials().end() || !it->second.active)
                return false;
            Sbrpg::Materials::MaterialFarmState& state = it->second;
            if (state.mapId != bot->GetMapId())
            {
                StopMaterial(bot, "map changed before return");
                return false;
            }

            uint32 const now = getMSTime();
            uint32 const actionDelayMs = runtimeSettings.actionDelayMs;
            if (state.lastActionMs != 0 && now - state.lastActionMs < actionDelayMs)
                return false;
            state.lastActionMs = now;
            if (bot->IsInCombat())
            {
                if (state.combatPauseSinceMs == 0)
                    state.combatPauseSinceMs = now;
            }
            else if (state.combatPauseSinceMs != 0)
            {
                uint32 const pausedMs = now - state.combatPauseSinceMs;
                state.adaptiveCooldowns.Pause(state.combatPauseSinceMs, pausedMs);
                if (state.targetSinceMs >= state.combatPauseSinceMs)
                    state.targetSinceMs += pausedMs;
                if (state.corpseWaitSinceMs >= state.combatPauseSinceMs)
                    state.corpseWaitSinceMs += pausedMs;
                if (state.invalidLootSinceMs >= state.combatPauseSinceMs)
                    state.invalidLootSinceMs += pausedMs;
                state.combatPauseSinceMs = 0;
            }
            UpdateMaterialReturn(bot);
            if (state.fishing)
                return FishingController(botAI).Execute(state, now);
            if (bot->IsInCombat())
            {
                SetMaterialPhase(bot, state, state.session.returnRequested ?
                    "Combat started. Return home is paused." : "Combat started. Material route paused.");
                return false;
            }

            // Stock playerbots exclusively owns corpse approach, opening,
            // looting and skinning. SBRPG only keeps the exact killed GUID in
            // its stack and yields while LootObject says that corpse remains
            // actionable. Never issue a competing MoveNear/OpenLoot request.
            LootObject activeLoot = AI_VALUE(LootObject, "loot target");
            if (!activeLoot.IsEmpty())
            {
                // Give stock loot time to complete or replace a target. An
                // empty corpse can only be discovered after opening it, so an
                // invalid target must not stall material farming forever.
                WorldObject* activeLootObject = activeLoot.GetWorldObject(bot);
                bool const activeLootInRange = activeLootObject &&
                    bot->GetDistance(activeLootObject) <= sPlayerbotAIConfig.contactDistance + 0.5f;
                if ((!activeLootInRange && activeLoot.IsLootPossible(bot)) ||
                    !bot->GetLootGUID().IsEmpty())
                {
                    state.invalidLootSinceMs = 0;
                    SetMaterialPhase(bot, state, state.session.returnRequested ?
                        "Returning home after loot finishes." : "Loot opened. Collecting items.");
                    return false;
                }
                if (state.invalidLootSinceMs == 0)
                    state.invalidLootSinceMs = now;
                if (bot->IsInCombat() || now - state.invalidLootSinceMs < 10000 ||
                    !bot->GetLootGUID().IsEmpty())
                {
                    SetMaterialPhase(bot, state, "Waiting for loot target to recover.");
                    return false;
                }
                Debug(bot, Acore::StringFormat("material releasing stale loot target after 10s: guid {}",
                    activeLoot.guid.GetCounter()));
                AI_VALUE(LootObjectStack*, "available loot")->Remove(activeLoot.guid);
                context->GetValue<LootObject>("loot target")->Set(LootObject());
                state.invalidLootSinceMs = 0;
            }
            if (!bot->GetLootGUID().IsEmpty())
            {
                SetMaterialPhase(bot, state, state.session.returnRequested ? "Returning home after loot finishes." : "Loot opened. Collecting items.");
                return false;
            }

            // Explicitly approach queued combat corpses/chests on return (and
            // during normal routing) so stock loot can select them promptly.
            if (AI_VALUE(bool, "has available loot"))
            {
                LootObject pendingLoot = AI_VALUE(LootObjectStack*, "available loot")->GetLoot();
                WorldObject* pendingObject = pendingLoot.GetWorldObject(bot);
                if (pendingObject && bot->GetDistance(pendingObject) > sPlayerbotAIConfig.contactDistance + 0.5f)
                {
                    SetMaterialPhase(bot, state, state.session.returnRequested ?
                        "Returning home after nearby loot." : "Moving to corpse to collect loot.");
                    if (!bot->isMoving() && !bot->IsNonMeleeSpellCast(true))
                        travel.MoveNearLoot(pendingObject);
                    return false;
                }
                SetMaterialPhase(bot, state, state.session.returnRequested ?
                    "Returning home after nearby loot." : "Waiting for playerbot loot to finish.");
                return false;
            }

            if (!state.target.IsEmpty())
            {
                Creature* corpse = botAI->GetCreature(state.target);
                if (corpse && corpse->getDeathState() == DeathState::Corpse)
                {
                    bool const needsLoot = corpse->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                    LootObject corpseLoot(bot, state.target);
                    // Normal corpse loot always has priority. Do not expose a
                    // harvest target to stock gather until the lootable flag is
                    // gone, otherwise a ranged bot can harvest/interrupt while
                    // regular corpse items remain.
                    SkillType const corpseSkill = static_cast<SkillType>(corpseLoot.skillId);
                    bool const harvestReady = !needsLoot &&
                        std::find(state.harvestSkills.begin(), state.harvestSkills.end(), corpseSkill) !=
                            state.harvestSkills.end() &&
                        HasHarvestTool(bot, corpseSkill) && botAI->HasSkill(corpseSkill) &&
                        bot->GetSkillValue(corpseSkill) >= corpseLoot.reqSkillValue;
                    bool const corpseActionable = needsLoot || harvestReady;
                    if (corpseActionable)
                    {
                        if (harvestReady && bot->IsMounted())
                        {
                            bot->Dismount();
                            SetMaterialPhase(bot, state, "Dismounting before gathering.");
                            Debug(bot, "corpse gathering requires dismounted form; dismounting");
                            return false;
                        }
                        if (state.corpseWaitSinceMs == 0)
                            state.corpseWaitSinceMs = now;
                        // Ranged kills commonly land outside the stock
                        // Approach all the way to contact range so the stock
                        // loot action can interact reliably, using the same
                        // bounded mmap movement as hotspot travel.
                        if (bot->GetDistance(corpse) > sPlayerbotAIConfig.contactDistance + 0.5f)
                        {
                            SetMaterialPhase(bot, state, state.session.returnRequested ?
                                "Returning home after corpse loot." : "Moving to corpse to collect loot.");
                            if (!bot->isMoving() && !bot->IsNonMeleeSpellCast(true))
                            {
                                Sbrpg::RouteStep corpseStep;
                                bool const routeBuilt = Sbrpg::BuildRouteStep(bot, corpse->GetPositionX(),
                                    corpse->GetPositionY(), corpse->GetPositionZ(), corpseStep);
                                bool const moveIssued = routeBuilt && travel.IssueBoundedMove(corpseStep);
                                if (state.lastCorpseDebugMs == 0 || now - state.lastCorpseDebugMs >= 5000)
                                {
                                    state.lastCorpseDebugMs = now;
                                    Debug(bot, Acore::StringFormat(
                                        "material corpse approach: entry {}, distance {:.1f}, route {}, path {}, points {}, move {}, lootable {}, skill {}, lootPossible {}",
                                        corpse->GetEntry(), bot->GetDistance(corpse), routeBuilt ? "yes" : "no",
                                        static_cast<uint32>(corpseStep.type), corpseStep.corridor.size(),
                                        moveIssued ? "issued" : "blocked", needsLoot ? "yes" : "no",
                                        static_cast<uint32>(corpseLoot.skillId), corpseLoot.IsLootPossible(bot) ? "yes" : "no"));
                                }
                            }
                            return false;
                        }
                        if (corpseLoot.IsLootPossible(bot))
                        {
                            bool const queued = AI_VALUE(LootObjectStack*, "available loot")->Add(state.target);
                            if (state.lastCorpseDebugMs == 0 || now - state.lastCorpseDebugMs >= 5000)
                            {
                                state.lastCorpseDebugMs = now;
                                Debug(bot, Acore::StringFormat(
                                    "material corpse loot handoff: entry {}, distance {:.1f}, queued {}, lootable {}, skill {}",
                                    corpse->GetEntry(), bot->GetDistance(corpse), queued ? "yes" : "already queued",
                                    needsLoot ? "yes" : "no", static_cast<uint32>(corpseLoot.skillId)));
                            }
                        }
                        uint32 const corpseWaitLimit = 10000;
                        if (now - state.corpseWaitSinceMs < corpseWaitLimit)
                        {
                            SetMaterialPhase(bot, state, state.session.returnRequested ?
                                "Returning home after corpse loot." : "Waiting for playerbot loot to finish.");
                            return false;
                        }
                        AI_VALUE(LootObjectStack*, "available loot")->Remove(state.target);
                        ++state.corpseTimeouts;
                        SetMaterialPhase(bot, state, "Corpse loot timed out. Continuing route.");
                        Debug(bot, Acore::StringFormat("material corpse timed out; skipping entry {}",
                            corpse->GetEntry()));
                    }
                }
                Debug(bot, Acore::StringFormat("material corpse objective cleared: guid {}, return {}",
                    state.target.GetCounter(), state.session.returnRequested ? "yes" : "no"));
                if (state.returnCombatCorpse)
                    Debug(bot, "material return combat corpse resolved; resuming return route");
                state.target.Clear();
                state.corpseWaitSinceMs = 0;
                state.lastCorpseDebugMs = 0;
                state.invalidLootSinceMs = 0;
                state.returnCombatCorpse = false;
            }

            if (state.session.returnRequested)
            {
                state.gatherStrategy.Suspend(botAI);
                // A nearby queued corpse still belongs to the loot strategy.
                // Do not start return movement until it has had a chance to
                // select and finish that corpse.
                if (AI_VALUE(bool, "has available loot"))
                {
                    SetMaterialPhase(bot, state, "Returning home after nearby loot.");
                    return false;
                }
                SetMaterialPhase(bot, state, "Returning home: " + state.session.returnReason);
                if (IsRecovering())
                    return false;
                return travel.ReturnHome(state, now);
            }
            if (IsRecovering())
            {
                SetMaterialPhase(bot, state, "Recovering before selecting a material target.");
                return false;
            }

            Creature* target = nullptr;
            if (!state.target.IsEmpty())
                target = botAI->GetCreature(state.target);
            if (target && !IsValidTarget(state, target))
                target = nullptr;
            if (!target)
            {
                state.target.Clear();
                float distance = std::numeric_limits<float>::max();
                // Stock possible-target values intentionally omit many gray
                // creatures. Scan unfriendly units directly so every live
                // eligible source entry (including gray boars/wolves) can be
                // considered without changing stock targeting globally.
                std::list<Unit*> nearbyUnits;
                Acore::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot,
                    sPlayerbotAIConfig.sightDistance);
                Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck>
                    searcher(bot, nearbyUnits, check);
                Cell::VisitObjects(bot, searcher, sPlayerbotAIConfig.sightDistance);
                for (Unit* unit : nearbyUnits)
                {
                    Creature* candidate = unit ? unit->ToCreature() : nullptr;
                    if (!IsValidTarget(state, candidate))
                        continue;
                    float const candidateDistance = bot->GetDistance(candidate);
                    if (!target || candidateDistance < distance)
                    {
                        target = candidate;
                        distance = candidateDistance;
                    }
                }
                if (!target)
                    return MoveToNextHotspot(state);
                state.target = target->GetGUID();
                state.targetSinceMs = getMSTime();
                state.corpseWaitSinceMs = 0;
                SetMaterialPhase(bot, state, Acore::StringFormat("Found {}. Engaging target.", target->GetName()));
                Debug(bot, Acore::StringFormat("material target: {} (entry {}, gray allowed)",
                    target->GetName(), target->GetEntry()));
            }

            // AttackAction::Attack performs the normal target/loot/combat handoff.
            // This call is the only deliberate gray-mob bypass in the module.
            return Attack(target);
        }

}
