#pragma once
#include "Core/FeatureAvailability.h"

namespace Sbrpg::Awareness
{
// Future extension boundary. Not registered or used by the current runtime.
class LiveMaterialAwareness
{
public:
    static FeatureAvailability Availability();
};
}
