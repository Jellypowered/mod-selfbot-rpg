#pragma once
#include "Movement/TravelMovement.h"
namespace Sbrpg::Runtime
{
// Fishing orchestration is independent of the creature attack action.
class FishingController : public MovementAction
{
public:
    // Keep the base Action::Execute(Event) overload visible. MSVC treats the
    // stateful overload below as hiding the inherited virtual otherwise.
    using MovementAction::Execute;

    explicit FishingController(PlayerbotAI* ai)
        : MovementAction(ai, "sbrpg fishing controller"), travel(ai) { }
    bool Execute(Sbrpg::Materials::MaterialFarmState& state, uint32 now);
private:
    SelfbotMaterialTravel travel;
};
}
