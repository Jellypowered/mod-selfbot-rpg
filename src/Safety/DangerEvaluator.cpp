#include "Safety/DangerEvaluator.h"
#include "Core/SbrpgConfig.h"
#include "CellImpl.h"
#include "Creature.h"
#include "Formulas.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include <list>
#include <algorithm>
#include <cmath>

namespace Sbrpg::Safety
{
FeatureAvailability DangerEvaluator::Availability()
{
    return {true, "Opt-in local danger screening"};
}

Risk DangerEvaluator::Evaluate(Player* player, float x, float y, float z)
{
    if (!player || !player->IsInWorld()) return Risk::Unknown;
    float const distance = player->GetExactDist(x, y, z);
    // Never load remote grids or claim knowledge of a distant destination.
    if (distance > 80.0f) return Risk::Unknown;
    std::list<Unit*> units;
    Acore::AnyUnitInObjectRangeCheck check(player, distance + 15.0f);
    Acore::UnitListSearcher<Acore::AnyUnitInObjectRangeCheck> searcher(player, units, check);
    Cell::VisitObjects(player, searcher, distance + 15.0f);
    ThreatSummary threats;
    threats.complete = true;
    uint32_t inspected = 0;
    for (Unit* unit : units)
    {
        if (++inspected > 256) return Risk::Unknown;
        Creature* creature = unit ? unit->ToCreature() : nullptr;
        if (!creature || !creature->IsAlive() || creature->IsPet() ||
            !creature->IsHostileTo(player) || !player->IsValidAttackTarget(creature) ||
            creature->GetExactDist(x, y, z) > 15.0f)
            continue;
        auto const* info = creature->GetCreatureTemplate();
        if (!info) return Risk::Unknown;
        // Gray exemption never applies to non-normal ranks.
        if (info->rank != CREATURE_ELITE_NORMAL) threats.elite = true;
        if (creature->GetLevel() <= Acore::XP::GetGrayLevel(player->GetLevel())) continue;
        ++threats.ordinaryNonGray;
        int32_t const delta = int32_t(creature->GetLevel()) - int32_t(player->GetLevel());
        if (delta >= 2) ++threats.plusTwo;
        if (delta >= 3) ++threats.plusThree;
    }
    return Assess(threats);
}

bool DangerEvaluator::AllowsSegment(Player* player, float x, float y, float z)
{
    if (!Runtime::runtimeSettings.dangerScreening) return true;
    if (!player || player->IsInCombat()) return false;
    float const distance = player->GetExactDist(x, y, z);
    if (distance > 30.0f) return false;
    uint32_t const samples = std::max(1u, uint32_t(std::ceil(distance / 3.0f)));
    for (uint32_t i = 0; i <= samples; ++i)
    {
        float const fraction = float(i) / samples;
        if (!Allows(player, player->GetPositionX() + (x - player->GetPositionX()) * fraction,
                    player->GetPositionY() + (y - player->GetPositionY()) * fraction,
                    player->GetPositionZ() + (z - player->GetPositionZ()) * fraction)) return false;
    }
    return true;
}

bool DangerEvaluator::Allows(Player* player, float x, float y, float z)
{
    if (!Runtime::runtimeSettings.dangerScreening) return true;
    // A preemptive policy must never take over emergency combat movement.
    if (!player || player->IsInCombat()) return false;
    return Evaluate(player, x, y, z) == Risk::Safe;
}
}
