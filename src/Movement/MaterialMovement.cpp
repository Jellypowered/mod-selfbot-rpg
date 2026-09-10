#include "Integration/RuntimeDependencies.h"
#include "Core/SbrpgLogging.h"
#include "Materials/MaterialLifecycle.h"
#include "Materials/MaterialStatus.h"
#include "Movement/TravelMovement.h"
#include "Safety/DangerEvaluator.h"

namespace Sbrpg::Runtime
{
bool SelfbotMaterialTravel::IssueBoundedMove(Sbrpg::RouteStep const& step)
{
            // Follow the exact mmap corridor that was validated by
            // BuildRouteStep. Re-requesting MoveTo for an intermediate point
            // can replace a cave/partial-path spline with a conflicting route.
            if (!Sbrpg::Safety::DangerEvaluator::AllowsCorridor(bot, step.corridor))
                return false;
            return Sbrpg::FollowRouteStep(bot, step);
        }

bool SelfbotMaterialTravel::ReturnHome(Sbrpg::Materials::MaterialFarmState& state, uint32 now)
{
            float const distance = bot->GetExactDist(
                state.session.startX, state.session.startY, state.session.startZ);
            if (Sbrpg::IsAtActivityStart(state.session, bot, 8.0f))
            {
                bot->SetFacingTo(state.session.startO);
                if (WorldSession* session = bot->GetSession())
                    ChatHandler(session).PSendSysMessage("[SBRPG] Material run returned to its start position.");
                Debug(bot, "material run finished at activity start");
                StopMaterial(bot, state.session.returnReason.empty() ? "completed" : state.session.returnReason);
                return true;
            }

            if (state.returnProgressMs == 0)
            {
                state.returnProgressMs = now;
                state.returnLastDistance = distance;
            }
            else if (now - state.returnProgressMs >= 10000)
            {
                if (distance >= state.returnLastDistance - 3.0f)
                    ++state.returnStalls;
                else
                    state.returnStalls = 0;
                state.returnProgressMs = now;
                state.returnLastDistance = distance;
                if (state.returnStalls >= 3)
                {
                    state.returnStep = Sbrpg::RouteStep();
                    state.returnStalls = 0;
                    Debug(bot, "material return made no progress; rebuilding mmap route");
                }
            }
            if ((bot->movespline && !bot->movespline->Finalized()) || bot->isMoving())
                return false;

            Sbrpg::RouteStep step;
            if (!Sbrpg::BuildRouteStep(bot, state.session.startX, state.session.startY,
                state.session.startZ, step))
            {
                if (!((step.type & PATHFIND_FARFROMPOLY) && Sbrpg::BuildRecoveryStep(bot, step)))
                {
                    SetMaterialPhase(bot, state, "return route retry");
                    Debug(bot, "material return has no safe mmap segment yet; retrying");
                    return false;
                }
            }
            state.returnStep = step;
            state.returnStepBuiltMs = now;
            // RouteStep::complete means mmap reached the requested destination;
            // it does not mean the player has arrived. Always walk the corridor.
            if (IssueBoundedMove(state.returnStep))
            {
                SetMaterialPhase(bot, state, "returning");
                Debug(bot, Acore::StringFormat("material return moving; {:.1f} yards remain", distance));
                return true;
            }
            SetMaterialPhase(bot, state, "return movement retry");
            return false;
        }

}
