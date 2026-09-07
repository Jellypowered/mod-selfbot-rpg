#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
bool HasHarvestTool(Player* player, SkillType skill);
bool IsCorpseHarvestSkill(SkillType skill);
}
