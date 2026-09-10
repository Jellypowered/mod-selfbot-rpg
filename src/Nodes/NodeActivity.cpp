#include "Nodes/NodeSelector.h"
#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityLifecycle.h"
#include "Core/ActivityRegistry.h"
#include "Core/SbrpgConfig.h"
#include "Core/SbrpgLogging.h"
#include "Movement/MountController.h"
#include "Nodes/NodeActivity.h"
#include "Safety/DangerEvaluator.h"

namespace Sbrpg::Runtime
{
bool SelfbotRpgFarmAction::Execute(Event /*event*/)
{
            Sbrpg::FarmState const* state = Sbrpg::Get(bot);
            if (!state || !state->active)
                return false;
            Sbrpg::FarmState& mutableState = ActivityRegistry::Nodes()[bot->GetGUID()];
            uint32 const actionNow = getMSTime();
            uint32 const actionDelayMs = runtimeSettings.actionDelayMs;
            if (mutableState.lastActionMs != 0 && actionNow - mutableState.lastActionMs < actionDelayMs)
                return false;
            mutableState.lastActionMs = actionNow;
            if (bot->IsInCombat())
            {
                if (mutableState.combatPauseSinceMs == 0)
                    mutableState.combatPauseSinceMs = actionNow;
                mutableState.combatInterrupted = true;
                SetPhase(mutableState, Sbrpg::FarmPhase::CombatPaused,
                    mutableState.session.returnRequested ? "Combat started. Return home is paused." : "Combat started. Route paused.");
                mutableState.step.valid = false;
                mutableState.stepIssued = false;
                return false;
            }
            if (mutableState.combatPauseSinceMs != 0)
            {
                uint32 const pausedMs = actionNow - mutableState.combatPauseSinceMs;
                for (auto& entry : mutableState.blacklistedUntilMs)
                    if (entry.second > mutableState.combatPauseSinceMs)
                        entry.second += pausedMs;
                mutableState.adaptiveCooldowns.Pause(mutableState.combatPauseSinceMs, pausedMs);
                for (auto& entry : mutableState.combatLootSinceMs)
                    if (entry.second >= mutableState.combatPauseSinceMs)
                        entry.second += pausedMs;
                if (mutableState.gatherStartedMs >= mutableState.combatPauseSinceMs)
                    mutableState.gatherStartedMs += pausedMs;
                if (mutableState.pendingGatherSinceMs >= mutableState.combatPauseSinceMs)
                    mutableState.pendingGatherSinceMs += pausedMs;
                mutableState.combatPauseSinceMs = 0;
            }
            if (mutableState.combatInterrupted)
            {
                mutableState.combatInterrupted = false;
                mutableState.step.valid = false;
                mutableState.stepIssued = false;
                mutableState.targetSinceMs = 0;
                mutableState.stuckChecks = 0;
                if (bot->isMoving())
                    bot->StopMoving();
                SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "Combat ended. Checking nearby corpse loot.");
            }
            if (bot->isDead())
            {
                SetPhase(mutableState, Sbrpg::FarmPhase::Waiting, "Character is dead. Waiting for recovery.");
                return false;
            }
            if (state->mapId != bot->GetMapId())
            {
                Debug(bot, "farm stopped after map change; route coordinates are map-local");
                Sbrpg::Finish(bot, "map changed during farming");
                return false;
            }

            uint32 const now = getMSTime();
            if (!mutableState.session.returnRequested)
            {
                bool const timedOut = Sbrpg::ActivityTimedOut(mutableState.session, now);
                bool const bagsFull = AI_VALUE(uint8, "bag space") >= 100;
                if (timedOut || bagsFull)
                {
                    Sbrpg::RequestActivityReturn(mutableState.session, now,
                        bagsFull ? "bags full" : "timed session complete");
                    mutableState.currentSpawn = 0;
                    mutableState.pendingGatherNode = ObjectGuid::Empty;
                    mutableState.pendingGatherSinceMs = 0;
                    mutableState.step.valid = false;
                    mutableState.stepIssued = false;
                    mutableState.targetSinceMs = 0;
                    mutableState.stuckChecks = 0;
                    // Stop discovering new gathering nodes during return, but
                    // keep stock loot enabled so combat corpses can finish.
                    mutableState.gatherStrategy.Suspend(botAI);
                    for (ObjectGuid const& guid : mutableState.liveCache.Guids())
                        AI_VALUE(LootObjectStack*, "available loot")->Remove(guid);
                    SetPhase(mutableState, Sbrpg::FarmPhase::Returning,
                        "Returning home: " + mutableState.session.returnReason);
                }
            }

