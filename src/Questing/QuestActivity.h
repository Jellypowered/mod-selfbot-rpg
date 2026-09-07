#pragma once
#include "Core/FeatureAvailability.h"

namespace Sbrpg::Questing
{
// Future extension boundary. Not registered or used by the current runtime.
class QuestActivity
{
public:
    static FeatureAvailability Availability();
};
}
