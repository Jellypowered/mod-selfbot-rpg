#pragma once
#include <cstdint>
namespace Sbrpg::Reputation
{
// Future request model; it does not advertise or activate a capability.
struct Request
{
    uint32_t factionId = 0;
};
}
