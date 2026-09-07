#pragma once
#include "Nodes/NodeTypes.h"
namespace Sbrpg
{
class NodeSelector
{
public:
        static bool SelectNextRoute(Player* player, FarmState& state, uint32& spawn, float& x, float& y, float& z);
};
}
