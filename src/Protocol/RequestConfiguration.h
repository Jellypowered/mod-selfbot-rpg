#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
bool ConfigureFarm(Player* player, std::string const& professionText, std::string const& entries,
                       uint32 durationMinutes, std::string* error);
}