            if (auto result = HandleLoot(mutableState, now))
                return *result;

            if (mutableState.session.returnRequested)
                return ReturnHome(mutableState, now);

            if (auto result = HandleLiveNodes(mutableState, now))
                return *result;

            uint32 spawn = mutableState.currentSpawn;
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (spawn != 0)
            {
                auto current = std::find_if(mutableState.route.begin(), mutableState.route.end(),
                    [spawn](Sbrpg::RoutePoint const& point) { return point.spawn == spawn; });
                if (current == mutableState.route.end())
                    mutableState.currentSpawn = spawn = 0;
                else
                    x = current->x, y = current->y, z = current->z;
            }
            if (spawn == 0 && !Sbrpg::NodeSelector::SelectNextRoute(bot, mutableState, spawn, x, y, z))
            {
                SetPhase(mutableState, Sbrpg::FarmPhase::Waiting, "No usable node nearby. Moving to next route point.");
                return true;
            }
            SetPhase(mutableState, Sbrpg::FarmPhase::Travelling, "Travelling to next route node.");

            // Reaching a despawned node advances the circular route rather than
            // waiting at a single spawn forever. A later pass sees its respawn.
            float const distance = bot->GetDistance(x, y, z);
            if (distance < 5.0f)
            {
                // No matching loaded object was found in the 30yd scan above.
                // We are standing at its DB spawn, therefore it is despawned
                // (or unavailable to this character). Do not oscillate between
                // empty route points while waiting for its respawn.
                uint32 const blacklistSeconds = mutableState.emptyBlacklistSeconds;
                mutableState.blacklistedUntilMs[spawn] = now + 1000 * blacklistSeconds;
                mutableState.currentSpawn = 0;
                mutableState.targetSinceMs = 0;
                mutableState.stuckChecks = 0;
                SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "Route node is empty. Temporarily skipping it.");
                Debug(bot, Acore::StringFormat("spawn {} is empty; skipping it for {} seconds", spawn, blacklistSeconds));
                return true;
            }

            // MovementAction supplies normal playerbot pathing and duplicate-move
            // suppression. We measure progress in 10s windows: three windows
            // without closing the gap means this node is unreachable, so park it
            // for one minute and continue the route instead of vibrating in place.
            if (mutableState.currentSpawn != spawn)
            {
                mutableState.currentSpawn = spawn;
                mutableState.step.valid = false;
                mutableState.stepIssued = false;
                mutableState.targetSinceMs = now;
                mutableState.committedSpawnSinceMs = now;
                mutableState.lastTargetDistance = distance;
                mutableState.stuckChecks = 0;
            }
            else if (now - mutableState.targetSinceMs >= 10000)
            {
                if (distance >= mutableState.lastTargetDistance - 3.0f)
                    ++mutableState.stuckChecks;
                else
                    mutableState.stuckChecks = 0;
                mutableState.targetSinceMs = now;
                mutableState.lastTargetDistance = distance;
                if (mutableState.stuckChecks >= mutableState.attemptsBeforeBlacklist)
                {
                    uint32 const blacklistSeconds = mutableState.failedBlacklistSeconds;
                    mutableState.blacklistedUntilMs[spawn] = now + 1000 * blacklistSeconds;
                    Debug(bot, Acore::StringFormat("spawn {} made no progress; blacklisted for {} seconds", spawn, blacklistSeconds));
                    mutableState.currentSpawn = 0;
                    mutableState.stuckChecks = 0;
                    return true;
                }
            }

            if (!mutableState.step.valid)
            {
                SetPhase(mutableState, Sbrpg::FarmPhase::BuildingPath, "Finding a safe path to the route node.");
                Sbrpg::RouteStep step;
                if (!Sbrpg::BuildRouteStep(bot, x, y, z, step))
                {
                    // Only attempt a tiny, real-corridor nudge when the bot is
                    // off the navmesh. Normal no-path legs are blacklisted;
                    // recovery must never become a shortcut around geometry.
                    if ((step.type & PATHFIND_FARFROMPOLY) && Sbrpg::BuildRecoveryStep(bot, step))
                        SetPhase(mutableState, Sbrpg::FarmPhase::Recovering, "Path recovery: returning to the safe route.");
                    else
                    {
                        mutableState.blacklistedUntilMs[spawn] = now + 1000 * mutableState.failedBlacklistSeconds;
                        mutableState.currentSpawn = 0;
                        SetPhase(mutableState, Sbrpg::FarmPhase::Failed, "No safe path to route node. Skipping it.");
                        Debug(bot, Acore::StringFormat("spawn {} has no safe navigation segment", spawn));
                        return true;
                    }
                }
                mutableState.step = std::move(step);
                mutableState.stepIssued = false;
                mutableState.stepBuiltMs = now;
                uint32 alternates = 0;
                for (Sbrpg::RoutePoint const& point : mutableState.route)
                    if (point.spawn != spawn && point.observation != Sbrpg::NodeObservationState::Unavailable &&
                        !mutableState.blacklistedUntilMs.count(point.spawn))
                        ++alternates;
                Sbrpg::Awareness::GeometryAssessment const geometry = Sbrpg::Awareness::AssessGeometry(mutableState.step,
                    bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
                float const geometryPenalty = Sbrpg::Awareness::GeometryPenalty(geometry, alternates);
                if (runtimeSettings.adaptiveOrdering &&
                    (mutableState.pathGeometryCache.size() < 256 || mutableState.pathGeometryCache.count(spawn)))
                    mutableState.pathGeometryCache[spawn] = geometry;
                if (runtimeSettings.debug && (mutableState.lastAdaptiveDebugMs == 0 ||
                    now - mutableState.lastAdaptiveDebugMs >= 5000))
                {
                    mutableState.lastAdaptiveDebugMs = now;
                    Debug(bot, Acore::StringFormat("adaptive candidate: run {}, spawn {}, distance {:.1f}, geometry {:.1f} ({}), corridor {}, alternates {}, evidence {}",
                        mutableState.runId, spawn, distance, geometryPenalty, Sbrpg::Awareness::GeometryClassName(geometry.kind),
                        mutableState.step.corridor.size(), alternates, mutableState.routeEvidence.Size()));
                }
            }

            if (bot->isMoving() || (bot->movespline && !bot->movespline->Finalized()))
                return false;
            if (bot->GetExactDist(mutableState.step.x, mutableState.step.y, mutableState.step.z) <= 2.0f)
            {
                mutableState.step.valid = false;
                mutableState.stepIssued = false;
                return true;
            }
            // A completed spline that missed its cached endpoint must be
            // rebuilt from the live position. Never replace a committed mmap
            // corridor with a fresh MoveTo endpoint while it is in progress:
            // caves and partial paths can otherwise oscillate indefinitely.
            if (mutableState.stepIssued)
            {
                mutableState.step.valid = false;
                mutableState.stepIssued = false;
                SetPhase(mutableState, Sbrpg::FarmPhase::Recovering, "Route segment ended short. Rebuilding safe path.");
                return true;
            }
            SetPhase(mutableState, Sbrpg::FarmPhase::Travelling, "Travelling to route node.");
            if (!Sbrpg::Safety::DangerEvaluator::AllowsCorridor(bot, mutableState.step.corridor))
            {
                SetPhase(mutableState, Sbrpg::FarmPhase::Waiting, "Unsafe node approach. Waiting to replan.");
                mutableState.step.valid = false;
                return false;
            }
            if (!PrepareTravelMove(bot))
                return false;
            bool const issued = Sbrpg::FollowRouteStep(bot, mutableState.step);
            mutableState.stepIssued = issued;
            // A validated PathGenerator result is not proof that its spline
            // was accepted. Never present that as travelling forever: rebuild
            // once, then blacklist the dead leg.
            if (!issued && !bot->isMoving() && now - mutableState.stepBuiltMs >= 1000)
            {
                mutableState.step.valid = false;
                if (++mutableState.stuckChecks >= mutableState.attemptsBeforeBlacklist)
                {
                    mutableState.blacklistedUntilMs[spawn] = now + 1000 * mutableState.failedBlacklistSeconds;
                    mutableState.currentSpawn = 0;
                    mutableState.stuckChecks = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::Failed, "Route movement failed. Node skipped.");
                }
                else
                    SetPhase(mutableState, Sbrpg::FarmPhase::Recovering, "Route movement retrying.");
            }
            return issued;
        }

}
