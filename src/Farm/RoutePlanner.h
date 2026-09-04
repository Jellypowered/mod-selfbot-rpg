#ifndef SELFBOTRPG_ROUTE_PLANNER_H
#define SELFBOTRPG_ROUTE_PLANNER_H

#include "SBRPG.h"

namespace Sbrpg
{
    struct RoutePlanOptions
    {
        size_t maxRefinementNodes = 96;
        uint32 maxRefinementPasses = 4;
        float minimumImprovement = 1.0f;
    };

    // Engine-free route construction. The result is deterministic for a fixed
    // input and can be tested without a map, Player, or playerbots instance.
    std::vector<RoutePoint> BuildRoutePlan(std::vector<RoutePoint> points,
                                           float startX, float startY, float startZ,
                                           RoutePlanOptions options = {});
}

#endif
