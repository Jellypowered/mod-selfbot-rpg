#include "Safety/SafetyObservation.h"

namespace Sbrpg::Safety
{
FeatureAvailability SafetyObservation::Availability()
{
    return {false, "Not implemented; no runtime behavior enabled"};
}
}
