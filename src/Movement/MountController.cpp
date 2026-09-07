#include "Integration/RuntimeDependencies.h"
#include "Core/SbrpgLogging.h"
#include "Movement/MountController.h"

namespace Sbrpg::Runtime
{
    bool CastBestLearnedMount(Player* player, PlayerbotAI* ai)
    {
        if (!player || !ai)
            return false;

        struct MountCandidate
        {
            uint32 spellId = 0;
            int32 speed = 0;
        };
        std::vector<MountCandidate> candidates;
        for (auto const& [spellId, playerSpell] : player->GetSpellMap())
        {
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
            if (!playerSpell || playerSpell->State == PLAYERSPELL_REMOVED ||
                !playerSpell->Active || !spellInfo || spellInfo->IsPassive() ||
                !spellInfo->HasAura(SPELL_AURA_MOUNTED))
                continue;

            // The stock collector checks only Effects[0] for MOUNTED. Hybrid
            // and exotic mounts such as the Headless Horseman's Mount can put
            // that aura in another effect slot, so retain every learned mount
            // and rank it by its strongest applicable speed aura.
            int32 speed = 0;
            for (SpellEffectInfo const& effect : spellInfo->Effects)
            {
                if (effect.ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED ||
                    effect.ApplyAuraName == SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED)
                    speed = std::max(speed, effect.CalcValue(player));
            }
            candidates.push_back({ spellId, speed });
        }

        std::sort(candidates.begin(), candidates.end(),
            [](MountCandidate const& left, MountCandidate const& right)
            {
                return left.speed > right.speed;
            });
        for (MountCandidate const& candidate : candidates)
        {
            if (!ai->CanCastSpell(candidate.spellId, player, true))
                continue;
            if (player->isMoving())
                player->StopMoving();
            if (ai->CastSpell(candidate.spellId, player))
            {
                Debug(player, Acore::StringFormat(
                    "travel mount fallback cast learned spell {} (speed {})",
                    candidate.spellId, candidate.speed));
                return true;
            }
        }
        return false;
    }

    bool PrepareTravelMove(Player* player)
    {
        if (!player || player->IsMounted())
            return true;

        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai)
            return true;

        // The mount action refuses to run unless the mount strategy is
        // present. Reassert it here as a defensive check because other
        // playerbot strategy changes can remove non-combat strategies while
        // an SBRPG activity is active.
        if (!ai->HasStrategy("mount", BOT_STATE_NON_COMBAT))
            ai->ChangeStrategy("+mount", BOT_STATE_NON_COMBAT);

        // MountStrategy intentionally has no triggers. It is only the policy
        // flag checked by CheckMountStateAction; merely leasing "mount" cannot
        // initiate a cast. Invoke the stock selector directly so this works
        // independently of engine action scheduling and stale target values.
        CheckMountStateAction mountAction(ai);
        if (!mountAction.isUseful())
            return true;
        if (!mountAction.Mount())
        {
            if (!CastBestLearnedMount(player, ai))
            {
                Debug(player, "travel mount check found no usable learned mount");
                return true;
            }
            return player->IsMounted();
        }

        Debug(player, "travel mount requested through stock mount selector");
        // Mount casts are asynchronous. Do not issue movement in the same tick
        // or it will interrupt the cast before the mounted aura is applied.
        return player->IsMounted();
    }

}
