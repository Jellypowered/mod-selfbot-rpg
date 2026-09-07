#include "Integration/RuntimeDependencies.h"
#include "Core/SbrpgLogging.h"
#include "Materials/MaterialStatus.h"
#include "Materials/MaterialActivity.h"

namespace Sbrpg::Runtime
{
bool SelfbotMaterialAttackAction::MoveToNextHotspot(Sbrpg::Materials::MaterialFarmState& state)
{
            if (state.hotspots.empty())
                return false;
            if (state.hotspotIndex >= state.hotspots.size())
                state.hotspotIndex = 0;

            Sbrpg::Materials::Hotspot const& hotspot = state.hotspots[state.hotspotIndex];
            float const distance = bot->GetExactDist(hotspot.x, hotspot.y, hotspot.z);
            uint32 const now = getMSTime();
            if (distance <= 10.0f)
            {
                state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
                state.hotspotStep = Sbrpg::RouteStep();
                state.hotspotProgressMs = 0;
                state.hotspotStalls = 0;
                SetMaterialPhase(bot, state, "scanning hotspot");
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
                state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
                state.hotspotProgressMs = 0;
                state.hotspotStalls = 0;
                return false;
            }
            state.hotspotStep = step;
            state.hotspotStepBuiltMs = now;
            if (travel.IssueBoundedMove(state.hotspotStep))
            {
                SetMaterialPhase(bot, state, "travelling to hotspot");
                return true;
            }
            SetMaterialPhase(bot, state, "hotspot movement retry");
            return false;
        }

}
