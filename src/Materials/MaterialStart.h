#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
bool StartMaterial(Player* player, std::string materialName, uint32 durationMinutes,
                       uint32 quantityGoal, std::string* error);
}
