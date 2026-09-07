#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
std::vector<uint32> ResolveNodeEntriesForItem(uint32 itemId);
bool IsMiningMaterial(uint32 itemId);
bool HasEntry(Sbrpg::FarmState const& state, uint32 entry);
bool MatchesSkill(Sbrpg::Profession profession, uint32 skill);
bool MatchesProfession(Sbrpg::FarmState const& state, LootObject const& loot);
bool IsGatheringEntry(Sbrpg::Profession profession, uint32 entry);
std::string NormalizeResource(std::string value);
std::vector<uint32> ResolveResource(Sbrpg::Profession profession, std::string const& name);
char const* ProfessionName(Sbrpg::Profession profession);
char const* PhaseName(Sbrpg::FarmPhase phase);
}
