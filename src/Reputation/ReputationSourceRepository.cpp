#include "Reputation/ReputationSourceRepository.h"

namespace Sbrpg::Reputation
{
FeatureAvailability ReputationSourceRepository::Availability()
{
    return {false, "Not implemented; no runtime behavior enabled"};
}
}
