#include "Integration/RuntimeDependencies.h"
#include "Safety/EligibilityPolicy.h"

namespace Sbrpg::Runtime
{
    bool HasHarvestTool(Player* player, SkillType skill)
    {
        if (!player)
            return false;
        switch (skill)
        {
            case SKILL_MINING:
                return player->HasItemCount(756, 1) || player->HasItemCount(778, 1) ||
                    player->HasItemCount(1819, 1) || player->HasItemCount(1893, 1) ||
                    player->HasItemCount(1959, 1) || player->HasItemCount(2901, 1) ||
                    player->HasItemCount(9465, 1) || player->HasItemCount(20723, 1) ||
                    player->HasItemCount(40772, 1) || player->HasItemCount(40892, 1) ||
                    player->HasItemCount(40893, 1);
            case SKILL_SKINNING:
                return player->HasItemCount(7005, 1) || player->HasItemCount(40772, 1) ||
                    player->HasItemCount(40893, 1) || player->HasItemCount(12709, 1) ||
                    player->HasItemCount(19901, 1);
            case SKILL_HERBALISM:
            case SKILL_ENGINEERING:
                return true;
            default:
                return false;
        }
    }

    bool IsCorpseHarvestSkill(SkillType skill)
    {
        return skill == SKILL_SKINNING || skill == SKILL_HERBALISM ||
            skill == SKILL_MINING || skill == SKILL_ENGINEERING;
    }

}
