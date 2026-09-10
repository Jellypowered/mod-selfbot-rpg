#include "Integration/RuntimeDependencies.h"
#include "Core/SbrpgLogging.h"
#include "Materials/HotspotPlanner.h"
#include "Materials/MaterialStatus.h"
#include "Materials/MaterialActivity.h"
#include "Core/SbrpgConfig.h"
#include "Safety/DangerEvaluator.h"
#include <limits>

namespace Sbrpg::Runtime
{
bool SelfbotMaterialAttackAction::MoveToNextHotspot(Sbrpg::Materials::MaterialFarmState& state)
{
            if (state.hotspots.empty())
                return false;
            uint32 const now = getMSTime();
            if (state.policyRevision != runtimeSettings.policyRevision)
            {
                state.hotspotEvidence.Clear();
                state.hotspotGeometry.clear();
                state.adaptiveCooldowns.Clear();
                state.hotspotCommittedSinceMs = 0;
                state.policyRevision = runtimeSettings.policyRevision;
            }
            state.adaptiveCooldowns.Expire(now);
            uint32 eligibleHotspots = 0;
            for (Materials::Hotspot const& candidate : state.hotspots)
                if (candidate.reachable && !state.adaptiveCooldowns.Active(candidate.id, now))
                    ++eligibleHotspots;
            uint32 const alternateHotspots = eligibleHotspots > 0 ? eligibleHotspots - 1 : 0;
            if (state.hotspotIndex >= state.hotspots.size()) state.hotspotIndex = 0;
            if (runtimeSettings.adaptiveOrdering)
            {
                std::size_t best = state.hotspotIndex;
                float bestCost = std::numeric_limits<float>::max();
                for (std::size_t i = 0; i < state.hotspots.size(); ++i)
                {
                    Materials::Hotspot const& candidate = state.hotspots[i];
                    if (!candidate.reachable || state.adaptiveCooldowns.Active(candidate.id, now)) continue;
                    float const distance = bot->GetExactDist(candidate.x, candidate.y, candidate.z);
                    float const geometry = state.hotspotGeometry.count(candidate.id) ?
                        Sbrpg::Awareness::GeometryPenalty(state.hotspotGeometry[candidate.id], alternateHotspots) : 0.0f;
                    float const cost = state.hotspotEvidence.Cost(candidate.id, distance,
                        candidate.z - bot->GetPositionZ(), now) + geometry;
                    if (cost < bestCost || (cost == bestCost && candidate.id < state.hotspots[best].id))
                        best = i, bestCost = cost;
                }
                if (bestCost == std::numeric_limits<float>::max())
                {
                    SetMaterialPhase(bot, state, "All nearby material areas are temporarily unsafe.");
                    return false;
                }
                float const currentDistance = bot->GetExactDist(state.hotspots[state.hotspotIndex].x,
                    state.hotspots[state.hotspotIndex].y, state.hotspots[state.hotspotIndex].z);
                float const currentGeometry = state.hotspotGeometry.count(state.hotspots[state.hotspotIndex].id) ?
                    Sbrpg::Awareness::GeometryPenalty(state.hotspotGeometry[state.hotspots[state.hotspotIndex].id], alternateHotspots) : 0.0f;
                float const currentCost = state.hotspotEvidence.Cost(state.hotspots[state.hotspotIndex].id,
                    currentDistance, state.hotspots[state.hotspotIndex].z - bot->GetPositionZ(), now) + currentGeometry;
                if (best != state.hotspotIndex && Sbrpg::Awareness::ShouldSwitchObjective(
                    state.hotspotCommittedSinceMs, currentCost, bestCost, now))
                {
                    Debug(bot, Acore::StringFormat("adaptive switch: hotspot {} cost {:.1f} -> {} cost {:.1f}",
                        state.hotspots[state.hotspotIndex].id, currentCost, state.hotspots[best].id, bestCost));
                    state.hotspotIndex = best;
                    state.hotspotStep = Sbrpg::RouteStep();
                    state.hotspotProgressMs = 0;
                    state.hotspotStalls = 0;
                    state.hotspotCommittedSinceMs = now;
                }
                else if (state.hotspotCommittedSinceMs == 0)
                    state.hotspotCommittedSinceMs = now;
            }
            Sbrpg::Materials::Hotspot const& hotspot = state.hotspots[state.hotspotIndex];
            if (runtimeSettings.dangerScreening && !Sbrpg::Safety::DangerEvaluator::Allows(bot, hotspot.x, hotspot.y, hotspot.z))
            {
                state.adaptiveCooldowns.Set(hotspot.id, Sbrpg::Awareness::CooldownReason::Danger, now, 120000);
                state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
                state.hotspotStep = Sbrpg::RouteStep();
                state.hotspotProgressMs = 0;
                SetMaterialPhase(bot, state, "Material area looks unsafe. Temporarily skipping it.");
                Debug(bot, Acore::StringFormat("danger hotspot rejected: {}", hotspot.id));
                return false;
            }
            float const distance = bot->GetExactDist(hotspot.x, hotspot.y, hotspot.z);
            if (distance <= 10.0f)
            {
                state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
                state.hotspotCommittedSinceMs = 0;
                state.hotspotStep = Sbrpg::RouteStep();
                state.hotspotProgressMs = 0;
                state.hotspotStalls = 0;
                SetMaterialPhase(bot, state, "Searching for a material target.");
                return true;
            }
            if (state.hotspotProgressMs == 0)
            {
                state.hotspotProgressMs = now;
                state.hotspotLastDistance = distance;
                Debug(bot, Acore::StringFormat("material hotspot objective: {} ({} spawns)",
                    hotspot.id, hotspot.spawnCount));
            }
            else if (now - state.hotspotProgressMs >= 10000)
            {
                if (distance >= state.hotspotLastDistance - 3.0f)
                    ++state.hotspotStalls;
                else
                    state.hotspotStalls = 0;
                state.hotspotProgressMs = now;
                state.hotspotLastDistance = distance;
                if (state.hotspotStalls >= 3)
                {
                    Debug(bot, Acore::StringFormat("material hotspot {} stalled; selecting next", hotspot.id));
                    if (runtimeSettings.adaptiveOrdering) state.hotspotEvidence.Observe(hotspot.id, false, now);
                    state.adaptiveCooldowns.Set(hotspot.id, Sbrpg::Awareness::CooldownReason::RouteStalled, now, 30000);
                    state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
                    state.hotspotCommittedSinceMs = 0;
                    state.hotspotStep = Sbrpg::RouteStep();
                    state.hotspotProgressMs = 0;
                    state.hotspotStalls = 0;
                    return false;
                }
            }
            if ((bot->movespline && !bot->movespline->Finalized()) || bot->isMoving())
                return false;

            Sbrpg::RouteStep step;
            if (!hotspot.reachable || !Sbrpg::BuildRouteStep(bot, hotspot.x, hotspot.y, hotspot.z, step))
            {
                Debug(bot, Acore::StringFormat("material hotspot {} has no safe mmap segment; selecting next", hotspot.id));
                if (runtimeSettings.adaptiveOrdering) state.hotspotEvidence.Observe(hotspot.id, false, now);
                state.adaptiveCooldowns.Set(hotspot.id, Sbrpg::Awareness::CooldownReason::Unreachable, now, 60000);
                state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
                state.hotspotCommittedSinceMs = 0;
                state.hotspotProgressMs = 0;
                state.hotspotStalls = 0;
                return false;
            }
            state.hotspotStep = step;
            state.hotspotStepBuiltMs = now;
            Sbrpg::Awareness::GeometryAssessment const geometry = Sbrpg::Awareness::AssessGeometry(step,
                bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
            float const geometryPenalty = Sbrpg::Awareness::GeometryPenalty(geometry, alternateHotspots);
            if (state.hotspotGeometry.size() < 256 || state.hotspotGeometry.count(hotspot.id))
                state.hotspotGeometry[hotspot.id] = geometry;
            if (runtimeSettings.debug && (state.lastAdaptiveDebugMs == 0 ||
                now - state.lastAdaptiveDebugMs >= 5000))
            {
                state.lastAdaptiveDebugMs = now;
                Debug(bot, Acore::StringFormat("adaptive candidate: run {}, hotspot {}, distance {:.1f}, geometry {:.1f} ({}), corridor {}, alternates {}, evidence {}",
                    state.session.startedMs, hotspot.id, distance, geometryPenalty, Sbrpg::Awareness::GeometryClassName(geometry.kind),
                    step.corridor.size(), alternateHotspots, state.hotspotEvidence.Size()));
            }
            if (travel.IssueBoundedMove(state.hotspotStep))
            {
                SetMaterialPhase(bot, state, "Travelling to material area.");
                return true;
            }
            SetMaterialPhase(bot, state, "Material route movement retrying.");
            return false;
        }

}
