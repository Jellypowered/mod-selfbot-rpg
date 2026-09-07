#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
bool EnsureSelfBot(Player* player, bool& enabledBySbrpg);
void DisableOwnedSelfBot(Player* player, bool enabledBySbrpg);
}
