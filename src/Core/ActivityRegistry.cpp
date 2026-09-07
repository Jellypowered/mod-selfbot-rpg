#include "Core/ActivityRegistry.h"
namespace Sbrpg::Runtime
{
std::unordered_map<ObjectGuid, FarmState>& ActivityRegistry::Nodes()
{ static std::unordered_map<ObjectGuid, FarmState> value; return value; }
std::unordered_map<ObjectGuid, Sbrpg::Materials::MaterialFarmState>& ActivityRegistry::Materials()
{ static std::unordered_map<ObjectGuid, Sbrpg::Materials::MaterialFarmState> value; return value; }
std::unordered_set<ObjectGuid>& ActivityRegistry::PendingDisable()
{ static std::unordered_set<ObjectGuid> value; return value; }
uint64_t& ActivityRegistry::RunSequence()
{ static uint64_t value = 0; return value; }
}
