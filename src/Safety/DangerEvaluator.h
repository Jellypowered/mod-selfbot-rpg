#pragma once
#include "Core/FeatureAvailability.h"
#include <cstdint>
#include "Safety/DangerPolicy.h"
#include "Movement/Spline/MoveSplineInitArgs.h"
class Player;
namespace Sbrpg::Safety
{
enum class DangerState { Unknown, Safe, Dangerous, NeedsRecheck };
struct DangerAssessment
{
    DangerState state = DangerState::Unknown;
    uint32_t hostileCount = 0;
    int32_t maximumLevelDelta = 0;
};
// Local preemptive snapshot; never authorizes combat or replaces mmap checks.
class DangerEvaluator
{
public:
    static FeatureAvailability Availability();
    static Risk Evaluate(Player* player, float x, float y, float z);
    static bool Allows(Player* player, float x, float y, float z);
    static bool AllowsSegment(Player* player, float x, float y, float z);
    // Samples only edges supplied by a validated mmap corridor; it never
    // substitutes a direct line to the destination for the actual route.
    static bool AllowsCorridor(Player* player, Movement::PointsArray const& corridor);
};
}
