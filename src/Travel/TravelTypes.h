#pragma once
#include <cstdint>
namespace Sbrpg::Travel
{
// Future request model; it does not advertise or activate a capability.
struct Request
{
    uint32_t destinationMapId = 0;
};
}
