#pragma once
#include "Movement/TravelMovement.h"
namespace Sbrpg::Runtime
{
// Fishing orchestration is independent of the creature attack action.
class FishingController : public MovementAction
{
public:
    explicit FishingController(PlayerbotAI* ai)
        : MovementAction(ai, "sbrpg fishing controller"), travel(ai) { }
    bool Execute(Sbrpg::Materials::MaterialFarmState& state, uint32 now);
private:
    SelfbotMaterialTravel travel;
};
}
