#pragma once
#include "Nodes/NodeTypes.h"
namespace Sbrpg
{
class LiveNodeAssociation
{
public:
        static std::vector<LiveNodeObservation> ScanLive(Player* player, FarmState const& state, float radius);
        static void UpdateLiveAssociations(Player* player, FarmState& state, float radius, uint32 now);
};
}
