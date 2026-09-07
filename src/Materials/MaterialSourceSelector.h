#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
Sbrpg::Materials::MaterialDefinition const* ResolveMaterial(std::string value, uint32& itemId);
}
