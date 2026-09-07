#include "Integration/RuntimeDependencies.h"

#include "Materials/MaterialActivity.h"

namespace Sbrpg::Runtime
{
bool SelfbotMaterialAttackAction::IsValidTarget(Sbrpg::Materials::MaterialFarmState const& state, Creature* creature) const
{
            if (!creature || !creature->IsInWorld() || !creature->IsAlive() ||
                creature->GetMapId() != bot->GetMapId() || creature->GetZoneId() != state.zoneId ||
                !bot->IsValidAttackTarget(creature) ||
                !bot->IsWithinLOSInMap(creature) || bot->GetDistance(creature) > sPlayerbotAIConfig.grindDistance)
                return false;
            if (std::find(state.creatureEntries.begin(), state.creatureEntries.end(), creature->GetEntry()) ==
                state.creatureEntries.end())
                return false;
            CreatureTemplate const* info = creature->GetCreatureTemplate();
            return info && info->rank <= CREATURE_ELITE_NORMAL;
        }

}
