#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
void RequestMaterialStop(Player* player, std::string reason = "stopped by user");
void StopMaterial(Player* player, std::string reason = "stopped");
}
