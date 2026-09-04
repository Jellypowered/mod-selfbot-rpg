#ifndef SELFBOTRPG_ROUTE_FOLLOWER_H
#define SELFBOTRPG_ROUTE_FOLLOWER_H

#include "Movement/MovementGenerators/PathGenerator.h"

class Player;

namespace Sbrpg
{
    struct RouteStep
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        bool valid = false;
        bool complete = false;
        PathType type = PATHFIND_BLANK;
        Movement::PointsArray corridor;
    };

    // Builds one safe, bounded movement segment. The caller caches the result
    // and must not rebuild it until the segment is reached or invalidated.
    bool BuildRouteStep(Player* bot, float targetX, float targetY, float targetZ, RouteStep& step);

    // Conservative same-map recovery for an off-navmesh start. It only picks a
    // nearby cardinal nudge that itself has a real mmap corridor; it never
    // teleports or accepts shortcut paths.
    bool BuildRecoveryStep(Player* bot, RouteStep& step);

    // Launches the validated PathGenerator polyline as one escort spline. The
    // caller must cache the step and launch it only once; repeatedly replacing
    // an in-flight spline is the source of visible jitter.
    bool FollowRouteStep(Player* bot, RouteStep const& step);
}

#endif
