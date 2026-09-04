#include "StrategyLease.h"

#include "PlayerbotAI.h"

namespace Sbrpg
{
    void StrategyLease::Acquire(PlayerbotAI* ai)
    {
        if (!ai || strategyName.empty() || IsActive())
            return;

        owned = !ai->HasStrategy(strategyName, BOT_STATE_NON_COMBAT);
        if (owned)
            ai->ChangeStrategy("+" + strategyName, BOT_STATE_NON_COMBAT);
    }

    void StrategyLease::Suspend(PlayerbotAI* ai)
    {
        if (!ai || strategyName.empty() || suspended || !ai->HasStrategy(strategyName, BOT_STATE_NON_COMBAT))
            return;

        ai->ChangeStrategy("-" + strategyName, BOT_STATE_NON_COMBAT);
        suspended = true;
    }

    void StrategyLease::Release(PlayerbotAI* ai)
    {
        if (ai && !strategyName.empty())
        {
            if (owned)
                ai->ChangeStrategy("-" + strategyName, BOT_STATE_NON_COMBAT);
            else if (suspended)
                ai->ChangeStrategy("+" + strategyName, BOT_STATE_NON_COMBAT);
        }

        owned = false;
        suspended = false;
    }
}
