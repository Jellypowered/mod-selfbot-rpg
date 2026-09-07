#pragma once
#include "Core/FeatureAvailability.h"

namespace Sbrpg::Reputation
{
// Future extension boundary. Not registered or used by the current runtime.
class ReputationSourceRepository
{
public:
    static FeatureAvailability Availability();
};
}
