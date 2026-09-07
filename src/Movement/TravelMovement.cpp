#include "Integration/RuntimeDependencies.h"
#include "Movement/MountController.h"
#include "Movement/TravelMovement.h"

namespace Sbrpg::Runtime
{
bool SelfbotMaterialTravel::MoveToHotspot(uint32 mapId, float x, float y, float z)
{
            if (!PrepareTravelMove(bot))
                return false;
            return MoveTo(mapId, x, y, z, false, false, false, false,
                MovementPriority::MOVEMENT_NORMAL, false);
        }

bool SelfbotMaterialTravel::MoveNearLoot(WorldObject* object)
{
            return object && MoveNear(object, sPlayerbotAIConfig.contactDistance,
                MovementPriority::MOVEMENT_NORMAL);
        }

}
