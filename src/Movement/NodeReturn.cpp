#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityLifecycle.h"
#include "Movement/MountController.h"
#include "Nodes/NodeActivity.h"

namespace Sbrpg::Runtime
{
bool SelfbotRpgFarmAction::ReturnHome(Sbrpg::FarmState& state, uint32 now)
{
            float const distance = bot->GetExactDist(state.session.startX, state.session.startY, state.session.startZ);
            if (Sbrpg::IsAtActivityStart(state.session, bot, 5.0f))
            {
                bot->SetFacingTo(state.session.startO);
                Sbrpg::Finish(bot, state.session.returnReason + "; returned to farm start");
                return true;
            }

            SetPhase(state, Sbrpg::FarmPhase::Returning, state.session.returnReason + "; returning to farm start");
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
                    SetPhase(state, Sbrpg::FarmPhase::Recovering, "return-home route stalled; rebuilding");
                }
            }

            if (!state.step.valid)
            {
                Sbrpg::RouteStep step;
                if (!Sbrpg::BuildRouteStep(bot, state.session.startX, state.session.startY, state.session.startZ, step))
                {
                    if (!((step.type & PATHFIND_FARFROMPOLY) && Sbrpg::BuildRecoveryStep(bot, step)))
                    {
                        SetPhase(state, Sbrpg::FarmPhase::Recovering, "no safe mmap route home yet; retrying");
                        return false;
                    }
                }
                state.step = std::move(step);
                state.stepIssued = false;
                state.stepBuiltMs = now;
            }

            if (bot->GetExactDist(state.step.x, state.step.y, state.step.z) <= 2.0f)
            {
                state.step.valid = false;
                state.stepIssued = false;
                return true;
            }
            if (!PrepareTravelMove(bot))
                return false;
            bool const issued = MoveTo(bot->GetMapId(), state.step.x, state.step.y, state.step.z,
                                       false, false, false, false, MovementPriority::MOVEMENT_NORMAL, true);
            if (!issued && !bot->isMoving() && now - state.stepBuiltMs >= 1000)
            {
                state.step.valid = false;
                state.stepIssued = false;
                SetPhase(state, Sbrpg::FarmPhase::Recovering, "return-home movement was not issued; rebuilding");
            }
            return issued;
        }

}
