#pragma once
#include "Core/FeatureAvailability.h"
#include <cstdint>
namespace Sbrpg::Safety
{
enum class DangerState { Unknown, Safe, Dangerous, NeedsRecheck };
struct DangerAssessment
{
    DangerState state = DangerState::Unknown;
    uint32_t hostileCount = 0;
    int32_t maximumLevelDelta = 0;
};
// Unimplemented: Unknown is not permission to move. No runtime call sites yet.
class DangerEvaluator
{
public:
    static FeatureAvailability Availability();
};
}
