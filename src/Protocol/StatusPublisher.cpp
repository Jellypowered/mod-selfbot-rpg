#include "StatusPublisher.h"

#include "Protocol/SbrpgProtocol.h"
#include "StringFormat.h"

#include <algorithm>

namespace Sbrpg
{
    std::string BuildStatusFrame(FarmState const* state, uint32 nowMs)
    {
        if (!state)
            return Protocol::Build("STATUS", { "0", "stopped", "", "0", "0", "0", "0", "0", "0", "0", "0", "0" });
        uint32 const elapsed = std::max(1u, nowMs - state->session.startedMs);
        uint32 const remaining = ActivityRemainingSeconds(state->session, nowMs);
        double const perMinute = state->gatheredItems * 60000.0 / elapsed;
        return Protocol::Build("STATUS", {
            state->active ? "1" : "0", PhaseLabel(state->phase), state->lastReason,
            state->profession == Profession::Mining ? "mining" :
            state->profession == Profession::Herbalism ? "herbalism" : "mining and herbalism",
            std::to_string(state->route.size()), std::to_string(state->harvested),
            std::to_string(state->gatheredItems), Acore::StringFormat("{:.2f}", perMinute),
            Acore::StringFormat("{:.3f}", perMinute / 60.0), std::to_string(state->currentSpawn),
            std::to_string(state->session.durationMs / 1000), std::to_string(remaining) });
    }
}
