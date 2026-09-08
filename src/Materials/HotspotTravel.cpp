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
                state.dangerCooldowns.clear();
                state.policyRevision = runtimeSettings.policyRevision;
            }
            for (auto it = state.dangerCooldowns.begin(); it != state.dangerCooldowns.end(); )
                if (uint32(now - it->second) >= 120000) it = state.dangerCooldowns.erase(it);
                else ++it;
            if (state.hotspotIndex >= state.hotspots.size()) state.hotspotIndex = 0;
            if (runtimeSettings.adaptiveOrdering)
            {
                std::size_t best = state.hotspotIndex;
                float bestCost = std::numeric_limits<float>::max();
                for (std::size_t i = 0; i < state.hotspots.size(); ++i)
                {
                    Materials::Hotspot const& candidate = state.hotspots[i];
                    if (!candidate.reachable || state.dangerCooldowns.count(candidate.id)) continue;
                    float const distance = bot->GetExactDist(candidate.x, candidate.y, candidate.z);
                    float const cost = state.hotspotEvidence.Cost(candidate.id, distance,
                        candidate.z - bot->GetPositionZ(), now);
                    if (cost < bestCost || (cost == bestCost && candidate.id < state.hotspots[best].id))
                        best = i, bestCost = cost;
                }
                if (bestCost == std::numeric_limits<float>::max())
                {
                    SetMaterialPhase(bot, state, "All nearby material areas are temporarily unsafe.");
                    return false;
                }
                if (best != state.hotspotIndex)
                {
                    state.hotspotIndex = best;
                    state.hotspotStep = Sbrpg::RouteStep();
                    state.hotspotProgressMs = 0;
                    state.hotspotStalls = 0;
                }
            }
            Sbrpg::Materials::Hotspot const& hotspot = state.hotspots[state.hotspotIndex];
            if (runtimeSettings.dangerScreening && !Sbrpg::Safety::DangerEvaluator::Allows(bot, hotspot.x, hotspot.y, hotspot.z))
            {
                if (state.dangerCooldowns.size() < 256) state.dangerCooldowns[hotspot.id] = now;
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
                    state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
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
                state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
                state.hotspotProgressMs = 0;
                state.hotspotStalls = 0;
                return false;
            }
            state.hotspotStep = step;
            state.hotspotStepBuiltMs = now;
            if (travel.IssueBoundedMove(state.hotspotStep))
            {
                SetMaterialPhase(bot, state, "Travelling to material area.");
                return true;
            }
            SetMaterialPhase(bot, state, "Material route movement retrying.");
            return false;
        }

}
