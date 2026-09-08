#include "Awareness/LiveNodeAssociation.h"
#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityLifecycle.h"
#include "Core/SbrpgConfig.h"
#include "Core/SbrpgLogging.h"
#include "Movement/MountController.h"
#include "Nodes/NodeResources.h"
#include "Nodes/NodeActivity.h"
#include "Safety/DangerEvaluator.h"
namespace Sbrpg::Runtime
{
std::optional<bool> SelfbotRpgFarmAction::HandleLiveNodes(Sbrpg::FarmState& mutableState, uint32 now)
{
            Sbrpg::FarmState const* state = &mutableState;
            if (mutableState.policyRevision != runtimeSettings.policyRevision)
            {
                mutableState.routeEvidence.Clear();
                mutableState.dangerCooldowns.clear();
                mutableState.liveCache.Clear();
                mutableState.policyRevision = runtimeSettings.policyRevision;
            }
            // A loaded selected node wins over the recorded route point. The
            // stock open-loot action performs the profession/lock check and
            // casts the gathering spell.
            SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "Searching for nearby nodes.");
            // Refresh the live-node cache at most twice per second. GUIDs are
            // retained, not raw pointers, so unloaded/despawned objects are safe.
            float const liveScanRadius = std::max(30.0f, sPlayerbotAIConfig.sightDistance);
            bool const liveCacheRefreshed = mutableState.liveCache.Due(now);
            if (liveCacheRefreshed)
            {
                mutableState.liveCache.Replace(now,
                    Sbrpg::LiveNodeAssociation::ScanLive(bot, mutableState, liveScanRadius));
                Sbrpg::LiveNodeAssociation::UpdateLiveAssociations(bot, mutableState, liveScanRadius, now);
                uint32 const liveScanAgeMs = now - mutableState.liveCache.LastRefreshMs();
                if (runtimeSettings.debug &&
                    (!mutableState.liveCache.Observations().empty() || liveScanAgeMs != 0))
                    Debug(bot, Acore::StringFormat("live node scan: {} observations, {}ms age",
                        mutableState.liveCache.Observations().size(), liveScanAgeMs));
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
                    SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "Node closed. Checking gathered loot.");
                    return true;
                }
                // A node can yield more than once. A confirmed item callback
                // closes only SBRPG's claim window; stock playerbots still own
                // the next cast/open action. Do not blacklist a vein merely
                // because its first yield left it spawned.
                if (mutableState.gatheredItems > mutableState.gatheredItemsAtAttempt)
                {
                    mutableState.activeGatherNode = ObjectGuid::Empty;
                    mutableState.gatherStartedMs = 0;
                    mutableState.pendingGatherNode = ObjectGuid::Empty;
                    mutableState.pendingGatherSinceMs = 0;
                    mutableState.gatherReadyMs = now + 1000;
                    mutableState.gatherRetries = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "Gather complete. Checking for another yield.");
                    return false;
                }
                // Never abandon a valid stock gather claim halfway through its
                // bounded window: casts and loot packets can have a quiet gap.
                if (now - mutableState.gatherStartedMs < 10000)
                    return false;
                ObjectGuid const timedOutNode = mutableState.activeGatherNode;
                uint32 const timedOutSpawn = activeNode ? activeNode->GetSpawnId() : timedOutNode.GetCounter();
                AI_VALUE(LootObjectStack*, "available loot")->Remove(timedOutNode);
                context->GetValue<LootObject>("loot target")->Set(LootObject());
                mutableState.activeGatherNode = ObjectGuid::Empty;
                mutableState.gatherStartedMs = 0;
                mutableState.pendingGatherNode = ObjectGuid::Empty;
                mutableState.pendingGatherSinceMs = 0;
                mutableState.currentSpawn = 0;
                if (++mutableState.gatherRetries < mutableState.attemptsBeforeBlacklist)
                {
                    mutableState.gatherReadyMs = now + 1000;
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending, "Gather attempt timed out. Retrying node.");
                    Debug(bot, Acore::StringFormat("node gather timed out; retry {}/{} for spawn {}",
                        mutableState.gatherRetries, mutableState.attemptsBeforeBlacklist, timedOutSpawn));
                    return true;
                }
                mutableState.blacklistedUntilMs[timedOutSpawn] = now + 1000 * mutableState.emptyBlacklistSeconds;
                SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "Node gather timed out. Continuing route.");
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
                    SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "Route node is absent. Temporarily skipping it.");
                    Debug(bot, Acore::StringFormat("spawn {} absent from live scan; skipped for {} seconds",
                        current->spawn, blacklistSeconds));
                    return true;
                }
            }

            uint32 staleOrUnavailable = 0, professionOrLootBlocked = 0, temporarilyBlacklisted = 0;
            for (auto it = mutableState.dangerCooldowns.begin(); it != mutableState.dangerCooldowns.end(); )
                if (uint32(now - it->second) >= 120000) it = mutableState.dangerCooldowns.erase(it);
                else ++it;
            auto candidates = mutableState.liveCache.Guids();
            ObjectGuid const owned = AI_VALUE(LootObject, "loot target").guid;
            auto owner = std::find(candidates.begin(), candidates.end(), owned);
            if (owner != candidates.end()) std::rotate(candidates.begin(), owner, owner + 1);
            for (ObjectGuid const& guid : candidates)
            {
                GameObject* go = botAI->GetGameObject(guid);
                if (!go || !go->IsInWorld() || !go->isSpawned() ||
                    go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_NOT_SELECTABLE) || !HasEntry(*state, go->GetEntry()))
                {
                    ++staleOrUnavailable;
                    continue;
                }
                LootObject loot(bot, go->GetGUID());
                if (!MatchesProfession(*state, loot) || !loot.IsLootPossible(bot))
                {
                    ++professionOrLootBlocked;
                    continue;
                }
                auto blocked = mutableState.blacklistedUntilMs.find(go->GetSpawnId());
                if (blocked != mutableState.blacklistedUntilMs.end() && now < blocked->second)
                {
                    ++temporarilyBlacklisted;
                    continue;
                }
                if (runtimeSettings.dangerScreening && guid != owned)
                {
                    if (mutableState.dangerCooldowns.count(guid)) continue;
                    if (!Sbrpg::Safety::DangerEvaluator::Allows(bot, go->GetPositionX(), go->GetPositionY(), go->GetPositionZ()))
                    {
                        if (mutableState.dangerCooldowns.size() < 256) mutableState.dangerCooldowns[guid] = now;
                        SetPhase(mutableState, Sbrpg::FarmPhase::Waiting, "Nearby node looks unsafe. Temporarily skipping it.");
                        Debug(bot, Acore::StringFormat("danger node rejected: spawn {}, entry {}", go->GetSpawnId(), go->GetEntry()));
                        continue;
                    }
                }
                bool const reroutingToLiveNode = mutableState.currentSpawn != 0;
                if (reroutingToLiveNode)
                {
                    mutableState.currentSpawn = 0;
                    mutableState.step.valid = false;
                    mutableState.stepIssued = false;
                    mutableState.targetSinceMs = 0;
                    mutableState.stuckChecks = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::ApproachingNode,
                        Acore::StringFormat("Found live node (entry {}). Moving to it.", go->GetEntry()));
                    Debug(bot, Acore::StringFormat("rerouting from database route to live node: entry {}, spawn {}, distance {:.1f}",
                        go->GetEntry(), go->GetSpawnId(), bot->GetDistance(go)));
                }
                // Do not insert this GO into LootObjectStack ourselves.
                // Give stock gathering a bounded opportunity to claim this
                // exact GUID. A corpse loot target is not evidence that this
                // node is being handled, so it cannot leave SBRPG waiting here
                // forever after the corpse has been emptied.
                LootObject const stockTarget = AI_VALUE(LootObject, "loot target");
                // An unrelated buff/heal cast is not evidence that stock
                // gathering owns this node. Require the exact GUID; otherwise
                // a nearby cast can strand the vein in pending state.
                if (mutableState.pendingGatherNode != go->GetGUID())
                {
                    mutableState.pendingGatherNode = go->GetGUID();
                    mutableState.pendingGatherSinceMs = now;
                    bool const recentYield = mutableState.lastGatheredNode == go->GetGUID() &&
                        now - mutableState.lastGatheredMs <= 10000;
                    mutableState.gatherReadyMs = now + (recentYield ? 1000 : 6000);
                }
                else if (now - mutableState.pendingGatherSinceMs >= 10000 &&
                    !bot->IsNonMeleeSpellCast(true))
                {
                    mutableState.pendingGatherNode = ObjectGuid::Empty;
                    mutableState.pendingGatherSinceMs = 0;
                    if (++mutableState.gatherRetries < mutableState.attemptsBeforeBlacklist)
                    {
                        mutableState.gatherReadyMs = now + 1000;
                        SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending, "Gather action timed out. Retrying node.");
                        Debug(bot, Acore::StringFormat("live node gather pending timed out; retry {}/{} for spawn {}",
                            mutableState.gatherRetries, mutableState.attemptsBeforeBlacklist, go->GetSpawnId()));
                        continue;
                    }
                    mutableState.blacklistedUntilMs[go->GetSpawnId()] = now + 30000;
                    if (runtimeSettings.adaptiveOrdering && go->GetSpawnId())
                        mutableState.routeEvidence.Observe(go->GetSpawnId(), false, now);
                    SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "Gather action timed out. Continuing route.");
                    Debug(bot, Acore::StringFormat("live node gather pending exhausted retries: spawn {}", go->GetSpawnId()));
                    continue;
                }
                if (stockTarget.guid == go->GetGUID())
                {
                    setNodeObservation(go->GetGUID(), Sbrpg::NodeObservationState::Gathering);
                    mutableState.gatherReadyMs = now + 6000;
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                        reroutingToLiveNode ? "Gathering node." : "Gathering node.");
                    return false;
                }
                if (now < mutableState.gatherReadyMs)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending,
                        reroutingToLiveNode ? "At nearby node. Waiting for gather action." : "At node. Waiting for gather action.");
                    return false;
                }
                if (bot->GetDistance(go) > sPlayerbotAIConfig.contactDistance + 0.5f)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending,
                        reroutingToLiveNode ? Acore::StringFormat("Found live node (entry {}). Moving to it.", go->GetEntry()) : Acore::StringFormat("Approaching node (entry {}).", go->GetEntry()));
                    if (!bot->isMoving() && !bot->IsNonMeleeSpellCast(true))
                    {
                        if (!PrepareTravelMove(bot))
                            return true;
                        if (runtimeSettings.dangerScreening)
                        {
                            Sbrpg::RouteStep approach;
                            if (Sbrpg::BuildRouteStep(bot, go->GetPositionX(), go->GetPositionY(), go->GetPositionZ(), approach) &&
                                Sbrpg::Safety::DangerEvaluator::AllowsSegment(bot, approach.x, approach.y, approach.z))
                                MoveTo(bot->GetMapId(), approach.x, approach.y, approach.z,
                                    false, false, false, false, MovementPriority::MOVEMENT_NORMAL, true);
                        }
                        else MoveNear(go, sPlayerbotAIConfig.contactDistance,
                            MovementPriority::MOVEMENT_NORMAL);
                    }
                    return false;
                }
                if (mutableState.pendingGatherNode != go->GetGUID())
                {
                    mutableState.pendingGatherNode = go->GetGUID();
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending, "At node. Waiting for gather action.");
                    mutableState.gatherReadyMs = now + mutableState.gatherSettleDelayMs;
                    bot->StopMoving();
                    return true;
                }
                if (now < mutableState.gatherReadyMs)
                    return true;

                // Hand the node to the stock playerbots loot/gather pipeline.
                // SBRPG never owns loot target or open-loot state.
                SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "At node. Waiting for gather action.");
                bool const queued = AI_VALUE(LootObjectStack*, "available loot")->Add(go->GetGUID());
                if (queued)
                {
                    setNodeObservation(go->GetGUID(), Sbrpg::NodeObservationState::Gathering);
                    mutableState.activeGatherNode = go->GetGUID();
                    mutableState.gatherStartedMs = now;
                    mutableState.gatherAttemptedMs = now;
                    mutableState.gatheredItemsAtAttempt = mutableState.gatheredItems;
                }
                else
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "Gathering node.");
                mutableState.currentSpawn = 0;
                mutableState.pendingGatherNode = ObjectGuid::Empty;
                mutableState.pendingGatherSinceMs = 0;
                // Yield immediately so stock loot/gather can select, move to,
                // and open the node without this controller replacing its move.
                return false;
            }
            // Report only once per cache refresh. These counters distinguish a
            // true discovery gap from expected stock eligibility or cooldown
            // rejection without adding a chat/status stream.
            if (liveCacheRefreshed && runtimeSettings.debug && !mutableState.liveCache.Observations().empty())
                Debug(bot, Acore::StringFormat("live node scan found no eligible node: {} stale/unavailable, {} profession-or-loot blocked, {} temporarily blacklisted",
                    staleOrUnavailable, professionOrLootBlocked, temporarilyBlacklisted));

    return std::nullopt;
}
}
