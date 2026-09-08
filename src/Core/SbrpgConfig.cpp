#include "Integration/RuntimeDependencies.h"
#include "Core/SbrpgConfig.h"
#include "Core/SbrpgLogging.h"

namespace Sbrpg::Runtime
{
    void LoadRuntimeSettings()
    {
        ++runtimeSettings.policyRevision;
        runtimeSettings.dangerScreening = sConfigMgr->GetOption<bool>("SelfBotRpg.DangerScreening", false);
        runtimeSettings.adaptiveOrdering = sConfigMgr->GetOption<bool>("SelfBotRpg.AdaptiveOrdering", false);
        runtimeSettings.enable = sConfigMgr->GetOption<bool>("SelfBotRpg.Enable", true);
        runtimeSettings.debug = sConfigMgr->GetOption<bool>("SelfBotRpg.Debug", false);
        runtimeSettings.returnHomeOnStop = sConfigMgr->GetOption<bool>("SelfBotRpg.ReturnHomeOnStop", true);
        runtimeSettings.materialMinimumChance = sConfigMgr->GetOption<float>("SelfBotRpg.MaterialMinimumChance", 1.0f);
        runtimeSettings.materialReservedBagPercent = sConfigMgr->GetOption<uint32>("SelfBotRpg.MaterialReservedBagPercent", 0);
        runtimeSettings.attemptsBeforeBlacklist = sConfigMgr->GetOption<uint32>("SelfBotRpg.AttemptsBeforeBlacklist", 3);
        runtimeSettings.failedNodeBlacklistSeconds = sConfigMgr->GetOption<uint32>("SelfBotRpg.FailedNodeBlacklistSeconds", 120);
        runtimeSettings.emptyNodeBlacklistSeconds = sConfigMgr->GetOption<uint32>("SelfBotRpg.EmptyNodeBlacklistSeconds", 120);
        runtimeSettings.stayInCurrentZone = sConfigMgr->GetOption<bool>("SelfBotRpg.StayInCurrentZone", true);
        runtimeSettings.gatherSettleDelayMs = sConfigMgr->GetOption<uint32>("SelfBotRpg.GatherSettleDelayMs", 1000);
        runtimeSettings.actionDelayMs = sConfigMgr->GetOption<uint32>("SelfBotRpg.ActionDelayMs", 1000);
        runtimeSettings.fishingBobberEntries = sConfigMgr->GetOption<std::string>("SelfBotRpg.FishingBobberEntries", "35591");
        runtimeSettings.useLures = sConfigMgr->GetOption<bool>("SelfBotRpg.UseLures", true);
        runtimeSettings.fishingPrioritizePools = sConfigMgr->GetOption<bool>("SelfBotRpg.FishingPrioritizePools", false);
        runtimeSettings.fishingOpenWaterOnly = sConfigMgr->GetOption<bool>("SelfBotRpg.FishingOpenWaterOnly", true);
        runtimeSettings.fishingSearchDistance = sConfigMgr->GetOption<float>("SelfBotRpg.FishingSearchDistance", 500.0f);
        runtimeSettings.fishingCastDistance = sConfigMgr->GetOption<float>("SelfBotRpg.FishingCastDistance", 12.0f);
        runtimeSettingsLoaded = true;
    }

    bool RuntimeEnabled() { return !runtimeSettingsLoaded || runtimeSettings.enable; }

    bool SetRuntimeConfig(std::string const& key, std::string const& value, std::string* error)
    {
        try
        {
            if (key == "dangerscreening" || key == "adaptiveordering")
            {
                if (value != "0" && value != "1") throw std::invalid_argument("boolean");
                if (key == "dangerscreening") runtimeSettings.dangerScreening = value == "1";
                else runtimeSettings.adaptiveOrdering = value == "1";
            }
            else if (key == "enable") runtimeSettings.enable = std::stoul(value) != 0;
            else if (key == "debug") runtimeSettings.debug = std::stoul(value) != 0;
            else if (key == "returnhome") runtimeSettings.returnHomeOnStop = std::stoul(value) != 0;
            else if (key == "minchance")
            {
                float v = std::stof(value); if (v < 0.0f || v > 100.0f) throw std::out_of_range("range");
                runtimeSettings.materialMinimumChance = v;
            }
            else if (key == "bagreserve")
            {
                uint32 v = std::stoul(value); if (v > 100) throw std::out_of_range("range");
                runtimeSettings.materialReservedBagPercent = v;
            }
            else if (key == "attempts")
            {
                uint32 v = std::stoul(value); if (v < 1 || v > 10) throw std::out_of_range("range");
                runtimeSettings.attemptsBeforeBlacklist = v;
            }
            else if (key == "failedblacklist")
            {
                uint32 v = std::stoul(value); if (v > 3600) throw std::out_of_range("range");
                runtimeSettings.failedNodeBlacklistSeconds = v;
            }
            else if (key == "emptyblacklist")
            {
                uint32 v = std::stoul(value); if (v > 3600) throw std::out_of_range("range");
                runtimeSettings.emptyNodeBlacklistSeconds = v;
            }
            else if (key == "zone") runtimeSettings.stayInCurrentZone = std::stoul(value) != 0;
            else if (key == "settledelay")
            {
                uint32 v = std::stoul(value); if (v > 10000) throw std::out_of_range("range");
                runtimeSettings.gatherSettleDelayMs = v;
            }
            else if (key == "actiondelay")
            {
                uint32 v = std::stoul(value); if (v < 100 || v > 10000) throw std::out_of_range("range");
                runtimeSettings.actionDelayMs = v;
            }
            else if (key == "bobbers")
            {
                if (value.empty()) throw std::invalid_argument("empty");
                runtimeSettings.fishingBobberEntries = value;
            }
            else if (key == "uselures") runtimeSettings.useLures = std::stoul(value) != 0;
            else if (key == "prioritizepools") runtimeSettings.fishingPrioritizePools = std::stoul(value) != 0;
            else if (key == "openwateronly") runtimeSettings.fishingOpenWaterOnly = std::stoul(value) != 0;
            else if (key == "searchdistance")
            {
                float v = std::stof(value); if (v < 60.0f || v > 2000.0f) throw std::out_of_range("range");
                runtimeSettings.fishingSearchDistance = v;
            }
            else if (key == "castdistance")
            {
                float v = std::stof(value); if (v < 5.0f || v > 25.0f) throw std::out_of_range("range");
                runtimeSettings.fishingCastDistance = v;
            }
            else { if (error) *error = "unknown runtime setting"; return false; }
            ++runtimeSettings.policyRevision;
            runtimeSettingsLoaded = true;
            return true;
        }
        catch (...)
        {
            if (error) *error = "invalid value or out of range";
            return false;
        }
    }

RuntimeSettings runtimeSettings;
bool runtimeSettingsLoaded = false;

}
