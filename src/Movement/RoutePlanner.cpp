#include "RoutePlanner.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Sbrpg
{
    namespace
    {
        float LegDistance(RoutePoint const& a, RoutePoint const& b)
        {
            float const dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }
    }

    std::vector<RoutePoint> BuildRoutePlan(std::vector<RoutePoint> points,
                                           float startX, float startY, float startZ,
                                           RoutePlanOptions options)
    {
        std::vector<RoutePoint> ordered;
        ordered.reserve(points.size());
        while (!points.empty())
        {
            auto best = points.begin();
            float bestDistance = std::numeric_limits<float>::max();
            for (auto it = points.begin(); it != points.end(); ++it)
            {
                float const dx = it->x - startX, dy = it->y - startY, dz = it->z - startZ;
                float const distance = dx * dx + dy * dy + dz * dz;
                if (distance < bestDistance || (distance == bestDistance && it->spawn < best->spawn))
                {
                    bestDistance = distance;
                    best = it;
                }
            }
            RoutePoint next = *best;
            points.erase(best);
            ordered.push_back(next);
            startX = next.x; startY = next.y; startZ = next.z;
        }

        size_t const limit = std::min(options.maxRefinementNodes, ordered.size());
        for (uint32 pass = 0; pass < options.maxRefinementPasses && limit > 3; ++pass)
        {
            bool changed = false;
            for (size_t i = 1; i + 2 < limit; ++i)
                for (size_t j = i + 1; j + 1 < limit; ++j)
                {
                    float const before = LegDistance(ordered[i - 1], ordered[i]) + LegDistance(ordered[j], ordered[j + 1]);
                    float const after = LegDistance(ordered[i - 1], ordered[j]) + LegDistance(ordered[i], ordered[j + 1]);
                    if (after + options.minimumImprovement < before)
                    {
                        std::reverse(ordered.begin() + i, ordered.begin() + j + 1);
                        changed = true;
                    }
                }
            if (!changed) break;
        }
        return ordered;
    }
}
