#pragma once
#include "Define.h"
#include <string>
namespace Sbrpg::Runtime
{
    struct RuntimeSettings
    {
        bool enable = true;
        bool debug = false;
        bool returnHomeOnStop = true;
        bool dangerScreening = false;
        bool adaptiveOrdering = false;
        uint32 policyRevision = 1;
        float materialMinimumChance = 1.0f;
        uint32 materialReservedBagPercent = 0;
        uint32 attemptsBeforeBlacklist = 3;
        uint32 failedNodeBlacklistSeconds = 120;
        uint32 emptyNodeBlacklistSeconds = 120;
        bool stayInCurrentZone = true;
        uint32 gatherSettleDelayMs = 1000;
        uint32 actionDelayMs = 1000;
        std::string fishingBobberEntries = "35591";
        bool useLures = true;
        bool fishingPrioritizePools = false;
        bool fishingOpenWaterOnly = true;
        float fishingSearchDistance = 500.0f;
        float fishingCastDistance = 12.0f;
    };

void LoadRuntimeSettings();
bool RuntimeEnabled();
bool SetRuntimeConfig(std::string const& key, std::string const& value, std::string* error);
extern RuntimeSettings runtimeSettings;
extern bool runtimeSettingsLoaded;
}
