#pragma once
#include "Core/FeatureAvailability.h"

namespace Sbrpg::Questing
{
// Future extension boundary. Not registered or used by the current runtime.
class QuestRepository
{
public:
    static FeatureAvailability Availability();
};
}
