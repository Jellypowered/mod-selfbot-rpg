#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Core/ActivityReturn.h"
#include "Core/SbrpgConfig.h"
#include "Core/SbrpgLogging.h"

namespace Sbrpg::Runtime
{
    void UpdateMaterialReturn(Player* player)
    {
        if (!player)
            return;
        auto it = ActivityRegistry::Materials().find(player->GetGUID());
        if (it == ActivityRegistry::Materials().end() || !it->second.active || it->second.session.returnRequested)
            return;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai)
            return;
        uint32 const reservedBagPercent = runtimeSettings.materialReservedBagPercent;
        uint8 const bagThreshold = static_cast<uint8>(100u - std::min(100u, reservedBagPercent));
        uint32 const now = getMSTime();
        bool const timedOut = Sbrpg::ActivityTimedOut(it->second.session, now);
        bool const quantityReached = it->second.quantityGoal != 0 &&
            it->second.gatheredItems >= it->second.quantityGoal;
        bool const bagsFull = ai->GetAiObjectContext()->GetValue<uint8>("bag space")->Get() >= bagThreshold;
        if (!timedOut && !quantityReached && !bagsFull)
            return;
        std::string const reason = bagsFull ? "bag reserve reached" :
            quantityReached ? "quantity goal reached" : "timed session complete";
        Sbrpg::RequestActivityReturn(it->second.session, now, reason);
        if (!it->second.returnNotified)
        {
            it->second.returnNotified = true;
            if (WorldSession* session = player->GetSession())
                ChatHandler(session).PSendSysMessage(
                    "[SBRPG] Material session complete: {}. Returning to the session start.", reason);
        }
        Debug(player, Acore::StringFormat("material return requested: {}", reason));
    }

}
