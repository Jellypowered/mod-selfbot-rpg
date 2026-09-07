#include "RouteFollower.h"

#include "MotionMaster.h"
#include "MovementGenerator.h"
#include "Movement/Spline/MoveSplineInitArgs.h"
#include "Player.h"

#include <cmath>

namespace Sbrpg
{
    bool BuildRouteStep(Player* bot, float targetX, float targetY, float targetZ, RouteStep& step)
    {
        step = RouteStep();
        if (!bot || !bot->GetMap())
            return false;

        PathGenerator path(bot);
        path.CalculatePath(targetX, targetY, targetZ, false);
        step.type = path.GetPathType();
        if (step.type & (PATHFIND_NOPATH | PATHFIND_SHORTCUT | PATHFIND_NOT_USING_PATH | PATHFIND_FARFROMPOLY))
            return false;

        Movement::PointsArray const& points = path.GetPath();
        // Queue the complete safe corridor returned by this PathGenerator
        // probe. For PATHFIND_INCOMPLETE this is still useful forward progress;
        // the controller builds the next corridor only after reaching its end.
        // This removes artificial 45-yard boundaries and their visible pauses.
        G3D::Vector3 destination = points.empty() ? path.GetActualEndPosition() : points.back();
        if (!points.empty())
            step.corridor = points;
        step.x = destination.x;
        step.y = destination.y;
        step.z = destination.z;
        float const dx = destination.x - targetX, dy = destination.y - targetY, dz = destination.z - targetZ;
        step.complete = std::sqrt(dx * dx + dy * dy + dz * dz) <= 6.0f;
        step.valid = step.corridor.size() > 1 && bot->GetExactDist(step.x, step.y, step.z) > 2.0f;
        return step.valid || step.complete;
    }

    bool BuildRecoveryStep(Player* bot, RouteStep& step)
    {
        step = RouteStep();
        if (!bot || !bot->GetMap())
            return false;
        struct Offset { float x, y; };
        Offset const offsets[] = { { 5.0f, 0.0f }, { -5.0f, 0.0f }, { 0.0f, 5.0f }, { 0.0f, -5.0f } };
        for (Offset const& offset : offsets)
        {
            float x = bot->GetPositionX() + offset.x;
            float y = bot->GetPositionY() + offset.y;
            float z = bot->GetPositionZ();
            bot->UpdateAllowedPositionZ(x, y, z);
            PathGenerator path(bot);
            path.CalculatePath(x, y, z, false);
            PathType const type = path.GetPathType();
            if (type & (PATHFIND_NOPATH | PATHFIND_SHORTCUT | PATHFIND_NOT_USING_PATH | PATHFIND_FARFROMPOLY))
                continue;
            if (bot->GetExactDist(x, y, z) > 7.0f)
                continue;
            step.x = x; step.y = y; step.z = z;
            step.type = type;
            step.corridor = path.GetPath();
            step.valid = bot->GetExactDist(x, y, z) > 1.0f;
            return step.valid;
        }
        return false;
    }

    bool FollowRouteStep(Player* bot, RouteStep const& step)
    {
        if (!bot || step.corridor.size() < 2 || !bot->GetMotionMaster())
            return false;
        Movement::PointsArray corridor = step.corridor;
        // MoveSplinePath overwrites element zero with the live position.
        bot->GetMotionMaster()->MoveSplinePath(&corridor, FORCED_MOVEMENT_NONE);
        return bot->movespline && !bot->movespline->Finalized();
    }
}
