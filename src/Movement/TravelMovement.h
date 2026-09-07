#pragma once
#include "MovementActions.h"
#include "Materials/MaterialFarmState.h"
namespace Sbrpg::Runtime
{
    class SelfbotMaterialTravel : public MovementAction
    {
    public:
        SelfbotMaterialTravel(PlayerbotAI* ai) : MovementAction(ai, "sbrpg material travel") { }
        bool IssueBoundedMove(Sbrpg::RouteStep const& step);
        bool ReturnHome(Sbrpg::Materials::MaterialFarmState& state, uint32 now);
        bool MoveToHotspot(uint32 mapId, float x, float y, float z);
        bool MoveNearLoot(WorldObject* object);
    };
}
