#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
void SetPhase(Sbrpg::FarmState& state, Sbrpg::FarmPhase phase, std::string reason);
}
