#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityLifecycle.h"
#include "Movement/MountController.h"
#include "Nodes/NodeActivity.h"
#include "Safety/DangerEvaluator.h"

namespace Sbrpg::Runtime
{
bool SelfbotRpgFarmAction::ReturnHome(Sbrpg::FarmState& state, uint32 now)
{
            float const distance = bot->GetExactDist(state.session.startX, state.session.startY, state.session.startZ);
            if (Sbrpg::IsAtActivityStart(state.session, bot, 5.0f))
            {
                bot->SetFacingTo(state.session.startO);
                Sbrpg::Finish(bot, "Returned home: " + state.session.returnReason);
                return true;
            }

            SetPhase(state, Sbrpg::FarmPhase::Returning, "Returning home: " + state.session.returnReason);
            if (state.targetSinceMs == 0)
            {
                state.targetSinceMs = now;
                state.lastTargetDistance = distance;
            }
            else if (now - state.targetSinceMs >= 10000)
            {
                if (distance >= state.lastTargetDistance - 3.0f)
                    ++state.stuckChecks;
                else
                    state.stuckChecks = 0;
                state.targetSinceMs = now;
                state.lastTargetDistance = distance;
                if (state.stuckChecks >= state.attemptsBeforeBlacklist)
                {
                    state.step.valid = false;
                    state.stepIssued = false;
                    state.stuckChecks = 0;
                    SetPhase(state, Sbrpg::FarmPhase::Recovering, "Return route stalled. Retrying safe path.");
                }
            }

            if (!state.step.valid)
            {
                Sbrpg::RouteStep step;
                if (!Sbrpg::BuildRouteStep(bot, state.session.startX, state.session.startY, state.session.startZ, step))
                {
                    if (!((step.type & PATHFIND_FARFROMPOLY) && Sbrpg::BuildRecoveryStep(bot, step)))
                    {
                        SetPhase(state, Sbrpg::FarmPhase::Recovering, "No safe route home yet. Retrying.");
                        return false;
                    }
                }
                state.step = std::move(step);
                state.stepIssued = false;
                state.stepBuiltMs = now;
            }

            if (bot->isMoving() || (bot->movespline && !bot->movespline->Finalized()))
                return false;
            if (bot->GetExactDist(state.step.x, state.step.y, state.step.z) <= 2.0f)
            {
                state.step.valid = false;
                state.stepIssued = false;
                return true;
            }
            if (state.stepIssued)
            {
                state.step.valid = false;
                state.stepIssued = false;
                SetPhase(state, Sbrpg::FarmPhase::Recovering, "Return segment ended short. Rebuilding safe path.");
                return true;
            }
            if (!Sbrpg::Safety::DangerEvaluator::AllowsCorridor(bot, state.step.corridor))
            {
                SetPhase(state, Sbrpg::FarmPhase::Waiting, "danger on return route; movement withheld");
                state.step.valid = false;
                return false;
            }
            if (!PrepareTravelMove(bot))
                return false;
            bool const issued = Sbrpg::FollowRouteStep(bot, state.step);
            state.stepIssued = issued;
            if (!issued && !bot->isMoving() && now - state.stepBuiltMs >= 1000)
            {
                state.step.valid = false;
                state.stepIssued = false;
                SetPhase(state, Sbrpg::FarmPhase::Recovering, "Return movement retrying.");
            }
            return issued;
        }

}
