#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityLifecycle.h"

namespace Sbrpg::Runtime
{
    void SetPhase(Sbrpg::FarmState& state, Sbrpg::FarmPhase phase, std::string reason)
    {
        Sbrpg::Transition(state, phase, std::move(reason));
    }

}
