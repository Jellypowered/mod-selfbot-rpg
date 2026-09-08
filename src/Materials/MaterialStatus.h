#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
void SetMaterialPhase(Player* player, Sbrpg::Materials::MaterialFarmState& state, std::string phase);
void PublishMaterialStatus(Player* player, Sbrpg::Materials::MaterialFarmState& state, bool heartbeat = false);
}
