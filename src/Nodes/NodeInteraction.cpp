#include "Awareness/LiveNodeAssociation.h"
#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityLifecycle.h"
#include "Core/SbrpgConfig.h"
#include "Core/SbrpgLogging.h"
#include "Movement/MountController.h"
#include "Nodes/NodeResources.h"
#include "Nodes/NodeActivity.h"
namespace Sbrpg::Runtime
{
std::optional<bool> SelfbotRpgFarmAction::HandleLiveNodes(Sbrpg::FarmState& mutableState, uint32 now)
{
            Sbrpg::FarmState const* state = &mutableState;
            // A loaded selected node wins over the recorded route point. The
            // stock open-loot action performs the profession/lock check and
            // casts the gathering spell.
            SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "scanning live nodes");
            // Refresh the live-node cache at most twice per second. GUIDs are
            // retained, not raw pointers, so unloaded/despawned objects are safe.
            float const liveScanRadius = std::max(30.0f, sPlayerbotAIConfig.sightDistance);
            if (mutableState.liveCache.Due(now))
            {
                mutableState.liveCache.Replace(now,
                    Sbrpg::LiveNodeAssociation::ScanLive(bot, mutableState, liveScanRadius));
                Sbrpg::LiveNodeAssociation::UpdateLiveAssociations(bot, mutableState, liveScanRadius, now);
                if (runtimeSettings.debug)
                    Debug(bot, Acore::StringFormat("live node scan: {} observations, {}ms age",
                        mutableState.liveCache.Observations().size(), now - mutableState.liveCache.LastRefreshMs()));
            }
            auto setNodeObservation = [&mutableState](ObjectGuid guid, Sbrpg::NodeObservationState observation)
            {
                auto routePoint = std::find_if(mutableState.route.begin(), mutableState.route.end(),
                    [guid](Sbrpg::RoutePoint const& point) { return point.liveGuid == guid; });
                if (routePoint != mutableState.route.end())
                    routePoint->observation = observation;
            };
            if (!mutableState.activeGatherNode.IsEmpty())
            {
                setNodeObservation(mutableState.activeGatherNode, Sbrpg::NodeObservationState::Gathering);
                GameObject* activeNode = botAI->GetGameObject(mutableState.activeGatherNode);
                if (!activeNode || !activeNode->IsInWorld() || !activeNode->isSpawned())
                {
                    ++mutableState.harvested;
                    mutableState.currentSpawn = 0;
                    mutableState.pendingGatherNode = ObjectGuid::Empty;
                    mutableState.pendingGatherSinceMs = 0;
                    mutableState.activeGatherNode = ObjectGuid::Empty;
                    mutableState.gatherStartedMs = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "stock gather completed; node despawned");
                    return true;
                }
                // Do not requeue or reopen a still-spawned GO. That was
                // closing its loot window early and fighting playerbots. After
                // the first stock handoff, suppress only SBRPG's own selection;
                // the stock `gather` strategy remains free to detect/reopen a
                // multi-yield node through its normal lifecycle.
                if (now - mutableState.gatherStartedMs < 10000)
                {
                    if (now - mutableState.gatherAttemptedMs >= 5000 && !bot->IsNonMeleeSpellCast(true) &&
                        bot->GetLootGUID() != mutableState.activeGatherNode)
                    {
                        // Route points and live nodes both use the DB spawn ID.
                        // Never key this by ObjectGuid::GetCounter(): its value
                        // is not a stable route-spawn identifier on every core.
                        mutableState.blacklistedUntilMs[activeNode->GetSpawnId()] = now + 30000;
                        mutableState.activeGatherNode = ObjectGuid::Empty;
                        SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "yielding remaining loot to stock gather");
                    }
                    return false;
                }
                ObjectGuid const timedOutNode = mutableState.activeGatherNode;
                uint32 const timedOutSpawn = activeNode ? activeNode->GetSpawnId() : timedOutNode.GetCounter();
                mutableState.blacklistedUntilMs[timedOutSpawn] =
                    now + 1000 * mutableState.emptyBlacklistSeconds;
                AI_VALUE(LootObjectStack*, "available loot")->Remove(timedOutNode);
                context->GetValue<LootObject>("loot target")->Set(LootObject());
                mutableState.activeGatherNode = ObjectGuid::Empty;
                mutableState.gatherStartedMs = 0;
                mutableState.pendingGatherNode = ObjectGuid::Empty;
                mutableState.pendingGatherSinceMs = 0;
                mutableState.currentSpawn = 0;
                SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "gather timed out; replanning");
                return true;
            }
            // A selected database point is only considered empty once it is
            // inside the observed area and the live scan found no matching
            // spawned node. Unknown points remain valid discovery targets.
            if (mutableState.currentSpawn != 0)
            {
                auto current = std::find_if(mutableState.route.begin(), mutableState.route.end(),
                    [&mutableState](Sbrpg::RoutePoint const& point) { return point.spawn == mutableState.currentSpawn; });
                if (current != mutableState.route.end() &&
                    current->observation == Sbrpg::NodeObservationState::Unavailable)
                {
                    uint32 const blacklistSeconds = mutableState.emptyBlacklistSeconds;
                    mutableState.blacklistedUntilMs[current->spawn] = now + 1000 * blacklistSeconds;
                    current->observation = Sbrpg::NodeObservationState::TemporarilySkipped;
                    mutableState.currentSpawn = 0;
                    mutableState.step.valid = false;
                    mutableState.stepIssued = false;
                    mutableState.targetSinceMs = 0;
                    mutableState.stuckChecks = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "selected node absent from observed area");
                    Debug(bot, Acore::StringFormat("spawn {} absent from live scan; skipped for {} seconds",
                        current->spawn, blacklistSeconds));
                    return true;
                }
            }

            for (ObjectGuid const& guid : mutableState.liveCache.Guids())
            {
                GameObject* go = botAI->GetGameObject(guid);
                if (!go || !go->IsInWorld() || !go->isSpawned() || !HasEntry(*state, go->GetEntry()))
                    continue;
                LootObject loot(bot, go->GetGUID());
                if (!MatchesProfession(*state, loot) || !loot.IsLootPossible(bot))
                    continue;
                auto blocked = mutableState.blacklistedUntilMs.find(go->GetSpawnId());
                if (blocked != mutableState.blacklistedUntilMs.end() && now < blocked->second)
                    continue;
                bool const reroutingToLiveNode = mutableState.currentSpawn != 0;
                if (reroutingToLiveNode)
                {
                    mutableState.currentSpawn = 0;
                    mutableState.step.valid = false;
                    mutableState.stepIssued = false;
                    mutableState.targetSinceMs = 0;
                    mutableState.stuckChecks = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::ApproachingNode,
                        Acore::StringFormat("rerouting to live node (entry {}, spawn {})", go->GetEntry(), go->GetSpawnId()));
                    Debug(bot, Acore::StringFormat("rerouting from database route to live node: entry {}, spawn {}, distance {:.1f}",
                        go->GetEntry(), go->GetSpawnId(), bot->GetDistance(go)));
                }
                // Do not insert this GO into LootObjectStack ourselves.
                // Give stock gathering a bounded opportunity to claim this
                // exact GUID. A corpse loot target is not evidence that this
                // node is being handled, so it cannot leave SBRPG waiting here
                // forever after the corpse has been emptied.
                LootObject const stockTarget = AI_VALUE(LootObject, "loot target");
                bool const stockOwnsNode = stockTarget.guid == go->GetGUID() || bot->IsNonMeleeSpellCast(true);
                if (mutableState.pendingGatherNode != go->GetGUID())
                {
                    mutableState.pendingGatherNode = go->GetGUID();
                    mutableState.pendingGatherSinceMs = now;
                    mutableState.gatherReadyMs = now + 6000;
                }
                else if (now - mutableState.pendingGatherSinceMs >= 10000 &&
                    !stockOwnsNode && !bot->IsNonMeleeSpellCast(true))
                {
                    mutableState.blacklistedUntilMs[go->GetSpawnId()] = now + 30000;
                    mutableState.pendingGatherNode = ObjectGuid::Empty;
                    mutableState.pendingGatherSinceMs = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "stock gather pending timed out; skipping live node");
                    Debug(bot, Acore::StringFormat("live node gather pending timed out after 10s: spawn {}", go->GetSpawnId()));
                    continue;
                }
                if (stockOwnsNode)
                {
                    setNodeObservation(go->GetGUID(), Sbrpg::NodeObservationState::Gathering);
                    mutableState.gatherReadyMs = now + 6000;
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                        reroutingToLiveNode ? "rerouted to live node; stock gather owns it" : "stock gather owns live node");
                    return false;
                }
                if (now < mutableState.gatherReadyMs)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending,
                        reroutingToLiveNode ? "rerouting to live node; waiting for stock gather claim" : "waiting for stock gather claim");
                    return false;
                }
                if (bot->GetDistance(go) > sPlayerbotAIConfig.contactDistance + 0.5f)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending,
                        reroutingToLiveNode ? "rerouting to live node" : "approaching live gathering node");
                    if (!bot->isMoving() && !bot->IsNonMeleeSpellCast(true))
                    {
                        if (!PrepareTravelMove(bot))
                            return true;
                        MoveNear(go, sPlayerbotAIConfig.contactDistance,
                            MovementPriority::MOVEMENT_NORMAL);
                    }
                    return false;
                }
                if (mutableState.pendingGatherNode != go->GetGUID())
                {
                    mutableState.pendingGatherNode = go->GetGUID();
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending, "settling before stock gather");
                    mutableState.gatherReadyMs = now + mutableState.gatherSettleDelayMs;
                    bot->StopMoving();
                    return true;
                }
                if (now < mutableState.gatherReadyMs)
                    return true;

                // Hand the node to the stock playerbots loot/gather pipeline.
                // SBRPG never owns loot target or open-loot state.
                SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "queued in stock loot stack");
                bool const queued = AI_VALUE(LootObjectStack*, "available loot")->Add(go->GetGUID());
                if (queued)
                {
                    setNodeObservation(go->GetGUID(), Sbrpg::NodeObservationState::Gathering);
                    mutableState.activeGatherNode = go->GetGUID();
                    mutableState.gatherStartedMs = now;
                    mutableState.gatherAttemptedMs = now;
                    mutableState.gatheredItemsAtAttempt = mutableState.gatheredItems;
                    mutableState.gatherRetries = 0;
                }
                else
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "gather node already queued");
                mutableState.currentSpawn = 0;
                mutableState.pendingGatherNode = ObjectGuid::Empty;
                mutableState.pendingGatherSinceMs = 0;
                // Yield immediately so stock loot/gather can select, move to,
                // and open the node without this controller replacing its move.
                return false;
            }

    return std::nullopt;
}
}
