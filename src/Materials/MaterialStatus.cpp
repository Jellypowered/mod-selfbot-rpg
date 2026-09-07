#include "Integration/RuntimeDependencies.h"
#include "Core/SbrpgLogging.h"
#include "Materials/MaterialStatus.h"
#include "Protocol/ResponsePublisher.h"

namespace Sbrpg::Runtime
{
    void SetMaterialPhase(Player* player, Sbrpg::Materials::MaterialFarmState& state, std::string phase)
    {
        if (state.phase != phase)
        {
            state.phase = std::move(phase);
            Debug(player, Acore::StringFormat("material phase: {}", state.phase));
            if (state.fishing)
                SendAddon(player, CHAT_MSG_WHISPER, Sbrpg::Protocol::Build("MATERIAL_STATUS", {
                    "0", state.active ? "1" : "0", std::to_string(state.itemId),
                    std::to_string(state.gatheredItems), std::to_string(state.quantityGoal),
                    std::to_string(state.kills), "0", "0", state.phase
                }));
        }
    }

}
