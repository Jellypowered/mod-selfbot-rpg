#ifndef MOD_SELFBOT_RPG_H
#define MOD_SELFBOT_RPG_H

#include "Nodes/NodeTypes.h"

namespace Sbrpg
{
    char const* PhaseLabel(FarmPhase phase);
    void Transition(FarmState& state, FarmPhase phase, std::string reason);

    bool Start(Player* player, Profession profession, std::vector<uint32> entries,
               uint32 durationMinutes, std::string* error, uint32 targetItemId = 0,
               uint32 quantityGoal = 0);
    void Finish(Player* player, std::string reason);
    void Stop(Player* player);
    void ForceStop(Player* player);
    FarmState const* Get(Player* player);
    std::string Status(Player* player);
    bool IsActive(Player* player);
    bool SetOption(Player* player, std::string const& key, uint32 value, std::string* error);
}

#endif
