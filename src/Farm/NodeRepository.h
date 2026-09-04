#ifndef SELFBOTRPG_NODE_REPOSITORY_H
#define SELFBOTRPG_NODE_REPOSITORY_H

#include "SBRPG.h"

#include <functional>

class Player;

namespace Sbrpg
{
    // Owns database route materialization and safe live-object GUID scanning.
    // It deliberately returns GUIDs rather than object pointers: grid unloads
    // and despawns between controller ticks are normal.
    class NodeRepository
    {
    public:
        static std::vector<RoutePoint> LoadRoute(Player* player, FarmState const& state,
            std::function<bool(uint32)> const& isGatheringEntry);
        static std::vector<ObjectGuid> ScanLive(Player* player, FarmState const& state, float radius);
        static bool SelectNextRoute(Player* player, FarmState& state, uint32& spawn, float& x, float& y, float& z);
    };
}

#endif
