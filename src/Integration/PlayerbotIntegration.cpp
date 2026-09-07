#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Integration/PlayerbotIntegration.h"

namespace Sbrpg::Runtime
{
    bool EnsureSelfBot(Player* player, bool& enabledBySbrpg)
    {
        if (!player)
            return false;
        if (IsSelfBot(player))
        {
            ActivityRegistry::PendingDisable().erase(player->GetGUID());
            return true;
        }
        PlayerbotMgr* manager = GET_PLAYERBOT_MGR(player);
        if (!manager)
            return false;
        manager->HandlePlayerbotCommand("self", player);
        enabledBySbrpg = IsSelfBot(player);
        return enabledBySbrpg;
    }

    void DisableOwnedSelfBot(Player* player, bool enabledBySbrpg)
    {
        if (!player || !enabledBySbrpg || !IsSelfBot(player))
            return;
        // Do not delete PlayerbotAI from inside an action/update callback. The
        // next player update performs the toggle after the current AI stack has
        // unwound, avoiding invalid ActionNode continuers/vtables.
        ActivityRegistry::PendingDisable().insert(player->GetGUID());
    }


}
