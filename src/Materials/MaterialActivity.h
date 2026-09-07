#pragma once
#include "AttackAction.h"
#include "Materials/MaterialFarmState.h"
#include "Movement/TravelMovement.h"
namespace Sbrpg::Runtime
{
    class SelfbotMaterialAttackAction : public AttackAction
    {
    public:
        SelfbotMaterialAttackAction(PlayerbotAI* ai) : AttackAction(ai, "sbrpg material attack"), travel(ai) { }

        bool Execute(Event /*event*/) override;

    private:
        bool IsRecovering() const;

        bool MoveToNextHotspot(Sbrpg::Materials::MaterialFarmState& state);

        bool IsValidTarget(Sbrpg::Materials::MaterialFarmState const& state, Creature* creature) const;
        SelfbotMaterialTravel travel;
    };
}
