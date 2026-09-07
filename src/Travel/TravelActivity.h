#pragma once
#include "Core/FeatureAvailability.h"

namespace Sbrpg::Travel
{
// Future extension boundary. Not registered or used by the current runtime.
class TravelActivity
{
public:
    static FeatureAvailability Availability();
};
}
