#pragma once
#include <optional>
#include "MovementActions.h"
#include "Nodes/NodeTypes.h"
namespace Sbrpg::Runtime
{
    class SelfbotRpgFarmAction : public MovementAction
    {
    public:
        SelfbotRpgFarmAction(PlayerbotAI* ai) : MovementAction(ai, "sbrpg farm") { }

        bool Execute(Event /*event*/) override;

    private:
        std::optional<bool> HandleLoot(Sbrpg::FarmState& mutableState, uint32 now);
        std::optional<bool> HandleLiveNodes(Sbrpg::FarmState& mutableState, uint32 now);
        bool ReturnHome(Sbrpg::FarmState& state, uint32 now);
    };
}
