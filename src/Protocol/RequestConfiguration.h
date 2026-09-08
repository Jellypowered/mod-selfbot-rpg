#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
bool ConfigureFarm(Player* player, std::string const& professionText, std::string const& entries,
                       uint32 durationMinutes, uint32 quantityGoal, std::string* error);
}
