#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
std::string FormatDuration(uint64 seconds);
std::string FormatDurationMs(uint64 milliseconds);
void Debug(Player* bot, std::string const& message);
}
