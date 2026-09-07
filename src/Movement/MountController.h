#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
bool CastBestLearnedMount(Player* player, PlayerbotAI* ai);
bool PrepareTravelMove(Player* player);
}
