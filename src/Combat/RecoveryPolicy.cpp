#include "Integration/RuntimeDependencies.h"

#include "Materials/MaterialActivity.h"

namespace Sbrpg::Runtime
{
bool SelfbotMaterialAttackAction::IsRecovering() const
{
            if (bot->IsNonMeleeSpellCast(true))
                return true;
            if (bot->HasAura(25990))
                return true;
            if (!bot->IsSitState())
                return false;
            return bot->GetHealthPct() < 75.0f ||
                (bot->GetMaxPower(POWER_MANA) > 0 && bot->GetPowerPct(POWER_MANA) < 75.0f);
        }

}
