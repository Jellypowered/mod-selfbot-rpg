#ifndef SELFBOTRPG_STATUS_PUBLISHER_H
#define SELFBOTRPG_STATUS_PUBLISHER_H

#include "SBRPG.h"

#include <string>

namespace Sbrpg
{
    // Formats status at the protocol boundary. The controller owns state, not
    // addon field layout, and callers may cache by FarmState::revision.
    std::string BuildStatusFrame(FarmState const* state, uint32 nowMs);
}

#endif
