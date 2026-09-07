#pragma once
#include <cstdint>
namespace Sbrpg::Questing
{
// Future request model; it does not advertise or activate a capability.
struct Request
{
    uint32_t questId = 0;
};
}
