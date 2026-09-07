#pragma once
#include "Nodes/NodeTypes.h"
#include "Materials/MaterialFarmState.h"
#include <unordered_set>
namespace Sbrpg::Runtime
{
class ActivityRegistry
{
public:
    static std::unordered_map<ObjectGuid, FarmState>& Nodes();
    static std::unordered_map<ObjectGuid, Sbrpg::Materials::MaterialFarmState>& Materials();
    static std::unordered_set<ObjectGuid>& PendingDisable();
    static uint64_t& RunSequence();
};
}
