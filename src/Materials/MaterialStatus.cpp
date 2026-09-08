#include "Integration/RuntimeDependencies.h"
#include "Core/SbrpgLogging.h"
#include "Materials/MaterialStatus.h"
#include "Protocol/ResponsePublisher.h"

namespace Sbrpg::Runtime
{
    void PublishMaterialStatus(Player* player, Sbrpg::Materials::MaterialFarmState& state, bool heartbeat)
    {
        uint32 const now = getMSTime();
        if (!heartbeat && state.lastPublishedStatusRevision == state.statusRevision)
            return;
        SendAddon(player, CHAT_MSG_WHISPER, Sbrpg::Protocol::Build("MATERIAL_STATUS", {
            "0", state.active ? "1" : "0", std::to_string(state.itemId),
            std::to_string(state.gatheredItems), std::to_string(state.quantityGoal),
            std::to_string(state.kills), std::to_string(Sbrpg::ActivityRemainingSeconds(state.session, now)),
            state.harvestSkills.empty() ? "0" : "1", state.phase
        }));
        state.lastPublishedStatusRevision = state.statusRevision;
        state.lastStatusPublishMs = now;
    }

    void SetMaterialPhase(Player* player, Sbrpg::Materials::MaterialFarmState& state, std::string phase)
    {
        if (state.phase == phase)
            return;
        state.phase = std::move(phase);
        ++state.statusRevision;
        Debug(player, Acore::StringFormat("material status: {}", state.phase));
        PublishMaterialStatus(player, state);
    }
}
