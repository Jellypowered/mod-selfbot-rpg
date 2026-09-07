#ifndef SELFBOTRPG_NODE_REPOSITORY_H
#define SELFBOTRPG_NODE_REPOSITORY_H

#include "SBRPG.h"

#include <functional>

class Player;

namespace Sbrpg
{
    // Loads database route candidates; live observations and selection are separate.
    class NodeRepository
    {
    public:
        static std::vector<RoutePoint> LoadRoute(Player* player, FarmState const& state,
            std::function<bool(uint32)> const& isGatheringEntry);
    };
}

#endif
