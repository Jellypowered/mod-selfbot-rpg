#pragma once
#include "Core/FeatureAvailability.h"

namespace Sbrpg::Safety
{
// Future extension boundary. Not registered or used by the current runtime.
class Blacklist
{
public:
    static FeatureAvailability Availability();
};
}
