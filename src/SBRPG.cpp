#include "SBRPG.h"
#include "Farm/NodeRepository.h"
#include "Farm/RouteFollower.h"
#include "Farm/RoutePlanner.h"
#include "Materials/CreatureSpawnRepository.h"
#include "Materials/HotspotPlanner.h"
#include "Materials/LootSourceIndex.h"
#include "Materials/MaterialFarmState.h"
#include "Materials/MaterialCatalog.h"
#include "Protocol/SbrpgProtocol.h"
#include "Protocol/StatusPublisher.h"

#include "Action.h"
#include "AttackAction.h"
#include "AiObjectContext.h"
#include "DKAiObjectContext.h"
#include "DruidAiObjectContext.h"
#include "HunterAiObjectContext.h"
#include "MageAiObjectContext.h"
#include "PaladinAiObjectContext.h"
#include "PriestAiObjectContext.h"
#include "RogueAiObjectContext.h"
#include "ShamanAiObjectContext.h"
#include "WarlockAiObjectContext.h"
#include "WarriorAiObjectContext.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CheckMountStateAction.h"
#include "CellImpl.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Event.h"
#include "GameObject.h"
#include "Group.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "LootObjectStack.h"
#include "LootStrategyValue.h"
#include "Log.h"
#include "Map.h"
#include "Movement/Spline/MoveSplineInitArgs.h"
#include "MovementActions.h"
#include "FishingAction.h"
#include "NamedObjectContext.h"
#include "UseItemAction.h"
#include "NearestGameObjects.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotMgr.h"
#include "Playerbots.h"
#include "PathGenerator.h"
#include "ScriptMgr.h"
#include "ServerFacade.h"
#include "SpellMgr.h"
#include "Strategy.h"
#include "StringFormat.h"
#include "Timer.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <exception>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

using namespace Acore::ChatCommands;

// Stock playerbot helper; kept as a module-local declaration so no global
// playerbot behavior or public header needs to change.
WorldPosition FindLandFromPosition(PlayerbotAI* botAI, float startDistance, float endDistance,
    float increment, float orientation, WorldPosition targetPos, float fishingSearchWindow, bool checkLOS);

namespace
{
    std::unordered_map<ObjectGuid, Sbrpg::FarmState> states;
    std::unordered_map<ObjectGuid, Sbrpg::Materials::MaterialFarmState> materialStates;
    std::unordered_set<ObjectGuid> pendingSelfBotDisable;
    uint64_t nextRunId = 0;

    std::string FormatDuration(uint64 seconds)
    {
        uint64 const hours = seconds / 3600;
        uint64 const minutes = (seconds % 3600) / 60;
        uint64 const remainder = seconds % 60;
        if (hours != 0)
            return Acore::StringFormat("{}h {}m {}s", hours, minutes, remainder);
        if (minutes != 0)
            return Acore::StringFormat("{}m {}s", minutes, remainder);
        return Acore::StringFormat("{}s", remainder);
    }

    std::string FormatDurationMs(uint64 milliseconds)
    {
        return FormatDuration(milliseconds / 1000);
    }

    bool EnsureSelfBot(Player* player, bool& enabledBySbrpg)
    {
        if (!player)
            return false;
        if (IsSelfBot(player))
        {
            pendingSelfBotDisable.erase(player->GetGUID());
            return true;
        }
        PlayerbotMgr* manager = GET_PLAYERBOT_MGR(player);
        if (!manager)
            return false;
        manager->HandlePlayerbotCommand("self", player);
        enabledBySbrpg = IsSelfBot(player);
        return enabledBySbrpg;
    }

    void DisableOwnedSelfBot(Player* player, bool enabledBySbrpg)
    {
        if (!player || !enabledBySbrpg || !IsSelfBot(player))
            return;
        // Do not delete PlayerbotAI from inside an action/update callback. The
        // next player update performs the toggle after the current AI stack has
        // unwound, avoiding invalid ActionNode continuers/vtables.
        pendingSelfBotDisable.insert(player->GetGUID());
    }

    struct RuntimeSettings
    {
        bool enable = true;
        bool debug = false;
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

    RuntimeSettings runtimeSettings;
    bool runtimeSettingsLoaded = false;

    void LoadRuntimeSettings()
    {
        runtimeSettings.enable = sConfigMgr->GetOption<bool>("SelfBotRpg.Enable", true);
        runtimeSettings.debug = sConfigMgr->GetOption<bool>("SelfBotRpg.Debug", false);
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
            if (key == "enable") runtimeSettings.enable = std::stoul(value) != 0;
            else if (key == "debug") runtimeSettings.debug = std::stoul(value) != 0;
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
            runtimeSettingsLoaded = true;
            return true;
        }
        catch (...)
        {
            if (error) *error = "invalid value or out of range";
            return false;
        }
    }

    std::vector<uint32> ResolveNodeEntriesForItem(uint32 itemId)
    {
        std::vector<uint32> entries;
        QueryResult rows = WorldDatabase.Query(
            "SELECT DISTINCT gt.entry FROM gameobject_template gt "
            "JOIN gameobject_loot_template gl ON gl.Entry = gt.Data1 "
            "WHERE gt.type IN (3, 25) AND gl.Item = {}", itemId);
        if (!rows)
            return entries;
        do
        {
            uint32 entry = rows->Fetch()[0].Get<uint32>();
            if (std::find(entries.begin(), entries.end(), entry) == entries.end())
                entries.push_back(entry);
        } while (rows->NextRow());
        return entries;
    }

    std::vector<uint32> ResolveFishingPoolEntriesForItem(uint32 itemId)
    {
        std::vector<uint32> entries;
        QueryResult rows = WorldDatabase.Query(
            "SELECT DISTINCT gt.entry FROM gameobject_template gt "
            "JOIN fishing_loot_template fl ON fl.Entry = gt.Data1 "
            "WHERE gt.type = 25 AND fl.Item = {}", itemId);
        if (!rows)
            return entries;
        do
        {
            uint32 entry = rows->Fetch()[0].Get<uint32>();
            if (std::find(entries.begin(), entries.end(), entry) == entries.end())
                entries.push_back(entry);
        } while (rows->NextRow());
        return entries;
    }

    std::vector<uint32> ResolveFishingPoolEntriesForZone(Player* player)
    {
        std::vector<uint32> entries;
        if (!player)
            return entries;
        QueryResult rows = WorldDatabase.Query(
            "SELECT DISTINCT gt.entry FROM gameobject_template gt "
            "JOIN fishing_loot_template fl ON fl.Entry = gt.Data1 "
            "WHERE gt.type = 25");
        if (!rows)
            return entries;
        do { entries.push_back(rows->Fetch()[0].Get<uint32>()); } while (rows->NextRow());
        return entries;
    }

    std::vector<Sbrpg::Materials::FishingPoolPoint> LoadFishingPools(Player* player,
        std::vector<uint32> const& entries)
    {
        std::vector<Sbrpg::Materials::FishingPoolPoint> pools;
        if (!player || entries.empty())
            return pools;
        std::ostringstream ids;
        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            if (i) ids << ',';
            ids << entries[i];
        }
        QueryResult rows = WorldDatabase.Query(
            "SELECT guid, id, position_x, position_y, position_z FROM gameobject "
            "WHERE map = {} AND id IN ({})", player->GetMapId(), ids.str());
        if (!rows)
            return pools;
        do
        {
            Field* fields = rows->Fetch();
            float x = fields[2].Get<float>(), y = fields[3].Get<float>(), z = fields[4].Get<float>();
            if (player->GetMap()->GetZoneId(player->GetPhaseMask(), x, y, z) == player->GetZoneId())
                pools.push_back({fields[0].Get<uint32>(), fields[1].Get<uint32>(), x, y, z, false});
        } while (rows->NextRow());
        return pools;
    }

    bool IsMiningMaterial(uint32 itemId)
    {
        switch (itemId)
        {
            case 2835: case 2770: case 2771: case 2772: case 3858: case 7911:
            case 10620: case 23424: case 23425: case 36909: case 36912: case 36910:
                return true;
            default: return false;
        }
    }

    bool HasEntry(Sbrpg::FarmState const& state, uint32 entry)
    {
        return std::find(state.entries.begin(), state.entries.end(), entry) != state.entries.end();
    }

    bool MatchesSkill(Sbrpg::Profession profession, uint32 skill)
    {
        return (profession == Sbrpg::Profession::Mining && skill == SKILL_MINING) ||
               (profession == Sbrpg::Profession::Herbalism && skill == SKILL_HERBALISM) ||
               (profession == Sbrpg::Profession::Both && (skill == SKILL_MINING || skill == SKILL_HERBALISM));
    }

    bool MatchesProfession(Sbrpg::FarmState const& state, LootObject const& loot)
    {
        return MatchesSkill(state.profession, loot.skillId);
    }

    bool IsGatheringEntry(Sbrpg::Profession profession, uint32 entry)
    {
        GameObjectTemplate const* info = sObjectMgr->GetGameObjectTemplate(entry);
        LockEntry const* lock = info ? sLockStore.LookupEntry(info->GetLockId()) : nullptr;
        if (!lock) return false;
        for (uint8 i = 0; i < 8; ++i)
            if (lock->Type[i] == LOCK_KEY_SKILL && MatchesSkill(profession, SkillByLockType(LockType(lock->Index[i]))))
                return true;
        return false;
    }

    std::string NormalizeResource(std::string value)
    {
        std::string normalized;
        bool previousSpace = true;
        for (unsigned char c : value)
        {
            if (std::isspace(c))
            {
                if (!previousSpace) normalized += ' ';
                previousSpace = true;
            }
            else if (std::isalpha(c))
            {
                normalized += static_cast<char>(std::tolower(c));
                previousSpace = false;
            }
        }
        if (!normalized.empty() && normalized.back() == ' ')
            normalized.pop_back();
        return normalized;
    }

    // Resource names deliberately resolve through the server's gameobject
    // templates rather than baking entry IDs into the UI. That keeps the module
    // compatible with custom database packs that retain the WotLK node names.
    std::vector<uint32> ResolveResource(Sbrpg::Profession profession, std::string const& name)
    {
        static std::unordered_map<std::string, std::string> const mining = {
            {"copper", "Copper Vein"}, {"tin", "Tin Vein"}, {"silver", "Silver Vein"},
            {"iron", "Iron Deposit"}, {"gold", "Gold Vein"}, {"mithril", "Mithril Deposit"},
            {"truesilver", "Truesilver Deposit"}, {"thorium", "Thorium"},
            {"fel iron", "Fel Iron Deposit"}, {"adamantite", "Adamantite"},
            {"khorium", "Khorium"}, {"cobalt", "Cobalt"}, {"saronite", "Saronite"},
            {"titanium", "Titanium"}
        };
        static std::unordered_map<std::string, std::string> const herbs = {
            {"peacebloom", "Peacebloom"}, {"silverleaf", "Silverleaf"}, {"earthroot", "Earthroot"},
            {"mageroyal", "Mageroyal"}, {"briarthorn", "Briarthorn"}, {"bruiseweed", "Bruiseweed"},
            {"wild steelbloom", "Wild Steelbloom"}, {"kingsblood", "Kingsblood"},
            {"liferoot", "Liferoot"}, {"fadeleaf", "Fadeleaf"}, {"goldthorn", "Goldthorn"},
            {"khadgar's whisker", "Khadgar's Whisker"}, {"firebloom", "Firebloom"},
            {"purple lotus", "Purple Lotus"}, {"arthas' tears", "Arthas' Tears"},
            {"sungrass", "Sungrass"}, {"blindweed", "Blindweed"}, {"ghost mushroom", "Ghost Mushroom"},
            {"gromsblood", "Gromsblood"}, {"dreamfoil", "Dreamfoil"}, {"mountain silversage", "Mountain Silversage"},
            {"plaguebloom", "Plaguebloom"}, {"icecap", "Icecap"}, {"felweed", "Felweed"},
            {"dreaming glory", "Dreaming Glory"}, {"ragveil", "Ragveil"}, {"flame cap", "Flame Cap"},
            {"terocone", "Terocone"}, {"ancient lichen", "Ancient Lichen"}, {"netherbloom", "Netherbloom"},
            {"nightmare vine", "Nightmare Vine"}, {"mana thistle", "Mana Thistle"},
            {"goldclover", "Goldclover"}, {"tiger lily", "Tiger Lily"}, {"talandra's rose", "Talandra's Rose"},
            {"deadnettle", "Deadnettle"}, {"fire leaf", "Fire Leaf"}, {"adder's tongue", "Adder's Tongue"},
            {"lichbloom", "Lichbloom"}, {"icethorn", "Icethorn"}, {"frost lotus", "Frost Lotus"}
        };

        auto const& resources = profession == Sbrpg::Profession::Mining ? mining : herbs;
        auto it = resources.find(NormalizeResource(name));
        if (it == resources.end())
            return {};

        std::vector<uint32> entries;
        QueryResult result = WorldDatabase.Query(
            "SELECT entry FROM gameobject_template WHERE name LIKE '%{}%'", it->second);
        if (!result)
            return entries;
        do
        {
            uint32 const entry = result->Fetch()[0].Get<uint32>();
            if (entry && std::find(entries.begin(), entries.end(), entry) == entries.end())
                entries.push_back(entry);
        } while (result->NextRow());
        return entries;
    }

    char const* ProfessionName(Sbrpg::Profession profession)
    {
        return profession == Sbrpg::Profession::Mining ? "mining" : profession == Sbrpg::Profession::Herbalism ? "herbalism" : "mining and herbalism";
    }

    char const* PhaseName(Sbrpg::FarmPhase phase)
    {
        return Sbrpg::PhaseLabel(phase);
    }

    void SetPhase(Sbrpg::FarmState& state, Sbrpg::FarmPhase phase, std::string reason)
    {
        Sbrpg::Transition(state, phase, std::move(reason));
    }

    void Debug(Player* bot, std::string const& message)
    {
        if (!runtimeSettings.debug)
            return;
        LOG_DEBUG("module", "[SBRPG] {}: {}", bot->GetName(), message);
        if (WorldSession* session = bot->GetSession())
            ChatHandler(session).PSendSysMessage("[SBRPG] {}", message);
    }

    void PublishStatus(Player* player);

    Sbrpg::Materials::MaterialDefinition const* ResolveMaterial(std::string value, uint32& itemId)
    {
        Sbrpg::Materials::MaterialDefinition const* material = Sbrpg::Materials::Find(value);
        if (material)
        {
            itemId = material->itemId;
            return material;
        }

        std::string normalized = Sbrpg::Materials::Normalize(std::move(value));
        if (normalized.empty() || !std::all_of(normalized.begin(), normalized.end(),
            [](unsigned char character) { return std::isdigit(character); }))
            return nullptr;
        try
        {
            unsigned long const parsed = std::stoul(normalized);
            if (parsed <= std::numeric_limits<uint32>::max())
                itemId = static_cast<uint32>(parsed);
        }
        catch (std::exception const&)
        {
            itemId = 0;
        }
        return Sbrpg::Materials::Find(itemId);
    }

    class SelfbotMaterialLootStrategy : public Strategy
    {
    public:
        explicit SelfbotMaterialLootStrategy(PlayerbotAI* ai) : Strategy(ai) { }
        std::string const getName() override { return "sbrpg material loot"; }
        uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
        std::vector<NextAction> getDefaultActions() override
        {
            return {
                NextAction("loot", 8.0f),
                NextAction("move to loot", 7.0f),
                NextAction("open loot", 9.0f),
                NextAction("add all loot", 5.0f)
            };
        }
    };

    void RequestMaterialStop(Player* player, std::string reason = "stopped by user")
    {
        if (!player)
            return;
        auto it = materialStates.find(player->GetGUID());
        if (it == materialStates.end() || !it->second.active)
            return;
        uint32 const now = getMSTime();
        Sbrpg::RequestActivityReturn(it->second.session, now, std::move(reason));
        it->second.returnNotified = true;
        // Preserve the current creature objective so combat can finish and its
        // corpse can still be looted/skinned before the return route begins.
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
            it->second.gatherStrategy.Suspend(ai);
        player->StopMoving();
        if (WorldSession* session = player->GetSession())
            ChatHandler(session).PSendSysMessage("[SBRPG] Stop requested. Returning to the session start after active loot/combat.");
        Debug(player, "material stop requested; returning to session start");
    }

    Item* FindEquippedFishingPole(Player* player);

    void RestoreFishingEquipment(Player* player, Sbrpg::Materials::MaterialFarmState const& state)
    {
        if (!player || !state.fishingPoleEquipped)
            return;
        if (!state.fishingPreviousMainHand.IsEmpty())
            if (Item* item = player->GetItemByGuid(state.fishingPreviousMainHand))
                player->SwapItem(item->GetPos(), (INVENTORY_SLOT_BAG_0 << 8) | EQUIPMENT_SLOT_MAINHAND);
        if (!state.fishingPreviousOffHand.IsEmpty())
            if (Item* item = player->GetItemByGuid(state.fishingPreviousOffHand))
                player->SwapItem(item->GetPos(), (INVENTORY_SLOT_BAG_0 << 8) | EQUIPMENT_SLOT_OFFHAND);

        // EquipFishingPoleAction uses the normal inventory/equipment swap
        // path. Mirror that path on return; do not RemoveItem/StoreItem an
        // equipped object, which can duplicate or orphan the pole.
        Item* pole = FindEquippedFishingPole(player);
        if (!pole)
            return;
        ItemPosCountVec destination;
        if (player->CanStoreItem(NULL_BAG, NULL_SLOT, destination, pole, false) != EQUIP_ERR_OK || destination.empty())
        {
            if (WorldSession* session = player->GetSession())
                ChatHandler(session).PSendSysMessage("[SBRPG] Could not return the fishing pole to inventory: no free inventory space.");
            return;
        }
        player->SwapItem(pole->GetPos(), destination.front().pos);
    }

    void StopMaterial(Player* player, std::string reason = "stopped")
    {
        if (!player)
            return;
        auto it = materialStates.find(player->GetGUID());
        if (it == materialStates.end())
            return;
        if (WorldSession* session = player->GetSession(); session && it->second.session.startedMs != 0)
        {
            uint32 const elapsed = std::max(1u, getMSTime() - it->second.session.startedMs);
            ChatHandler(session).PSendSysMessage(
                "[SBRPG] Material summary: {}, {} kills, {} requested items, {} loot events, {} corpse timeouts ({}).",
                FormatDurationMs(elapsed), it->second.kills, it->second.gatheredItems, it->second.lootEvents,
                it->second.corpseTimeouts, reason);
        }
        RestoreFishingEquipment(player, it->second);
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        {
            if (it->second.lootStrategyOverridden)
            {
                ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Set(
                    LootStrategyValue::instance(it->second.previousLootStrategy));
                it->second.lootStrategyOverridden = false;
            }
            it->second.mountStrategy.Release(ai);
            it->second.materialStrategy.Release(ai);
            it->second.materialLootStrategy.Release(ai);
            it->second.lootStrategy.Release(ai);
            it->second.gatherStrategy.Release(ai);
            it->second.grindStrategy.Release(ai);
            it->second.rpgStrategy.Release(ai);
        }
        bool const disableSelfBot = it->second.selfBotEnabledBySbrpg;
        materialStates.erase(it);
        DisableOwnedSelfBot(player, disableSelfBot);
    }

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

    bool IsCatalogFishingItem(uint32 itemId)
    {
        auto const* material = Sbrpg::Materials::Find(itemId);
        return material && std::find(material->methods.begin(), material->methods.end(),
            Sbrpg::Materials::AcquisitionMethod::Fishing) != material->methods.end();
    }

    uint32 FishingInventoryCount(Player* player)
    {
        if (!player)
            return 0;
        uint32 total = 0;
        for (auto const& material : Sbrpg::Materials::Catalog())
            if (IsCatalogFishingItem(material.itemId))
                total += player->GetItemCount(material.itemId, true);
        return total;
    }

    bool HasFishingPole(Player* player)
    {
        return player && (player->HasItemCount(6256, 1) || player->HasItemCount(6365, 1) ||
            player->HasItemCount(6366, 1) || player->HasItemCount(6367, 1) ||
            player->HasItemCount(6368, 1) || player->HasItemCount(19022, 1) ||
            player->HasItemCount(19970, 1) || player->HasItemCount(44050, 1));
    }

    Item* FindEquippedFishingPole(Player* player)
    {
        if (!player)
            return nullptr;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;
            switch (item->GetEntry())
            {
                case 6256: case 6365: case 6366: case 6367: case 6368:
                case 19022: case 19970: case 44050:
                    return item;
                default: break;
            }
        }
        return nullptr;
    }

    Item* FindFishingLure(Player* player)
    {
        if (!player)
            return nullptr;
        // Prefer the strongest/current-expansion lure, then fall back to the
        // classic lures. No lure is mandatory for fishing.
        static uint32 const lureIds[] = { 46006, 34861, 6811, 6533, 6532, 6531, 6530, 6529 };
        for (uint32 lureId : lureIds)
            if (Item* lure = player->GetItemByEntry(lureId))
                return lure;
        return nullptr;
    }

    class SbrpgLureUseAction : public UseItemAction
    {
    public:
        explicit SbrpgLureUseAction(PlayerbotAI* ai) : UseItemAction(ai, "sbrpg fishing lure") { }
        bool Apply(Item* lure, Item* pole) { return lure && pole && UseItemOnItem(lure, pole); }
    };

    bool StartFishingMaterial(Player* player, uint32 itemId, uint32 durationMinutes,
        uint32 quantityGoal, std::string* error, bool byZone = false,
        bool prioritizePools = false, bool openWaterOnly = true)
    {
        if (!RuntimeEnabled())
        { if (error) *error = "SBRPG is disabled by SelfBotRpg.Enable."; return false; }
        if (!player)
        { if (error) *error = "Player is unavailable."; return false; }
        if (Sbrpg::IsActive(player))
        { if (error) *error = "Stop the active node-farming session first."; return false; }
        auto existingMaterial = materialStates.find(player->GetGUID());
        if (existingMaterial != materialStates.end() && existingMaterial->second.active)
        { if (error) *error = "Stop the active material session first."; return false; }
        bool enabledBySbrpg = false;
        if (!EnsureSelfBot(player, enabledBySbrpg))
        { if (error) *error = "Unable to enable self-bot mode."; return false; }
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai->HasSkill(SKILL_FISHING))
        { DisableOwnedSelfBot(player, enabledBySbrpg); if (error) *error = "This material requires the Fishing skill."; return false; }
        if (!HasFishingPole(player))
        { DisableOwnedSelfBot(player, enabledBySbrpg); if (error) *error = "Fishing requires a fishing pole in the inventory."; return false; }

        Sbrpg::Materials::MaterialFarmState& state = materialStates[player->GetGUID()];
        state = Sbrpg::Materials::MaterialFarmState();
        state.active = true;
        state.selfBotEnabledBySbrpg = enabledBySbrpg;
        state.fishing = true;
        state.fishingByZone = byZone;
        state.fishingPrioritizePools = prioritizePools;
        state.fishingOpenWaterOnly = openWaterOnly;
        state.itemId = itemId;
        state.quantityGoal = quantityGoal;
        state.inventoryStart = byZone ? FishingInventoryCount(player) : player->GetItemCount(itemId, true);
        state.inventoryCount = state.inventoryStart;
        state.mapId = player->GetMapId();
        state.zoneId = player->GetZoneId();
        state.phase = "fishing";
        std::vector<uint32> poolEntries = byZone ? ResolveFishingPoolEntriesForZone(player) : ResolveFishingPoolEntriesForItem(itemId);
        if (state.fishingOpenWaterOnly || !state.fishingPrioritizePools)
            poolEntries.clear();
        state.fishingPools = LoadFishingPools(player, poolEntries);
        state.fishingPoolMode = state.fishingPrioritizePools && !state.fishingOpenWaterOnly && !state.fishingPools.empty();
        Item* mainHand = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        Item* offHand = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
        if (!mainHand || !FindEquippedFishingPole(player))
        {
            state.fishingPreviousMainHand = mainHand ? mainHand->GetGUID() : ObjectGuid::Empty;
            state.fishingPreviousOffHand = offHand ? offHand->GetGUID() : ObjectGuid::Empty;
            EquipFishingPoleAction equipAction(ai);
            if (equipAction.isUseful() && equipAction.Execute(Event("sbrpg fishing equip")))
                state.fishingPoleEquipped = true;
        }
        Sbrpg::BeginActivitySession(state.session, player, durationMinutes, getMSTime());
        state.previousLootStrategy = ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Get()->GetName();
        state.lootStrategy.SetStrategy("loot");
        // Ensure stock loot exists even when selfbot was just enabled; the
        // lease records whether SBRPG added it so it can be restored on stop.
        state.lootStrategy.Acquire(ai);
        state.lootStrategy.Suspend(ai);
        ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Set(
            LootStrategyValue::instance("all"));
        state.lootStrategyOverridden = true;
        state.materialLootStrategy.SetStrategy("sbrpg material loot");
        state.materialLootStrategy.Acquire(ai);
        state.materialStrategy.SetStrategy("sbrpg material");
        state.materialStrategy.Acquire(ai);
        return true;
    }

    bool StartMaterial(Player* player, std::string materialName, uint32 durationMinutes,
                       uint32 quantityGoal, std::string* error)
    {
        // Material starts use the same module-owned selfbot lease as node
        // farming. Refuse overlap so Stop can always release exactly what this
        // run acquired and return to its own captured start position.
        if (!player)
        {
            if (error)
                *error = "Player is unavailable.";
            return false;
        }
        if (Sbrpg::IsActive(player))
        {
            if (error)
                *error = "Stop the active node-farming session before starting material farming.";
            return false;
        }
        if (!RuntimeEnabled())
        { if (error) *error = "SBRPG is disabled by SelfBotRpg.Enable."; return false; }
        if (materialStates.contains(player->GetGUID()))
        {
            if (error)
                *error = "Stop the active material session before starting another one.";
            return false;
        }

        uint32 itemId = 0;
        Sbrpg::Materials::MaterialDefinition const* material = ResolveMaterial(std::move(materialName), itemId);
        if (!material)
        { if (error) *error = "Unknown material; use a catalog name (cloth or leather) or an exact item ID."; return false; }

        float const minimumChance = runtimeSettings.materialMinimumChance;
        auto supportsMethod = [material](Sbrpg::Materials::AcquisitionMethod method)
        {
            return std::find(material->methods.begin(), material->methods.end(), method) != material->methods.end();
        };
        if (supportsMethod(Sbrpg::Materials::AcquisitionMethod::Fishing))
            return StartFishingMaterial(player, itemId, durationMinutes, quantityGoal, error, false,
                runtimeSettings.fishingPrioritizePools, runtimeSettings.fishingOpenWaterOnly);
        if (supportsMethod(Sbrpg::Materials::AcquisitionMethod::GameObjectNode))
        {
            std::vector<uint32> nodeEntries = ResolveNodeEntriesForItem(itemId);
            if (nodeEntries.empty())
            { if (error) *error = "No database gathering nodes produce this material."; return false; }
            Sbrpg::Profession profession = IsMiningMaterial(itemId) ?
                Sbrpg::Profession::Mining : Sbrpg::Profession::Herbalism;
            return Sbrpg::Start(player, profession, std::move(nodeEntries), durationMinutes,
                error, itemId, quantityGoal);
        }

        bool enabledBySbrpg = false;
        if (!EnsureSelfBot(player, enabledBySbrpg))
        { if (error) *error = "Unable to enable self-bot mode."; return false; }
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        std::vector<SkillType> harvestSkills;
        std::vector<uint32> sourceEntries;
        uint32 blockedHarvestSources = 0;
        auto addHarvestSkill = [&harvestSkills](SkillType skill)
        {
            if (IsCorpseHarvestSkill(skill) &&
                std::find(harvestSkills.begin(), harvestSkills.end(), skill) == harvestSkills.end())
                harvestSkills.push_back(skill);
        };
        for (Sbrpg::Materials::LootSource const& source : Sbrpg::Materials::LootSourceIndex::Find(itemId))
        {
            if (source.questRequired || !supportsMethod(source.method) ||
                source.estimatedChance < minimumChance)
                continue;

            if (source.requiredSkill != SKILL_NONE)
            {
                if (!IsCorpseHarvestSkill(source.requiredSkill) ||
                    !ai->HasSkill(source.requiredSkill) ||
                    !HasHarvestTool(player, source.requiredSkill))
                {
                    ++blockedHarvestSources;
                    continue;
                }
                addHarvestSkill(source.requiredSkill);
            }

            if (std::find(sourceEntries.begin(), sourceEntries.end(), source.creatureEntry) == sourceEntries.end())
                sourceEntries.push_back(source.creatureEntry);
        }
        if (sourceEntries.empty())
        {
            if (error)
            {
                if (blockedHarvestSources != 0)
                    *error = "All qualifying sources require a missing profession, skill, or tool.";
                else if (supportsMethod(Sbrpg::Materials::AcquisitionMethod::Transformation))
                    *error = "This material is a crafted or transformation output, not a direct farm target.";
                else if (supportsMethod(Sbrpg::Materials::AcquisitionMethod::Fishing))
                    *error = "Fishing material runs require the module-owned fishing controller.";
                else if (supportsMethod(Sbrpg::Materials::AcquisitionMethod::GameObjectNode))
                    *error = "Use the node-farming controls for this gathering material until material-target node bridging is enabled.";
                else
                    *error = Acore::StringFormat("No supported creature sources meet the {:.2f}% minimum chance.", minimumChance);
            }
            DisableOwnedSelfBot(player, enabledBySbrpg);
            return false;
        }

        std::vector<Sbrpg::Materials::Hotspot> hotspots =
            Sbrpg::Materials::HotspotPlanner::Build(
                Sbrpg::Materials::CreatureSpawnRepository::Load(player, sourceEntries));
        hotspots = Sbrpg::Materials::HotspotPlanner::Plan(player, std::move(hotspots));
        if (hotspots.empty())
        { DisableOwnedSelfBot(player, enabledBySbrpg); if (error) *error = "No reachable creature hotspots found in the current zone."; return false; }

        Sbrpg::Materials::MaterialFarmState& state = materialStates[player->GetGUID()];
        state = Sbrpg::Materials::MaterialFarmState();
        state.active = true;
        state.selfBotEnabledBySbrpg = enabledBySbrpg;
        state.itemId = itemId;
        state.quantityGoal = quantityGoal;
        state.inventoryStart = player->GetItemCount(itemId, true);
        state.inventoryCount = state.inventoryStart;
        Sbrpg::BeginActivitySession(state.session, player, durationMinutes, getMSTime());
        state.mapId = player->GetMapId();
        state.zoneId = player->GetZoneId();
        state.creatureEntries = std::move(sourceEntries);
        state.needsSkinning = std::find(harvestSkills.begin(), harvestSkills.end(), SKILL_SKINNING) != harvestSkills.end();
        state.harvestSkills = std::move(harvestSkills);
        state.hotspots = std::move(hotspots);
        state.previousLootStrategy = ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Get()->GetName();
        state.lootStrategy.SetStrategy("loot");
        // Ensure stock loot exists even when selfbot was just enabled; the
        // lease records whether SBRPG added it so it can be restored on stop.
        state.lootStrategy.Acquire(ai);
        state.lootStrategy.Suspend(ai);
        ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Set(
            LootStrategyValue::instance("all"));
        state.lootStrategyOverridden = true;
        state.materialLootStrategy.SetStrategy("sbrpg material loot");
        state.materialLootStrategy.Acquire(ai);
        if (!state.harvestSkills.empty())
        {
            state.gatherStrategy.SetStrategy("gather");
            state.gatherStrategy.Acquire(ai);
        }
        state.grindStrategy.SetStrategy("grind");
        state.grindStrategy.Suspend(ai);
        state.rpgStrategy.SetStrategy("rpg");
        state.rpgStrategy.Suspend(ai);
        state.materialStrategy.SetStrategy("sbrpg material");
        state.materialStrategy.Acquire(ai);
        // Reuse playerbot's mount controller. It evaluates the character's
        // learned riding skill and available mounts, then chooses the best
        // valid ground or flying mount automatically. Fishing may mount while
        // travelling to water or between pools, then dismount before casting.
        state.mountStrategy.SetStrategy("mount");
        state.mountStrategy.Acquire(ai);
        return true;
    }

    void SendAddon(Player* player, ChatMsg chatType, std::string const& payload);

    void SetMaterialPhase(Player* player, Sbrpg::Materials::MaterialFarmState& state, std::string phase)
    {
        if (state.phase != phase)
        {
            state.phase = std::move(phase);
            Debug(player, Acore::StringFormat("material phase: {}", state.phase));
            if (state.fishing)
                SendAddon(player, CHAT_MSG_WHISPER, Sbrpg::Protocol::Build("MATERIAL_STATUS", {
                    "0", state.active ? "1" : "0", std::to_string(state.itemId),
                    std::to_string(state.gatheredItems), std::to_string(state.quantityGoal),
                    std::to_string(state.kills), "0", "0", state.phase
                }));
        }
    }

    void UpdateMaterialReturn(Player* player)
    {
        if (!player)
            return;
        auto it = materialStates.find(player->GetGUID());
        if (it == materialStates.end() || !it->second.active || it->second.session.returnRequested)
            return;
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        if (!ai)
            return;
        uint32 const reservedBagPercent = runtimeSettings.materialReservedBagPercent;
        uint8 const bagThreshold = static_cast<uint8>(100u - std::min(100u, reservedBagPercent));
        uint32 const now = getMSTime();
        bool const timedOut = Sbrpg::ActivityTimedOut(it->second.session, now);
        bool const quantityReached = it->second.quantityGoal != 0 &&
            it->second.gatheredItems >= it->second.quantityGoal;
        bool const bagsFull = ai->GetAiObjectContext()->GetValue<uint8>("bag space")->Get() >= bagThreshold;
        if (!timedOut && !quantityReached && !bagsFull)
            return;
        std::string const reason = bagsFull ? "bag reserve reached" :
            quantityReached ? "quantity goal reached" : "timed session complete";
        Sbrpg::RequestActivityReturn(it->second.session, now, reason);
        if (!it->second.returnNotified)
        {
            it->second.returnNotified = true;
            if (WorldSession* session = player->GetSession())
                ChatHandler(session).PSendSysMessage(
                    "[SBRPG] Material session complete: {}. Returning to the session start.", reason);
        }
        Debug(player, Acore::StringFormat("material return requested: {}", reason));
    }

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

    class SelfbotMaterialTravel : public MovementAction
    {
    public:
        SelfbotMaterialTravel(PlayerbotAI* ai) : MovementAction(ai, "sbrpg material travel") { }
        bool MoveToHotspot(uint32 mapId, float x, float y, float z)
        {
            if (!PrepareTravelMove(bot))
                return false;
            return MoveTo(mapId, x, y, z, false, false, false, false,
                MovementPriority::MOVEMENT_NORMAL, false);
        }
        bool MoveNearLoot(WorldObject* object)
        {
            return object && MoveNear(object, sPlayerbotAIConfig.contactDistance,
                MovementPriority::MOVEMENT_NORMAL);
        }
    };

    class SelfbotMaterialAttackAction : public AttackAction
    {
    public:
        SelfbotMaterialAttackAction(PlayerbotAI* ai) : AttackAction(ai, "sbrpg material attack"), travel(ai) { }

        bool Execute(Event /*event*/) override
        {
            auto it = materialStates.find(bot->GetGUID());
            if (it == materialStates.end() || !it->second.active)
                return false;
            Sbrpg::Materials::MaterialFarmState& state = it->second;
            if (state.mapId != bot->GetMapId())
            {
                StopMaterial(bot, "map changed before return");
                return false;
            }

            uint32 const now = getMSTime();
            uint32 const actionDelayMs = runtimeSettings.actionDelayMs;
            if (state.lastActionMs != 0 && now - state.lastActionMs < actionDelayMs)
                return false;
            state.lastActionMs = now;
            UpdateMaterialReturn(bot);
            if (state.fishing)
                return HandleFishing(state, now);
            if (bot->IsInCombat())
            {
                SetMaterialPhase(bot, state, state.session.returnRequested ?
                    "combat before return" : "combat");
                return false;
            }

            // Stock playerbots exclusively owns corpse approach, opening,
            // looting and skinning. SBRPG only keeps the exact killed GUID in
            // its stack and yields while LootObject says that corpse remains
            // actionable. Never issue a competing MoveNear/OpenLoot request.
            LootObject activeLoot = AI_VALUE(LootObject, "loot target");
            if (!activeLoot.IsEmpty())
            {
                // Give stock loot time to complete or replace a target. An
                // empty corpse can only be discovered after opening it, so an
                // invalid target must not stall material farming forever.
                WorldObject* activeLootObject = activeLoot.GetWorldObject(bot);
                bool const activeLootInRange = activeLootObject &&
                    bot->GetDistance(activeLootObject) <= sPlayerbotAIConfig.contactDistance + 0.5f;
                if ((!activeLootInRange && activeLoot.IsLootPossible(bot)) ||
                    !bot->GetLootGUID().IsEmpty())
                {
                    state.invalidLootSinceMs = 0;
                    SetMaterialPhase(bot, state, state.session.returnRequested ?
                        "finishing loot before return" : "looting");
                    return false;
                }
                if (state.invalidLootSinceMs == 0)
                    state.invalidLootSinceMs = now;
                if (bot->IsInCombat() || now - state.invalidLootSinceMs < 10000 ||
                    !bot->GetLootGUID().IsEmpty())
                {
                    SetMaterialPhase(bot, state, "waiting for stock loot target recovery");
                    return false;
                }
                Debug(bot, Acore::StringFormat("material releasing stale loot target after 10s: guid {}",
                    activeLoot.guid.GetCounter()));
                AI_VALUE(LootObjectStack*, "available loot")->Remove(activeLoot.guid);
                context->GetValue<LootObject>("loot target")->Set(LootObject());
                state.invalidLootSinceMs = 0;
            }
            if (!bot->GetLootGUID().IsEmpty())
            {
                SetMaterialPhase(bot, state, state.session.returnRequested ? "finishing loot before return" : "looting");
                return false;
            }

            // Explicitly approach queued combat corpses/chests on return (and
            // during normal routing) so stock loot can select them promptly.
            if (AI_VALUE(bool, "has available loot"))
            {
                LootObject pendingLoot = AI_VALUE(LootObjectStack*, "available loot")->GetLoot();
                WorldObject* pendingObject = pendingLoot.GetWorldObject(bot);
                if (pendingObject && bot->GetDistance(pendingObject) > sPlayerbotAIConfig.contactDistance + 0.5f)
                {
                    SetMaterialPhase(bot, state, state.session.returnRequested ?
                        "approaching loot before return" : "approaching nearby loot");
                    if (!bot->isMoving() && !bot->IsNonMeleeSpellCast(true))
                        travel.MoveNearLoot(pendingObject);
                    return false;
                }
                SetMaterialPhase(bot, state, state.session.returnRequested ?
                    "finishing nearby loot before return" : "yielding to nearby loot");
                return false;
            }

            if (!state.target.IsEmpty())
            {
                Creature* corpse = botAI->GetCreature(state.target);
                if (corpse && corpse->getDeathState() == DeathState::Corpse)
                {
                    bool const needsLoot = corpse->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                    LootObject corpseLoot(bot, state.target);
                    // Normal corpse loot always has priority. Do not expose a
                    // harvest target to stock gather until the lootable flag is
                    // gone, otherwise a ranged bot can harvest/interrupt while
                    // regular corpse items remain.
                    SkillType const corpseSkill = static_cast<SkillType>(corpseLoot.skillId);
                    bool const harvestReady = !needsLoot &&
                        std::find(state.harvestSkills.begin(), state.harvestSkills.end(), corpseSkill) !=
                            state.harvestSkills.end() &&
                        HasHarvestTool(bot, corpseSkill) && botAI->HasSkill(corpseSkill) &&
                        bot->GetSkillValue(corpseSkill) >= corpseLoot.reqSkillValue;
                    bool const corpseActionable = needsLoot || harvestReady;
                    if (corpseActionable)
                    {
                        if (state.corpseWaitSinceMs == 0)
                            state.corpseWaitSinceMs = now;
                        // Ranged kills commonly land outside the stock
                        // Approach all the way to contact range so the stock
                        // loot action can interact reliably, using the same
                        // bounded mmap movement as hotspot travel.
                        if (bot->GetDistance(corpse) > sPlayerbotAIConfig.contactDistance + 0.5f)
                        {
                            SetMaterialPhase(bot, state, state.session.returnRequested ?
                                "approaching corpse before return" : "approaching corpse");
                            if (!bot->isMoving() && !bot->IsNonMeleeSpellCast(true))
                            {
                                Sbrpg::RouteStep corpseStep;
                                bool const routeBuilt = Sbrpg::BuildRouteStep(bot, corpse->GetPositionX(),
                                    corpse->GetPositionY(), corpse->GetPositionZ(), corpseStep);
                                bool const moveIssued = routeBuilt && IssueBoundedMove(corpseStep);
                                if (state.lastCorpseDebugMs == 0 || now - state.lastCorpseDebugMs >= 5000)
                                {
                                    state.lastCorpseDebugMs = now;
                                    Debug(bot, Acore::StringFormat(
                                        "material corpse approach: entry {}, distance {:.1f}, route {}, path {}, points {}, move {}, lootable {}, skill {}, lootPossible {}",
                                        corpse->GetEntry(), bot->GetDistance(corpse), routeBuilt ? "yes" : "no",
                                        static_cast<uint32>(corpseStep.type), corpseStep.corridor.size(),
                                        moveIssued ? "issued" : "blocked", needsLoot ? "yes" : "no",
                                        static_cast<uint32>(corpseLoot.skillId), corpseLoot.IsLootPossible(bot) ? "yes" : "no"));
                                }
                            }
                            return false;
                        }
                        if (corpseLoot.IsLootPossible(bot))
                        {
                            bool const queued = AI_VALUE(LootObjectStack*, "available loot")->Add(state.target);
                            if (state.lastCorpseDebugMs == 0 || now - state.lastCorpseDebugMs >= 5000)
                            {
                                state.lastCorpseDebugMs = now;
                                Debug(bot, Acore::StringFormat(
                                    "material corpse loot handoff: entry {}, distance {:.1f}, queued {}, lootable {}, skill {}",
                                    corpse->GetEntry(), bot->GetDistance(corpse), queued ? "yes" : "already queued",
                                    needsLoot ? "yes" : "no", static_cast<uint32>(corpseLoot.skillId)));
                            }
                        }
                        uint32 const corpseWaitLimit = 10000;
                        if (now - state.corpseWaitSinceMs < corpseWaitLimit)
                        {
                            SetMaterialPhase(bot, state, state.session.returnRequested ?
                                "finishing combat corpse before return" : "waiting for stock loot");
                            return false;
                        }
                        AI_VALUE(LootObjectStack*, "available loot")->Remove(state.target);
                        ++state.corpseTimeouts;
                        Debug(bot, Acore::StringFormat("material corpse timed out; skipping entry {}",
                            corpse->GetEntry()));
                    }
                }
                Debug(bot, Acore::StringFormat("material corpse objective cleared: guid {}, return {}",
                    state.target.GetCounter(), state.session.returnRequested ? "yes" : "no"));
                if (state.returnCombatCorpse)
                    Debug(bot, "material return combat corpse resolved; resuming return route");
                state.target.Clear();
                state.corpseWaitSinceMs = 0;
                state.lastCorpseDebugMs = 0;
                state.invalidLootSinceMs = 0;
                state.returnCombatCorpse = false;
            }

            if (state.session.returnRequested)
            {
                state.gatherStrategy.Suspend(botAI);
                // A nearby queued corpse still belongs to the loot strategy.
                // Do not start return movement until it has had a chance to
                // select and finish that corpse.
                if (AI_VALUE(bool, "has available loot"))
                {
                    SetMaterialPhase(bot, state, "finishing nearby loot before return");
                    return false;
                }
                SetMaterialPhase(bot, state, "returning");
                if (IsRecovering())
                    return false;
                return ReturnHome(state, now);
            }
            if (IsRecovering())
            {
                SetMaterialPhase(bot, state, "recovering");
                return false;
            }

            Creature* target = nullptr;
            if (!state.target.IsEmpty())
                target = botAI->GetCreature(state.target);
            if (target && !IsValidTarget(state, target))
                target = nullptr;
            if (!target)
            {
                state.target.Clear();
                float distance = std::numeric_limits<float>::max();
                // Stock possible-target values intentionally omit many gray
                // creatures. Scan unfriendly units directly so every live
                // eligible source entry (including gray boars/wolves) can be
                // considered without changing stock targeting globally.
                std::list<Unit*> nearbyUnits;
                Acore::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot,
                    sPlayerbotAIConfig.sightDistance);
                Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck>
                    searcher(bot, nearbyUnits, check);
                Cell::VisitObjects(bot, searcher, sPlayerbotAIConfig.sightDistance);
                for (Unit* unit : nearbyUnits)
                {
                    Creature* candidate = unit ? unit->ToCreature() : nullptr;
                    if (!IsValidTarget(state, candidate))
                        continue;
                    float const candidateDistance = bot->GetDistance(candidate);
                    if (!target || candidateDistance < distance)
                    {
                        target = candidate;
                        distance = candidateDistance;
                    }
                }
                if (!target)
                    return MoveToNextHotspot(state);
                state.target = target->GetGUID();
                state.targetSinceMs = getMSTime();
                state.corpseWaitSinceMs = 0;
                Debug(bot, Acore::StringFormat("material target: {} (entry {}, gray allowed)",
                    target->GetName(), target->GetEntry()));
            }

            // AttackAction::Attack performs the normal target/loot/combat handoff.
            // This call is the only deliberate gray-mob bypass in the module.
            return Attack(target);
        }

    private:
        bool HandleFishing(Sbrpg::Materials::MaterialFarmState& state, uint32 now)
        {
            if (state.session.returnRequested)
            {
                // Stop cancels future casts, but an active cast/bobber is a
                // normal player action that must be allowed to finish before
                // returning home.
                if (bot->IsNonMeleeSpellCast(true))
                {
                    SetMaterialPhase(bot, state, "finishing fishing cast before return");
                    return false;
                }

                std::stringstream configuredBobbers(runtimeSettings.fishingBobberEntries);
                std::string bobberValue;
                while (std::getline(configuredBobbers, bobberValue, ','))
                {
                    uint32 bobberEntry = 0;
                    try { bobberEntry = static_cast<uint32>(std::stoul(bobberValue)); }
                    catch (...) { continue; }
                    std::list<GameObject*> bobbers;
                    bot->GetGameObjectListWithEntryInGrid(bobbers, bobberEntry, 30.0f);
                    for (GameObject* bobber : bobbers)
                    {
                        if (!bobber || bobber->GetOwnerGUID() != bot->GetGUID() ||
                            bobber->GetGoType() != GAMEOBJECT_TYPE_FISHINGNODE)
                            continue;
                        if (bobber->getLootState() == GO_READY)
                        {
                            SetMaterialPhase(bot, state, "reeling fishing catch before return");
                            bobber->Use(bot);
                        }
                        else
                            SetMaterialPhase(bot, state, "waiting for fishing bite before return");
                        return false;
                    }
                }

                LootObject loot = AI_VALUE(LootObject, "loot target");
                if (loot.IsEmpty() && bot->GetLootGUID().IsEmpty())
                    return ReturnHome(state, now);
                SetMaterialPhase(bot, state, "finishing fishing loot before return");
                return false;
            }
            if (bot->IsInCombat() || bot->IsNonMeleeSpellCast(true))
                return false;


            if (!state.fishingPoolMode && !state.fishingPools.empty() &&
                now - state.fishingPoolLastScanMs >= 60000)
            {
                state.fishingPoolMode = true;
                state.fishingPoolMisses = 0;
                state.fishingPoolIndex = 0;
            }
            if (state.fishingPoolMode && !state.fishingPools.empty())
            {
                if (state.fishingPoolIndex >= state.fishingPools.size())
                    state.fishingPoolIndex = 0;
                auto& pool = state.fishingPools[state.fishingPoolIndex];
                std::list<GameObject*> livePools;
                bot->GetGameObjectListWithEntryInGrid(livePools, pool.entry, 20.0f);
                bool poolPresent = false;
                for (GameObject* object : livePools)
                    if (object && object->isSpawned() &&
                        bot->GetExactDist(object) <= 20.0f)
                    { poolPresent = true; break; }
                if (!poolPresent)
                {
                    ++state.fishingPoolMisses;
                    state.fishingPoolIndex = (state.fishingPoolIndex + 1) % state.fishingPools.size();
                    state.fishingPoolStep = Sbrpg::RouteStep();
                    if (state.fishingPoolMisses >= state.fishingPools.size())
                    {
                        state.fishingPoolMode = false;
                        state.fishingPoolLastScanMs = now;
                        state.fishingPoolMisses = 0;
                        SetMaterialPhase(bot, state, "no active fishing pools; open water fallback");
                    }
                    return false;
                }
                state.fishingPoolMisses = 0;
                if (bot->GetExactDist(pool.x, pool.y, pool.z) > 10.0f)
                {
                    if ((bot->movespline && !bot->movespline->Finalized()) || bot->isMoving())
                        return false;
                    Sbrpg::RouteStep step;
                    if (Sbrpg::BuildRouteStep(bot, pool.x, pool.y, pool.z, step) &&
                        IssueBoundedMove(step))
                    {
                        state.fishingPoolStep = step;
                        SetMaterialPhase(bot, state, "travelling to fishing pool");
                    }
                    else
                    {
                        state.fishingPoolIndex = (state.fishingPoolIndex + 1) % state.fishingPools.size();
                    }
                    return false;
                }
                SetMaterialPhase(bot, state, "fishing pool");
                if (bot->IsMounted())
                {
                    bot->Dismount();
                    return true;
                }
            }

            // The stock action searches only about 60 yards. SBRPG keeps the
            // same normal-player land/water rules but expands the bounded
            // search so fishing can reach water from a distant inland start.
            bool customWaterSearch = false;
            if (!state.fishingPoolMode && state.fishingLastWaterSearchMs == 0)
            {
                state.fishingLastWaterSearchMs = now;
                // Clear any stale stock-playerbot fishing spot first. The
                // configured cast distance must not be silently replaced by
                // the stock action's wider default search.
                SET_AI_VALUE(WorldPosition, "fishing spot", WorldPosition());
                WorldPosition water = FindWaterRadial(bot, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(),
                    bot->GetMap(), bot->GetPhaseMask(), 10.0f, runtimeSettings.fishingSearchDistance, 10.0f, false);
                if (water.IsValid())
                {
                    float const angle = bot->GetAngle(water.GetPositionX(), water.GetPositionY());
                    WorldPosition land = FindLandFromPosition(botAI, 0.0f, runtimeSettings.fishingCastDistance,
                        1.0f, angle, water, runtimeSettings.fishingSearchDistance, false);
                    if (land.IsValid())
                    {
                        SET_AI_VALUE(WorldPosition, "fishing spot", land);
                        state.fishingCustomWaterSpot = true;
                    }
                }
                customWaterSearch = true;
            }

            // Open-water fishing must use the configured module-owned spot.
            // Only use the stock search if the custom search was not needed
            // (for example, a pool session or an already valid stock spot).
            MoveNearWaterAction moveNearWater(botAI);
            bool const waterSpotAvailable = customWaterSearch ?
                AI_VALUE(WorldPosition, "fishing spot").IsValid() : moveNearWater.isPossible();
            if (moveNearWater.isUseful())
            {
                SetMaterialPhase(bot, state, "moving to open water");
                WorldPosition fishingSpot = AI_VALUE(WorldPosition, "fishing spot");
                Sbrpg::RouteStep waterStep;
                if (fishingSpot.IsValid() && Sbrpg::BuildRouteStep(bot,
                    fishingSpot.GetPositionX(), fishingSpot.GetPositionY(), fishingSpot.GetPositionZ(), waterStep))
                {
                    if (!PrepareTravelMove(bot))
                        return true;
                    if (Sbrpg::FollowRouteStep(bot, waterStep) || IssueBoundedMove(waterStep))
                        return true;
                    Debug(bot, Acore::StringFormat("water route built but movement was rejected ({:.1f}, {:.1f}, {:.1f})",
                        fishingSpot.GetPositionX(), fishingSpot.GetPositionY(), fishingSpot.GetPositionZ()));
                }
                else if (!fishingSpot.IsValid())
                {
                    if (state.fishingWaterLastReportMs == 0 || now - state.fishingWaterLastReportMs >= 5000)
                    {
                        state.fishingWaterLastReportMs = now;
                        ChatHandler(bot->GetSession()).PSendSysMessage(
                            "[SBRPG] No reachable fishing water found from the current position; check VMAP/MMap data and fishing distance settings.");
                    }
                    SetMaterialPhase(bot, state, "water search failed");
                    Sbrpg::RequestActivityReturn(state.session, now, "no reachable fishing water");
                    return true;
                }
                // The stock action may still succeed when its cached spot is
                // not mmap-routeable from this controller tick. Apply the
                // same mount gate before allowing that fallback movement.
                if (!PrepareTravelMove(bot))
                    return true;
                if (moveNearWater.Execute(Event("sbrpg move near water")))
                    return true;
                if (state.fishingWaterLastReportMs == 0 || now - state.fishingWaterLastReportMs >= 5000)
                {
                    state.fishingWaterLastReportMs = now;
                    ChatHandler(bot->GetSession()).PSendSysMessage(
                        "[SBRPG] Fishing water route could not be started; movement was rejected.");
                }
                return true;
            }
            if (!waterSpotAvailable && !AI_VALUE(WorldPosition, "fishing spot").IsValid())
            {
                SetMaterialPhase(bot, state, "no reachable water found");
                Sbrpg::RequestActivityReturn(state.session, now, "no reachable water");
                return true;
            }

            if (bot->IsMounted())
            {
                bot->Dismount();
                return true;
            }

            std::vector<uint32> bobberEntries;
            std::string configured = runtimeSettings.fishingBobberEntries;
            bool hasOwnedBobber = false;
            std::stringstream entries(configured);
            std::string value;
            while (std::getline(entries, value, ','))
            {
                try { bobberEntries.push_back(static_cast<uint32>(std::stoul(value))); }
                catch (...) { }
            }
            std::list<GameObject*> nearby;
            for (uint32 entry : bobberEntries)
                bot->GetGameObjectListWithEntryInGrid(nearby, entry, 30.0f);
            for (GameObject* bobber : nearby)
            {
                if (bobber && bobber->GetOwnerGUID() == bot->GetGUID() &&
                    bobber->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE)
                {
                    hasOwnedBobber = true;
                    if (bobber->getLootState() == GO_READY)
                    {
                        SetMaterialPhase(bot, state, "reeling fishing catch");
                        bobber->Use(bot);
                        state.fishingLastCastMs = now;
                        return true;
                    }
                }
            }
            if (hasOwnedBobber)
            {
                SetMaterialPhase(bot, state, "waiting for fishing bite");
                return false;
            }
            if (state.fishingLastCastMs == 0 || now - state.fishingLastCastMs >= 5000)
            {
                // Check the lure immediately before every real cast. A lure
                // action may take a tick; never mark fishing as failed when a
                // lure is absent or unusable.
                if (runtimeSettings.useLures)
                {
                    Item* pole = FindEquippedFishingPole(bot);
                    if (pole && pole->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT) == 0)
                        if (Item* lure = FindFishingLure(bot))
                        {
                            SetMaterialPhase(bot, state, "applying optional fishing lure");
                            SbrpgLureUseAction lureAction(botAI);
                            if (lureAction.Apply(lure, pole))
                                return true;
                            if (state.fishingLureLastReportMs == 0 || now - state.fishingLureLastReportMs >= 10000)
                            {
                                state.fishingLureLastReportMs = now;
                                ChatHandler(bot->GetSession()).PSendSysMessage(
                                    "[SBRPG] Fishing lure found but could not be applied; continuing without lure.");
                            }
                        }
                }
                SetMaterialPhase(bot, state, "casting fishing line");
                bot->CastSpell(bot, 18248, true);
                state.fishingLastCastMs = now;
                return true;
            }
            SetMaterialPhase(bot, state, "waiting for fishing bobber");
            return false;
        }

        bool IsRecovering() const
        {
            if (bot->IsNonMeleeSpellCast(true))
                return true;
            if (bot->HasAura(25990))
                return true;
            if (!bot->IsSitState())
                return false;
            return bot->GetHealthPct() < 100.0f ||
                (bot->GetMaxPower(POWER_MANA) > 0 && bot->GetPowerPct(POWER_MANA) < 100.0f);
        }

        bool IssueBoundedMove(Sbrpg::RouteStep const& step)
        {
            float moveX = 0.0f, moveY = 0.0f, moveZ = 0.0f;
            bool found = false;
            for (G3D::Vector3 const& point : step.corridor)
            {
                float const distance = bot->GetExactDist(point.x, point.y, point.z);
                if (distance > 25.0f)
                    break;
                if (distance > 2.0f)
                {
                    moveX = point.x; moveY = point.y; moveZ = point.z;
                    found = true;
                }
            }
            return found && travel.MoveToHotspot(bot->GetMapId(), moveX, moveY, moveZ);
        }

        bool ReturnHome(Sbrpg::Materials::MaterialFarmState& state, uint32 now)
        {
            float const distance = bot->GetExactDist(
                state.session.startX, state.session.startY, state.session.startZ);
            if (Sbrpg::IsAtActivityStart(state.session, bot, 8.0f))
            {
                bot->SetFacingTo(state.session.startO);
                if (WorldSession* session = bot->GetSession())
                    ChatHandler(session).PSendSysMessage("[SBRPG] Material run returned to its start position.");
                Debug(bot, "material run finished at activity start");
                StopMaterial(bot, state.session.returnReason.empty() ? "completed" : state.session.returnReason);
                return true;
            }

            if (state.returnProgressMs == 0)
            {
                state.returnProgressMs = now;
                state.returnLastDistance = distance;
            }
            else if (now - state.returnProgressMs >= 10000)
            {
                if (distance >= state.returnLastDistance - 3.0f)
                    ++state.returnStalls;
                else
                    state.returnStalls = 0;
                state.returnProgressMs = now;
                state.returnLastDistance = distance;
                if (state.returnStalls >= 3)
                {
                    state.returnStep = Sbrpg::RouteStep();
                    state.returnStalls = 0;
                    Debug(bot, "material return made no progress; rebuilding mmap route");
                }
            }
            if ((bot->movespline && !bot->movespline->Finalized()) || bot->isMoving())
                return false;

            Sbrpg::RouteStep step;
            if (!Sbrpg::BuildRouteStep(bot, state.session.startX, state.session.startY,
                state.session.startZ, step))
            {
                if (!((step.type & PATHFIND_FARFROMPOLY) && Sbrpg::BuildRecoveryStep(bot, step)))
                {
                    SetMaterialPhase(bot, state, "return route retry");
                    Debug(bot, "material return has no safe mmap segment yet; retrying");
                    return false;
                }
            }
            state.returnStep = step;
            state.returnStepBuiltMs = now;
            // RouteStep::complete means mmap reached the requested destination;
            // it does not mean the player has arrived. Always walk the corridor.
            if (IssueBoundedMove(state.returnStep))
            {
                SetMaterialPhase(bot, state, "returning");
                Debug(bot, Acore::StringFormat("material return moving; {:.1f} yards remain", distance));
                return true;
            }
            SetMaterialPhase(bot, state, "return movement retry");
            return false;
        }

        bool MoveToNextHotspot(Sbrpg::Materials::MaterialFarmState& state)
        {
            if (state.hotspots.empty())
                return false;
            if (state.hotspotIndex >= state.hotspots.size())
                state.hotspotIndex = 0;

            Sbrpg::Materials::Hotspot const& hotspot = state.hotspots[state.hotspotIndex];
            float const distance = bot->GetExactDist(hotspot.x, hotspot.y, hotspot.z);
            uint32 const now = getMSTime();
            if (distance <= 10.0f)
            {
                state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
                state.hotspotStep = Sbrpg::RouteStep();
                state.hotspotProgressMs = 0;
                state.hotspotStalls = 0;
                SetMaterialPhase(bot, state, "scanning hotspot");
                return true;
            }
            if (state.hotspotProgressMs == 0)
            {
                state.hotspotProgressMs = now;
                state.hotspotLastDistance = distance;
                Debug(bot, Acore::StringFormat("material hotspot objective: {} ({} spawns)",
                    hotspot.id, hotspot.spawnCount));
            }
            else if (now - state.hotspotProgressMs >= 10000)
            {
                if (distance >= state.hotspotLastDistance - 3.0f)
                    ++state.hotspotStalls;
                else
                    state.hotspotStalls = 0;
                state.hotspotProgressMs = now;
                state.hotspotLastDistance = distance;
                if (state.hotspotStalls >= 3)
                {
                    Debug(bot, Acore::StringFormat("material hotspot {} stalled; selecting next", hotspot.id));
                    state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
                    state.hotspotStep = Sbrpg::RouteStep();
                    state.hotspotProgressMs = 0;
                    state.hotspotStalls = 0;
                    return false;
                }
            }
            if ((bot->movespline && !bot->movespline->Finalized()) || bot->isMoving())
                return false;

            Sbrpg::RouteStep step;
            if (!hotspot.reachable || !Sbrpg::BuildRouteStep(bot, hotspot.x, hotspot.y, hotspot.z, step))
            {
                Debug(bot, Acore::StringFormat("material hotspot {} has no safe mmap segment; selecting next", hotspot.id));
                state.hotspotIndex = (state.hotspotIndex + 1) % state.hotspots.size();
                state.hotspotProgressMs = 0;
                state.hotspotStalls = 0;
                return false;
            }
            state.hotspotStep = step;
            state.hotspotStepBuiltMs = now;
            if (IssueBoundedMove(state.hotspotStep))
            {
                SetMaterialPhase(bot, state, "travelling to hotspot");
                return true;
            }
            SetMaterialPhase(bot, state, "hotspot movement retry");
            return false;
        }

        bool IsValidTarget(Sbrpg::Materials::MaterialFarmState const& state, Creature* creature) const
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
        SelfbotMaterialTravel travel;
    };

    class SelfbotRpgFarmAction : public MovementAction
    {
    public:
        SelfbotRpgFarmAction(PlayerbotAI* ai) : MovementAction(ai, "sbrpg farm") { }

        bool Execute(Event /*event*/) override
        {
            Sbrpg::FarmState const* state = Sbrpg::Get(bot);
            if (!state || !state->active)
                return false;
            Sbrpg::FarmState& mutableState = states[bot->GetGUID()];
            uint32 const actionNow = getMSTime();
            uint32 const actionDelayMs = runtimeSettings.actionDelayMs;
            if (mutableState.lastActionMs != 0 && actionNow - mutableState.lastActionMs < actionDelayMs)
                return false;
            mutableState.lastActionMs = actionNow;
            if (bot->IsInCombat())
            {
                mutableState.combatInterrupted = true;
                SetPhase(mutableState, Sbrpg::FarmPhase::CombatPaused,
                    mutableState.session.returnRequested ? "combat active; return-home objective retained" : "combat active; farm destination retained");
                mutableState.step.valid = false;
                mutableState.stepIssued = false;
                return false;
            }
            if (mutableState.combatInterrupted)
            {
                mutableState.combatInterrupted = false;
                mutableState.step.valid = false;
                mutableState.stepIssued = false;
                mutableState.targetSinceMs = 0;
                mutableState.stuckChecks = 0;
                if (bot->isMoving())
                    bot->StopMoving();
                SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "combat ended; checking nearby loot");
            }
            if (bot->isDead())
            {
                SetPhase(mutableState, Sbrpg::FarmPhase::Waiting, "dead; waiting for recovery");
                return false;
            }
            if (state->mapId != bot->GetMapId())
            {
                Debug(bot, "farm stopped after map change; route coordinates are map-local");
                Sbrpg::Finish(bot, "map changed during farming");
                return false;
            }

            uint32 const now = getMSTime();
            if (!mutableState.session.returnRequested)
            {
                bool const timedOut = Sbrpg::ActivityTimedOut(mutableState.session, now);
                bool const bagsFull = AI_VALUE(uint8, "bag space") >= 100;
                if (timedOut || bagsFull)
                {
                    Sbrpg::RequestActivityReturn(mutableState.session, now,
                        bagsFull ? "bags full" : "timed session complete");
                    mutableState.currentSpawn = 0;
                    mutableState.pendingGatherNode = ObjectGuid::Empty;
                    mutableState.pendingGatherSinceMs = 0;
                    mutableState.step.valid = false;
                    mutableState.stepIssued = false;
                    mutableState.targetSinceMs = 0;
                    mutableState.stuckChecks = 0;
                    // Stop discovering new gathering nodes during return, but
                    // keep stock loot enabled so combat corpses can finish.
                    mutableState.gatherStrategy.Suspend(botAI);
                    for (ObjectGuid const& guid : mutableState.liveCache.Guids())
                        AI_VALUE(LootObjectStack*, "available loot")->Remove(guid);
                    SetPhase(mutableState, Sbrpg::FarmPhase::Returning,
                        mutableState.session.returnReason + "; returning to farm start");
                }
            }

            // Stock loot always preempts farming. This covers corpses as well
            // as gathering nodes and deliberately does not inspect, clear, or
            // replace the target: playerbots must finish/release the complete
            // loot window using its normal lifecycle.
            LootObject stockLootTarget = AI_VALUE(LootObject, "loot target");
            if (!stockLootTarget.IsEmpty())
            {
                WorldObject* stockLootObject = stockLootTarget.GetWorldObject(bot);
                bool const stockLootInRange = stockLootObject &&
                    bot->GetDistance(stockLootObject) <= sPlayerbotAIConfig.contactDistance + 0.5f;
                if (mutableState.lootWaitSinceMs == 0)
                    mutableState.lootWaitSinceMs = now;
                if ((!stockLootInRange && stockLootTarget.IsLootPossible(bot)) ||
                    !bot->GetLootGUID().IsEmpty() || bot->IsInCombat() ||
                    now - mutableState.lootWaitSinceMs < 10000)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                        mutableState.session.returnRequested ? "finishing active loot before return" : "stock playerbots owns active loot target");
                    return false;
                }
                Debug(bot, Acore::StringFormat("releasing stale loot target after 10s: guid {}",
                    stockLootTarget.guid.GetCounter()));
                AI_VALUE(LootObjectStack*, "available loot")->Remove(stockLootTarget.guid);
                botAI->GetAiObjectContext()->GetValue<LootObject>("loot target")->Set(LootObject());
                mutableState.lootWaitSinceMs = 0;
            }
            else
                mutableState.lootWaitSinceMs = 0;

            // Keep multiple post-combat corpses available until stock loot has
            // selected them. Expire only unselected corpses after 10 seconds;
            // an actively selected/open corpse is handled by the stock-target
            // timeout above.
            for (auto lootIt = mutableState.combatLootSinceMs.begin();
                lootIt != mutableState.combatLootSinceMs.end(); )
            {
                ObjectGuid const guid = lootIt->first;
                LootObject const selected = AI_VALUE(LootObject, "loot target");
                bool const selectedOrOpen = selected.guid == guid || bot->GetLootGUID() == guid;
                if (!selectedOrOpen && now - lootIt->second >= 10000)
                {
                    AI_VALUE(LootObjectStack*, "available loot")->Remove(guid);
                    Debug(bot, Acore::StringFormat("combat corpse loot timed out after 10s: guid {}", guid.GetCounter()));
                    lootIt = mutableState.combatLootSinceMs.erase(lootIt);
                }
                else
                    ++lootIt;
            }

            // Available loot can be a nearby chest/gameobject or a combat
            // corpse that has not yet become the selected loot target. Approach
            // it with normal bounded playerbot movement before yielding to the
            // stock strategy for opening and looting.
            if (AI_VALUE(bool, "has available loot"))
            {
                LootObject pendingLoot = AI_VALUE(LootObjectStack*, "available loot")->GetLoot();
                WorldObject* pendingObject = pendingLoot.GetWorldObject(bot);
                if (pendingObject && bot->GetDistance(pendingObject) > sPlayerbotAIConfig.contactDistance + 0.5f)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                        mutableState.session.returnRequested ? "approaching loot before return" : "approaching nearby loot");
                    if (!bot->isMoving() && !bot->IsNonMeleeSpellCast(true))
                        MoveNear(pendingObject, sPlayerbotAIConfig.contactDistance,
                            MovementPriority::MOVEMENT_NORMAL);
                    return false;
                }
                SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                    mutableState.session.returnRequested ? "finishing nearby loot before return" : "yielding to nearby loot");
                return false;
            }

            if (mutableState.session.returnRequested)
                return ReturnHome(mutableState, now);

            // A loaded selected node wins over the recorded route point. The
            // stock open-loot action performs the profession/lock check and
            // casts the gathering spell.
            SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "scanning live nodes");
            // Refresh the live-node cache at most twice per second. GUIDs are
            // retained, not raw pointers, so unloaded/despawned objects are safe.
            float const liveScanRadius = std::max(30.0f, sPlayerbotAIConfig.sightDistance);
            if (mutableState.liveCache.Due(now))
            {
                mutableState.liveCache.Replace(now,
                    Sbrpg::NodeRepository::ScanLive(bot, mutableState, liveScanRadius));
                Sbrpg::NodeRepository::UpdateLiveAssociations(bot, mutableState, liveScanRadius, now);
                if (runtimeSettings.debug)
                    Debug(bot, Acore::StringFormat("live node scan: {} observations, {}ms age",
                        mutableState.liveCache.Observations().size(), now - mutableState.liveCache.LastRefreshMs()));
            }
            auto setNodeObservation = [&mutableState](ObjectGuid guid, Sbrpg::NodeObservationState observation)
            {
                auto routePoint = std::find_if(mutableState.route.begin(), mutableState.route.end(),
                    [guid](Sbrpg::RoutePoint const& point) { return point.liveGuid == guid; });
                if (routePoint != mutableState.route.end())
                    routePoint->observation = observation;
            };
            if (!mutableState.activeGatherNode.IsEmpty())
            {
                setNodeObservation(mutableState.activeGatherNode, Sbrpg::NodeObservationState::Gathering);
                GameObject* activeNode = botAI->GetGameObject(mutableState.activeGatherNode);
                if (!activeNode || !activeNode->IsInWorld() || !activeNode->isSpawned())
                {
                    ++mutableState.harvested;
                    mutableState.currentSpawn = 0;
                    mutableState.pendingGatherNode = ObjectGuid::Empty;
                    mutableState.pendingGatherSinceMs = 0;
                    mutableState.activeGatherNode = ObjectGuid::Empty;
                    mutableState.gatherStartedMs = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "stock gather completed; node despawned");
                    return true;
                }
                // Do not requeue or reopen a still-spawned GO. That was
                // closing its loot window early and fighting playerbots. After
                // the first stock handoff, suppress only SBRPG's own selection;
                // the stock `gather` strategy remains free to detect/reopen a
                // multi-yield node through its normal lifecycle.
                if (now - mutableState.gatherStartedMs < 10000)
                {
                    if (now - mutableState.gatherAttemptedMs >= 5000 && !bot->IsNonMeleeSpellCast(true) &&
                        bot->GetLootGUID() != mutableState.activeGatherNode)
                    {
                        // Route points and live nodes both use the DB spawn ID.
                        // Never key this by ObjectGuid::GetCounter(): its value
                        // is not a stable route-spawn identifier on every core.
                        mutableState.blacklistedUntilMs[activeNode->GetSpawnId()] = now + 30000;
                        mutableState.activeGatherNode = ObjectGuid::Empty;
                        SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "yielding remaining loot to stock gather");
                    }
                    return false;
                }
                ObjectGuid const timedOutNode = mutableState.activeGatherNode;
                uint32 const timedOutSpawn = activeNode ? activeNode->GetSpawnId() : timedOutNode.GetCounter();
                mutableState.blacklistedUntilMs[timedOutSpawn] =
                    now + 1000 * mutableState.emptyBlacklistSeconds;
                AI_VALUE(LootObjectStack*, "available loot")->Remove(timedOutNode);
                context->GetValue<LootObject>("loot target")->Set(LootObject());
                mutableState.activeGatherNode = ObjectGuid::Empty;
                mutableState.gatherStartedMs = 0;
                mutableState.pendingGatherNode = ObjectGuid::Empty;
                mutableState.pendingGatherSinceMs = 0;
                mutableState.currentSpawn = 0;
                SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "gather timed out; replanning");
                return true;
            }
            // A selected database point is only considered empty once it is
            // inside the observed area and the live scan found no matching
            // spawned node. Unknown points remain valid discovery targets.
            if (mutableState.currentSpawn != 0)
            {
                auto current = std::find_if(mutableState.route.begin(), mutableState.route.end(),
                    [&mutableState](Sbrpg::RoutePoint const& point) { return point.spawn == mutableState.currentSpawn; });
                if (current != mutableState.route.end() &&
                    current->observation == Sbrpg::NodeObservationState::Unavailable)
                {
                    uint32 const blacklistSeconds = mutableState.emptyBlacklistSeconds;
                    mutableState.blacklistedUntilMs[current->spawn] = now + 1000 * blacklistSeconds;
                    current->observation = Sbrpg::NodeObservationState::TemporarilySkipped;
                    mutableState.currentSpawn = 0;
                    mutableState.step.valid = false;
                    mutableState.stepIssued = false;
                    mutableState.targetSinceMs = 0;
                    mutableState.stuckChecks = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "selected node absent from observed area");
                    Debug(bot, Acore::StringFormat("spawn {} absent from live scan; skipped for {} seconds",
                        current->spawn, blacklistSeconds));
                    return true;
                }
            }

            for (ObjectGuid const& guid : mutableState.liveCache.Guids())
            {
                GameObject* go = botAI->GetGameObject(guid);
                if (!go || !go->IsInWorld() || !go->isSpawned() || !HasEntry(*state, go->GetEntry()))
                    continue;
                LootObject loot(bot, go->GetGUID());
                if (!MatchesProfession(*state, loot) || !loot.IsLootPossible(bot))
                    continue;
                auto blocked = mutableState.blacklistedUntilMs.find(go->GetSpawnId());
                if (blocked != mutableState.blacklistedUntilMs.end() && now < blocked->second)
                    continue;
                bool const reroutingToLiveNode = mutableState.currentSpawn != 0;
                if (reroutingToLiveNode)
                {
                    mutableState.currentSpawn = 0;
                    mutableState.step.valid = false;
                    mutableState.stepIssued = false;
                    mutableState.targetSinceMs = 0;
                    mutableState.stuckChecks = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::ApproachingNode,
                        Acore::StringFormat("rerouting to live node (entry {}, spawn {})", go->GetEntry(), go->GetSpawnId()));
                    Debug(bot, Acore::StringFormat("rerouting from database route to live node: entry {}, spawn {}, distance {:.1f}",
                        go->GetEntry(), go->GetSpawnId(), bot->GetDistance(go)));
                }
                // Do not insert this GO into LootObjectStack ourselves.
                // Give stock gathering a bounded opportunity to claim this
                // exact GUID. A corpse loot target is not evidence that this
                // node is being handled, so it cannot leave SBRPG waiting here
                // forever after the corpse has been emptied.
                LootObject const stockTarget = AI_VALUE(LootObject, "loot target");
                bool const stockOwnsNode = stockTarget.guid == go->GetGUID() || bot->IsNonMeleeSpellCast(true);
                if (mutableState.pendingGatherNode != go->GetGUID())
                {
                    mutableState.pendingGatherNode = go->GetGUID();
                    mutableState.pendingGatherSinceMs = now;
                    mutableState.gatherReadyMs = now + 6000;
                }
                else if (now - mutableState.pendingGatherSinceMs >= 10000 &&
                    !stockOwnsNode && !bot->IsNonMeleeSpellCast(true))
                {
                    mutableState.blacklistedUntilMs[go->GetSpawnId()] = now + 30000;
                    mutableState.pendingGatherNode = ObjectGuid::Empty;
                    mutableState.pendingGatherSinceMs = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "stock gather pending timed out; skipping live node");
                    Debug(bot, Acore::StringFormat("live node gather pending timed out after 10s: spawn {}", go->GetSpawnId()));
                    continue;
                }
                if (stockOwnsNode)
                {
                    setNodeObservation(go->GetGUID(), Sbrpg::NodeObservationState::Gathering);
                    mutableState.gatherReadyMs = now + 6000;
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                        reroutingToLiveNode ? "rerouted to live node; stock gather owns it" : "stock gather owns live node");
                    return false;
                }
                if (now < mutableState.gatherReadyMs)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending,
                        reroutingToLiveNode ? "rerouting to live node; waiting for stock gather claim" : "waiting for stock gather claim");
                    return false;
                }
                if (bot->GetDistance(go) > sPlayerbotAIConfig.contactDistance + 0.5f)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending,
                        reroutingToLiveNode ? "rerouting to live node" : "approaching live gathering node");
                    if (!bot->isMoving() && !bot->IsNonMeleeSpellCast(true))
                    {
                        if (!PrepareTravelMove(bot))
                            return true;
                        MoveNear(go, sPlayerbotAIConfig.contactDistance,
                            MovementPriority::MOVEMENT_NORMAL);
                    }
                    return false;
                }
                if (mutableState.pendingGatherNode != go->GetGUID())
                {
                    mutableState.pendingGatherNode = go->GetGUID();
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending, "settling before stock gather");
                    mutableState.gatherReadyMs = now + mutableState.gatherSettleDelayMs;
                    bot->StopMoving();
                    return true;
                }
                if (now < mutableState.gatherReadyMs)
                    return true;

                // Hand the node to the stock playerbots loot/gather pipeline.
                // SBRPG never owns loot target or open-loot state.
                SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "queued in stock loot stack");
                bool const queued = AI_VALUE(LootObjectStack*, "available loot")->Add(go->GetGUID());
                if (queued)
                {
                    setNodeObservation(go->GetGUID(), Sbrpg::NodeObservationState::Gathering);
                    mutableState.activeGatherNode = go->GetGUID();
                    mutableState.gatherStartedMs = now;
                    mutableState.gatherAttemptedMs = now;
                    mutableState.gatheredItemsAtAttempt = mutableState.gatheredItems;
                    mutableState.gatherRetries = 0;
                }
                else
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "gather node already queued");
                mutableState.currentSpawn = 0;
                mutableState.pendingGatherNode = ObjectGuid::Empty;
                mutableState.pendingGatherSinceMs = 0;
                // Yield immediately so stock loot/gather can select, move to,
                // and open the node without this controller replacing its move.
                return false;
            }

            uint32 spawn = mutableState.currentSpawn;
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (spawn != 0)
            {
                auto current = std::find_if(mutableState.route.begin(), mutableState.route.end(),
                    [spawn](Sbrpg::RoutePoint const& point) { return point.spawn == spawn; });
                if (current == mutableState.route.end())
                    mutableState.currentSpawn = spawn = 0;
                else
                    x = current->x, y = current->y, z = current->z;
            }
            if (spawn == 0 && !Sbrpg::NodeRepository::SelectNextRoute(bot, mutableState, spawn, x, y, z))
            {
                SetPhase(mutableState, Sbrpg::FarmPhase::Waiting, "waiting for eligible route node");
                return true;
            }
            SetPhase(mutableState, Sbrpg::FarmPhase::Travelling, "route objective retained");

            // Reaching a despawned node advances the circular route rather than
            // waiting at a single spawn forever. A later pass sees its respawn.
            float const distance = bot->GetDistance(x, y, z);
            if (distance < 5.0f)
            {
                // No matching loaded object was found in the 30yd scan above.
                // We are standing at its DB spawn, therefore it is despawned
                // (or unavailable to this character). Do not oscillate between
                // empty route points while waiting for its respawn.
                uint32 const blacklistSeconds = mutableState.emptyBlacklistSeconds;
                mutableState.blacklistedUntilMs[spawn] = now + 1000 * blacklistSeconds;
                mutableState.currentSpawn = 0;
                mutableState.targetSinceMs = 0;
                mutableState.stuckChecks = 0;
                SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "route node empty; replanning");
                Debug(bot, Acore::StringFormat("spawn {} is empty; skipping it for {} seconds", spawn, blacklistSeconds));
                return true;
            }

            // MovementAction supplies normal playerbot pathing and duplicate-move
            // suppression. We measure progress in 10s windows: three windows
            // without closing the gap means this node is unreachable, so park it
            // for one minute and continue the route instead of vibrating in place.
            if (mutableState.currentSpawn != spawn)
            {
                mutableState.currentSpawn = spawn;
                mutableState.step.valid = false;
                mutableState.stepIssued = false;
                mutableState.targetSinceMs = now;
                mutableState.lastTargetDistance = distance;
                mutableState.stuckChecks = 0;
            }
            else if (now - mutableState.targetSinceMs >= 10000)
            {
                if (distance >= mutableState.lastTargetDistance - 3.0f)
                    ++mutableState.stuckChecks;
                else
                    mutableState.stuckChecks = 0;
                mutableState.targetSinceMs = now;
                mutableState.lastTargetDistance = distance;
                if (mutableState.stuckChecks >= mutableState.attemptsBeforeBlacklist)
                {
                    uint32 const blacklistSeconds = mutableState.failedBlacklistSeconds;
                    mutableState.blacklistedUntilMs[spawn] = now + 1000 * blacklistSeconds;
                    Debug(bot, Acore::StringFormat("spawn {} made no progress; blacklisted for {} seconds", spawn, blacklistSeconds));
                    mutableState.currentSpawn = 0;
                    mutableState.stuckChecks = 0;
                    return true;
                }
            }

            if (!mutableState.step.valid)
            {
                SetPhase(mutableState, Sbrpg::FarmPhase::BuildingPath, "building mmap corridor");
                Sbrpg::RouteStep step;
                if (!Sbrpg::BuildRouteStep(bot, x, y, z, step))
                {
                    // Only attempt a tiny, real-corridor nudge when the bot is
                    // off the navmesh. Normal no-path legs are blacklisted;
                    // recovery must never become a shortcut around geometry.
                    if ((step.type & PATHFIND_FARFROMPOLY) && Sbrpg::BuildRecoveryStep(bot, step))
                        SetPhase(mutableState, Sbrpg::FarmPhase::Recovering, "off-navmesh; nudging onto corridor");
                    else
                    {
                        mutableState.blacklistedUntilMs[spawn] = now + 1000 * mutableState.failedBlacklistSeconds;
                        mutableState.currentSpawn = 0;
                        SetPhase(mutableState, Sbrpg::FarmPhase::Failed, "route has no safe mmap segment");
                        Debug(bot, Acore::StringFormat("spawn {} has no safe navigation segment", spawn));
                        return true;
                    }
                }
                mutableState.step = std::move(step);
                mutableState.stepIssued = false;
                mutableState.stepBuiltMs = now;
            }

            if (bot->GetExactDist(mutableState.step.x, mutableState.step.y, mutableState.step.z) <= 2.0f)
            {
                mutableState.step.valid = false;
                mutableState.stepIssued = false;
                return true;
            }
            SetPhase(mutableState, Sbrpg::FarmPhase::Travelling, "following validated mmap segment");
            if (!PrepareTravelMove(bot))
                return false;
            bool const issued = MoveTo(bot->GetMapId(), mutableState.step.x, mutableState.step.y, mutableState.step.z,
                                       false, false, false, false, MovementPriority::MOVEMENT_NORMAL, true);
            // A validated PathGenerator result is not proof that playerbots
            // accepted the movement request (cooldowns, duplicate moves, and
            // higher-priority generators can refuse it). Never present that as
            // travelling forever: rebuild once, then blacklist the dead leg.
            if (!issued && !bot->isMoving() && now - mutableState.stepBuiltMs >= 1000)
            {
                mutableState.step.valid = false;
                if (++mutableState.stuckChecks >= mutableState.attemptsBeforeBlacklist)
                {
                    mutableState.blacklistedUntilMs[spawn] = now + 1000 * mutableState.failedBlacklistSeconds;
                    mutableState.currentSpawn = 0;
                    mutableState.stuckChecks = 0;
                    SetPhase(mutableState, Sbrpg::FarmPhase::Failed, "playerbots rejected mmap movement; node skipped");
                }
                else
                    SetPhase(mutableState, Sbrpg::FarmPhase::Recovering, "mmap movement was not issued; rebuilding");
            }
            return issued;
        }

    private:
        bool ReturnHome(Sbrpg::FarmState& state, uint32 now)
        {
            float const distance = bot->GetExactDist(state.session.startX, state.session.startY, state.session.startZ);
            if (Sbrpg::IsAtActivityStart(state.session, bot, 5.0f))
            {
                bot->SetFacingTo(state.session.startO);
                Sbrpg::Finish(bot, state.session.returnReason + "; returned to farm start");
                return true;
            }

            SetPhase(state, Sbrpg::FarmPhase::Returning, state.session.returnReason + "; returning to farm start");
            if (state.targetSinceMs == 0)
            {
                state.targetSinceMs = now;
                state.lastTargetDistance = distance;
            }
            else if (now - state.targetSinceMs >= 10000)
            {
                if (distance >= state.lastTargetDistance - 3.0f)
                    ++state.stuckChecks;
                else
                    state.stuckChecks = 0;
                state.targetSinceMs = now;
                state.lastTargetDistance = distance;
                if (state.stuckChecks >= state.attemptsBeforeBlacklist)
                {
                    state.step.valid = false;
                    state.stepIssued = false;
                    state.stuckChecks = 0;
                    SetPhase(state, Sbrpg::FarmPhase::Recovering, "return-home route stalled; rebuilding");
                }
            }

            if (!state.step.valid)
            {
                Sbrpg::RouteStep step;
                if (!Sbrpg::BuildRouteStep(bot, state.session.startX, state.session.startY, state.session.startZ, step))
                {
                    if (!((step.type & PATHFIND_FARFROMPOLY) && Sbrpg::BuildRecoveryStep(bot, step)))
                    {
                        SetPhase(state, Sbrpg::FarmPhase::Recovering, "no safe mmap route home yet; retrying");
                        return false;
                    }
                }
                state.step = std::move(step);
                state.stepIssued = false;
                state.stepBuiltMs = now;
            }

            if (bot->GetExactDist(state.step.x, state.step.y, state.step.z) <= 2.0f)
            {
                state.step.valid = false;
                state.stepIssued = false;
                return true;
            }
            if (!PrepareTravelMove(bot))
                return false;
            bool const issued = MoveTo(bot->GetMapId(), state.step.x, state.step.y, state.step.z,
                                       false, false, false, false, MovementPriority::MOVEMENT_NORMAL, true);
            if (!issued && !bot->isMoving() && now - state.stepBuiltMs >= 1000)
            {
                state.step.valid = false;
                state.stepIssued = false;
                SetPhase(state, Sbrpg::FarmPhase::Recovering, "return-home movement was not issued; rebuilding");
            }
            return issued;
        }
    };

    class SelfbotRpgFarmStrategy : public Strategy
    {
    public:
        SelfbotRpgFarmStrategy(PlayerbotAI* ai) : Strategy(ai) { }
        std::string const getName() override { return "sbrpg farm"; }
        uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
        std::vector<NextAction> getDefaultActions() override { return { NextAction("sbrpg farm", 4.0f) }; }
    };

    class SelfbotRpgActionContext : public NamedObjectContext<Action>
    {
    public:
        SelfbotRpgActionContext()
        {
            creators["sbrpg farm"] = &CreateFarm;
            creators["sbrpg material attack"] = &CreateMaterialAttack;
        }
    private:
        static Action* CreateFarm(PlayerbotAI* ai) { return new SelfbotRpgFarmAction(ai); }
        static Action* CreateMaterialAttack(PlayerbotAI* ai) { return new SelfbotMaterialAttackAction(ai); }
    };

    class SelfbotRpgMaterialStrategy : public Strategy
    {
    public:
        SelfbotRpgMaterialStrategy(PlayerbotAI* ai) : Strategy(ai) { }
        std::string const getName() override { return "sbrpg material"; }
        uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
        std::vector<NextAction> getDefaultActions() override
        { return { NextAction("sbrpg material attack", 4.5f) }; }
    };

    class SelfbotRpgStrategyContext : public NamedObjectContext<Strategy>
    {
    public:
        SelfbotRpgStrategyContext() : NamedObjectContext<Strategy>(false, false)
        {
            creators["sbrpg farm"] = &CreateFarm;
            creators["sbrpg material"] = &CreateMaterial;
            creators["sbrpg material loot"] = &CreateMaterialLoot;
        }
    private:
        static Strategy* CreateFarm(PlayerbotAI* ai) { return new SelfbotRpgFarmStrategy(ai); }
        static Strategy* CreateMaterial(PlayerbotAI* ai) { return new SelfbotRpgMaterialStrategy(ai); }
        static Strategy* CreateMaterialLoot(PlayerbotAI* ai) { return new SelfbotMaterialLootStrategy(ai); }
    };

    template <class Ctx>
    void RegisterContexts()
    {
        Ctx::sharedActionContexts.Add(new SelfbotRpgActionContext());
        Ctx::sharedStrategyContexts.Add(new SelfbotRpgStrategyContext());
    }

    class SelfbotRpgStatusScript final : public PlayerScript
    {
    public:
        SelfbotRpgStatusScript() : PlayerScript("SelfbotRpgStatusScript", { PLAYERHOOK_ON_UPDATE }) { }

        void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
        {
            if (player)
            {
                auto pending = pendingSelfBotDisable.find(player->GetGUID());
                bool const hasActiveNode = Sbrpg::IsActive(player);
                auto material = materialStates.find(player->GetGUID());
                bool const hasActiveMaterial = material != materialStates.end() && material->second.active;
                if (pending != pendingSelfBotDisable.end() && !hasActiveNode && !hasActiveMaterial)
                {
                    pendingSelfBotDisable.erase(pending);
                    if (IsSelfBot(player))
                        if (PlayerbotMgr* manager = GET_PLAYERBOT_MGR(player))
                            manager->HandlePlayerbotCommand("self", player);
                }
            }
            auto materialIt = player ? materialStates.find(player->GetGUID()) : materialStates.end();
            uint32 const now = getMSTime();
            if (materialIt != materialStates.end() && materialIt->second.active)
            {
                uint32 const current = materialIt->second.fishingByZone ? FishingInventoryCount(player) :
                    player->GetItemCount(materialIt->second.itemId, true);
                if (current > materialIt->second.inventoryCount)
                {
                    uint32 const gained = current - materialIt->second.inventoryCount;
                    materialIt->second.gatheredItems += gained;
                    Debug(player, Acore::StringFormat("fishing inventory gain: {} cataloged fish (total {})",
                        gained, materialIt->second.gatheredItems));
                    if (materialIt->second.quantityGoal != 0 &&
                        materialIt->second.gatheredItems >= materialIt->second.quantityGoal)
                        Sbrpg::RequestActivityReturn(materialIt->second.session, getMSTime(), "fish quantity goal reached");
                }
                materialIt->second.inventoryCount = current;
                if (now - materialIt->second.lastAddonStatusMs >= 2000)
                {
                    materialIt->second.lastAddonStatusMs = now;
                    SendAddon(player, CHAT_MSG_WHISPER, Sbrpg::Protocol::Build("MATERIAL_STATUS", {
                        "0", "1", std::to_string(materialIt->second.itemId),
                        std::to_string(materialIt->second.gatheredItems),
                        std::to_string(materialIt->second.quantityGoal),
                        std::to_string(materialIt->second.kills),
                        std::to_string(Sbrpg::ActivityRemainingSeconds(materialIt->second.session, now)),
                        materialIt->second.harvestSkills.empty() ? "0" : "1",
                        materialIt->second.phase
                    }));
                }
            }
            UpdateMaterialReturn(player);

            auto it = player ? states.find(player->GetGUID()) : states.end();
            if (it == states.end() || !it->second.active)
                return;
            if (it->second.targetItemId != 0)
            {
                uint32 const current = player->GetItemCount(it->second.targetItemId, true);
                if (current > it->second.inventoryCount)
                    it->second.gatheredItems += current - it->second.inventoryCount;
                it->second.inventoryCount = current;
                if (it->second.quantityGoal != 0 &&
                    it->second.gatheredItems >= it->second.quantityGoal)
                    Sbrpg::RequestActivityReturn(it->second.session, getMSTime(), "quantity goal reached");
            }
            if (it->second.revision != it->second.lastPublishedRevision ||
                (it->second.session.durationMs != 0 && now - it->second.lastStatusPublishMs >= 5000))
                PublishStatus(player);
        }
    };

    class SelfbotRpgLootScript : public PlayerScript
    {
    public:
        SelfbotRpgLootScript() : PlayerScript("SelfbotRpgLootScript", {
            PLAYERHOOK_ON_LOOT_ITEM, PLAYERHOOK_ON_CREATURE_KILL,
            PLAYERHOOK_ON_CREATURE_KILLED_BY_PET }) { }
        void OnPlayerLootItem(Player* player, Item* /*item*/, uint32 count, ObjectGuid lootGuid) override
        {
            auto materialIt = player ? materialStates.find(player->GetGUID()) : materialStates.end();
            // AzerothCore passes a temporary loot-item buffer here rather than
            // a fully initialized inventory Item. Do not dereference `item` in
            // this hook; the crash report showed GetUInt32Value(index 3) from
            // that invalid representation. Exact material quantity accounting
            // must use a later inventory-safe observation point.
            if (materialIt != materialStates.end() && materialIt->second.active &&
                lootGuid.IsCreature())
            {
                ++materialIt->second.lootEvents;
                Debug(player, Acore::StringFormat("material loot event: creature loot count {}", count));
            }

            auto it = player ? states.find(player->GetGUID()) : states.end();
            if (it != states.end() && it->second.active && lootGuid.IsCreature())
                it->second.combatLootSinceMs.erase(lootGuid);
            if (it == states.end() || !it->second.active || !lootGuid.IsGameObject() ||
                !HasEntry(it->second, lootGuid.GetEntry()))
                return;

            uint32 const now = getMSTime();
            // One gathering window may emit several item callbacks. Count all
            // quantities, but count the node once. The same DB spawn may be
            // harvested again after respawn, so the GUID dedupe has a short
            // transaction window rather than lasting for the whole run.
            it->second.gatheredItems += count;
            if (it->second.lastGatheredNode != lootGuid || now - it->second.lastGatheredMs >= 10000)
            {
                ++it->second.harvested;
                it->second.lastGatheredNode = lootGuid;
            }
            it->second.lastGatheredMs = now;
            Sbrpg::Transition(it->second, Sbrpg::FarmPhase::Looting, "stock gather item looted");
        }

        void OnPlayerCreatureKill(Player* player, Creature* killed) override
        {
            RecordMaterialKill(player, killed);
        }

        void OnPlayerCreatureKilledByPet(Player* player, Creature* killed) override
        {
            RecordMaterialKill(player, killed);
        }

    private:
        static void RecordMaterialKill(Player* player, Creature* killed)
        {
            if (!player || !killed)
                return;
            auto nodeIt = states.find(player->GetGUID());
            if (nodeIt != states.end() && nodeIt->second.active)
            {
                // Keep corpse discovery local to the active node run, but let
                // the stock loot strategy retain ownership of opening and
                // looting. Add() is idempotent when stock already queued it.
                if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
                    ai->GetAiObjectContext()->GetValue<LootObjectStack*>("available loot")->Get()->Add(killed->GetGUID());
            }
            auto it = materialStates.find(player->GetGUID());
            if (it == materialStates.end() || !it->second.active)
                return;
            if (std::find(it->second.creatureEntries.begin(), it->second.creatureEntries.end(),
                killed->GetEntry()) == it->second.creatureEntries.end())
                return;
            ++it->second.kills;
            it->second.target = killed->GetGUID();
            it->second.corpseWaitSinceMs = getMSTime();
            it->second.returnCombatCorpse = it->second.session.returnRequested;
            it->second.phase = it->second.session.returnRequested ?
                "combat corpse pending before return" : "waiting for stock loot";
            if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
                ai->GetAiObjectContext()->GetValue<LootObjectStack*>("available loot")->Get()->Add(killed->GetGUID());
        }
    };

    class SelfbotRpgRegistrar : public WorldScript
    {
    public:
        SelfbotRpgRegistrar() : WorldScript("SelfbotRpgRegistrar") { }
        void OnUpdate(uint32 /*diff*/) override
        {
            if (_done) return;
            _done = true;
            RegisterContexts<WarriorAiObjectContext>(); RegisterContexts<PaladinAiObjectContext>();
            RegisterContexts<DruidAiObjectContext>(); RegisterContexts<DKAiObjectContext>();
            RegisterContexts<HunterAiObjectContext>(); RegisterContexts<MageAiObjectContext>();
            RegisterContexts<PriestAiObjectContext>(); RegisterContexts<RogueAiObjectContext>();
            RegisterContexts<ShamanAiObjectContext>(); RegisterContexts<WarlockAiObjectContext>();
        }
    private:
        bool _done = false;
    };

    bool ConfigureFarm(Player* player, std::string const& professionText, std::string const& entries,
                       uint32 durationMinutes, std::string* error)
    {
        std::string profession = professionText;
        if (NormalizeResource(profession) == "zone") profession = entries;
        profession = NormalizeResource(profession);
        Sbrpg::Profession type;
        if (profession == "mining" || profession == "mine") type = Sbrpg::Profession::Mining;
        else if (profession == "herbalism" || profession == "herb") type = Sbrpg::Profession::Herbalism;
        else if (profession == "both") type = Sbrpg::Profession::Both;
        else { if (error) *error = "Profession must be mining or herbalism."; return false; }
        std::vector<uint32> nodeEntries;
        if (NormalizeResource(professionText) == "zone" || type == Sbrpg::Profession::Both)
        {
            // Exact resource-name families, never generic locked gameobjects.
            std::string where;
            if (type == Sbrpg::Profession::Mining || type == Sbrpg::Profession::Both)
                where = "name REGEXP 'Copper Vein|Tin Vein|Silver Vein|Iron Deposit|Gold Vein|Mithril Deposit|Truesilver Deposit|Thorium|Fel Iron Deposit|Adamantite|Khorium|Cobalt|Saronite|Titanium'";
            if (type == Sbrpg::Profession::Herbalism || type == Sbrpg::Profession::Both)
            {
                if (!where.empty()) where += " OR ";
                where += "name REGEXP 'Peacebloom|Silverleaf|Earthroot|Mageroyal|Briarthorn|Bruiseweed|Steelbloom|Kingsblood|Liferoot|Fadeleaf|Goldthorn|Khadgar|Firebloom|Lotus|Arthas|Sungrass|Blindweed|Ghost Mushroom|Gromsblood|Dreamfoil|Silversage|Plaguebloom|Icecap|Felweed|Dreaming Glory|Ragveil|Flame Cap|Terocone|Lichen|Netherbloom|Nightmare Vine|Mana Thistle|Goldclover|Tiger Lily|Talandra|Deadnettle|Fire Leaf|Adder|Lichbloom|Icethorn'";
            }
            QueryResult result = WorldDatabase.Query("SELECT entry FROM gameobject_template WHERE {}", where);
            if (result) do { nodeEntries.push_back(result->Fetch()[0].Get<uint32>()); } while (result->NextRow());
        }
        else
            nodeEntries = ResolveResource(type, entries);
        if (nodeEntries.empty())
        {
            if (error)
                *error = "Unknown resource or no matching gameobject template: " + entries;
            return false;
        }
        return Sbrpg::Start(player, type, std::move(nodeEntries), durationMinutes, error);
    }

    std::string AddonStatus(Player* player)
    {
        return Sbrpg::BuildStatusFrame(Sbrpg::Get(player), getMSTime());
    }

    std::string AddonMaterialStatus(Player* player, std::string const& requestId)
    {
        auto const it = player ? materialStates.find(player->GetGUID()) : materialStates.end();
        if (it == materialStates.end())
            return Sbrpg::Protocol::Build("MATERIAL_STATUS", { requestId, "0", "0", "0", "0", "0" });

        Sbrpg::Materials::MaterialFarmState const& state = it->second;
        uint32 const remaining = Sbrpg::ActivityRemainingSeconds(state.session, getMSTime());
        return Sbrpg::Protocol::Build("MATERIAL_STATUS", {
            requestId,
            state.active ? "1" : "0",
            std::to_string(state.itemId),
            std::to_string(state.gatheredItems),
            std::to_string(state.quantityGoal),
            std::to_string(state.kills),
            std::to_string(remaining),
            state.harvestSkills.empty() ? "0" : "1",
            state.phase
        });
    }

    void SendAddon(Player* player, ChatMsg chatType, std::string const& payload);

    void SendMaterialCatalog(Player* player, ChatMsg type, std::string const& requestId)
    {
        auto const& catalog = Sbrpg::Materials::Catalog();
        uint32 const total = static_cast<uint32>(catalog.size());
        for (uint32 i = 0; i < total; ++i)
        {
            auto const& material = catalog[i];
            std::string methods;
            for (auto method : material.methods)
            {
                if (!methods.empty()) methods += ",";
                methods += Sbrpg::Materials::MethodName(method);
            }
            SendAddon(player, type, Sbrpg::Protocol::Build("MATERIAL_CATALOG", {
                requestId, std::to_string(i), std::to_string(total),
                std::to_string(material.itemId), material.key, material.displayName,
                Sbrpg::Materials::FamilyName(material.family), methods
            }));
        }
    }

    void SendMaterialSources(Player* player, ChatMsg type, std::string const& requestId,
                             std::string const& materialName)
    {
        uint32 itemId = 0;
        auto const* material = ResolveMaterial(materialName, itemId);
        if (!material)
        {
            SendAddon(player, type, Sbrpg::Protocol::Build("ERROR", { requestId, "UNKNOWN_MATERIAL" }));
            return;
        }
        auto const& sources = Sbrpg::Materials::LootSourceIndex::Find(itemId);
        std::vector<uint32> fishingPools;
        if (std::find(material->methods.begin(), material->methods.end(),
            Sbrpg::Materials::AcquisitionMethod::Fishing) != material->methods.end())
            fishingPools = ResolveFishingPoolEntriesForItem(itemId);
        std::vector<uint32> gatheringNodes;
        if (std::find(material->methods.begin(), material->methods.end(),
            Sbrpg::Materials::AcquisitionMethod::GameObjectNode) != material->methods.end())
            gatheringNodes = ResolveNodeEntriesForItem(itemId);
        uint32 totalSources = static_cast<uint32>(sources.size() + fishingPools.size() + gatheringNodes.size());
        uint32 index = 0;
        for (uint32 nodeEntry : gatheringNodes)
        {
            SendAddon(player, type, Sbrpg::Protocol::Build("MATERIAL_SOURCE", {
                requestId, std::to_string(index++), std::to_string(totalSources),
                std::to_string(nodeEntry), "gathering node", "node", "normal"
            }));
        }
        for (uint32 poolEntry : fishingPools)
        {
            SendAddon(player, type, Sbrpg::Protocol::Build("MATERIAL_SOURCE", {
                requestId, std::to_string(index++), std::to_string(totalSources),
                std::to_string(poolEntry), "fishing pool", "pool", "normal"
            }));
        }
        for (auto const& source : sources)
        {
            SendAddon(player, type, Sbrpg::Protocol::Build("MATERIAL_SOURCE", {
                requestId, std::to_string(index++), std::to_string(totalSources),
                std::to_string(source.creatureEntry), Sbrpg::Materials::MethodName(source.method),
                Acore::StringFormat("{:.3f}", source.estimatedChance),
                source.questRequired ? "quest" : "normal"
            }));
        }
        // Empty source sets still need a terminal frame so the addon can
        // distinguish "no sources" from a delayed or incomplete response.
        SendAddon(player, type, Sbrpg::Protocol::Build("MATERIAL_SOURCES_END", {
            requestId, std::to_string(totalSources)
        }));
    }

    ChatMsg ReplyChatType(uint32 /*type*/)
    {
        // Replies are always directed to the requesting selfbot. Broadcasting
        // catalog/source chunks to PARTY or RAID leaks one bot's request into
        // every grouped addon and breaks request isolation.
        return CHAT_MSG_WHISPER;
    }

    void SendAddon(Player* player, ChatMsg chatType, std::string const& payload)
    {
        if (!player || !player->GetSession()) return;
        std::string const& wire = payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, chatType, LANG_ADDON, player, nullptr, wire.c_str());
        player->SendDirectMessage(&data);
    }

    void PublishStatus(Player* player)
    {
        if (!player || !player->GetSession())
            return;
        Sbrpg::FarmState& state = states[player->GetGUID()];
        SendAddon(player, CHAT_MSG_WHISPER, AddonStatus(player));
        state.lastPublishedRevision = state.revision;
        state.lastStatusPublishMs = getMSTime();
    }

    class SelfbotRpgAddonHook final : public PlayerScript
    {
    public:
        SelfbotRpgAddonHook() : PlayerScript("SelfbotRpgAddonHook", {
            PLAYERHOOK_ON_BEFORE_SEND_CHAT_MESSAGE,
            PLAYERHOOK_CAN_PLAYER_USE_PRIVATE_CHAT,
            PLAYERHOOK_CAN_PLAYER_USE_GROUP_CHAT
        }) { }

        bool OwnsFrame(uint32 lang, std::string const& msg) const
        {
            return lang == LANG_ADDON && msg.rfind(std::string(Sbrpg::Protocol::Prefix) + "\t", 0) == 0;
        }

        bool TryHandle(Player* player, uint32 type, uint32 lang, std::string& msg)
        {
            if (!player || !OwnsFrame(lang, msg))
                return false;
            // Anything claiming our prefix is consumed, even if malformed;
            // never let it fall through into normal playerbot chat handling.
            Sbrpg::Protocol::Frame frame;
            if (!Sbrpg::Protocol::Parse(msg, frame))
            {
                SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ERROR", { "0", "MALFORMED_FRAME" }));
                return true;
            }

            std::string const& opcode = frame.opcode;
            if (opcode == "HELLO")
            {
                SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("HELLO_ACK", { frame.requestId, "1" }));
                SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("CAPABILITIES", {
                    frame.requestId, "1", "START,STATUS,SET,SET_CONFIG,STOP,MATERIAL_CATALOG,MATERIAL_SOURCES,START_MATERIAL,START_FISHING,MATERIAL_STATUS"
                }));
                return true;
            }
            if (opcode == "SET_CONFIG" && frame.fields.size() >= 2)
            {
                std::string error;
                if (SetRuntimeConfig(frame.fields[0], frame.fields[1], &error))
                {
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ACK", { frame.requestId, "SET_CONFIG", frame.fields[0] }));
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("SETTING", { frame.fields[0], frame.fields[1] }));
                }
                else
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ERROR", { frame.requestId, "INVALID_CONFIG", error }));
                return true;
            }
            if (opcode == "START_FISHING" && frame.fields.size() >= 3)
            {
                uint32 duration = 0, quantity = 0;
                try
                {
                    duration = static_cast<uint32>(std::stoul(frame.fields[2]));
                    if (frame.fields.size() >= 4) quantity = static_cast<uint32>(std::stoul(frame.fields[3]));
                }
                catch (...) { SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ERROR", { frame.requestId, "INVALID_FISHING_GOAL" })); return true; }
                bool const byZone = frame.fields[0] == "zone";
                bool const prioritize = frame.fields.size() >= 5 ? frame.fields[4] == "1" : runtimeSettings.fishingPrioritizePools;
                bool const openWaterOnly = frame.fields.size() >= 6 ? frame.fields[5] == "1" : runtimeSettings.fishingOpenWaterOnly;
                uint32 itemId = 0;
                std::string error;
                if (!byZone)
                {
                    auto const* material = ResolveMaterial(frame.fields[1], itemId);
                    if (!material || std::find(material->methods.begin(), material->methods.end(), Sbrpg::Materials::AcquisitionMethod::Fishing) == material->methods.end())
                        error = "Select a fishing material or use zone fishing.";
                }
                if (error.empty() && !StartFishingMaterial(player, itemId, duration, quantity, &error, byZone, prioritize, openWaterOnly))
                    ;
                if (!error.empty()) SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ERROR", { frame.requestId, error }));
                else
                {
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ACK", { frame.requestId, opcode }));
                    SendAddon(player, ReplyChatType(type), AddonMaterialStatus(player, frame.requestId));
                }
                return true;
            }
            if (opcode == "MATERIAL_CATALOG")
            {
                SendMaterialCatalog(player, ReplyChatType(type), frame.requestId);
                return true;
            }
            if (opcode == "MATERIAL_SOURCES" && frame.fields.size() >= 2)
            {
                SendMaterialSources(player, ReplyChatType(type), frame.requestId, frame.fields[1]);
                return true;
            }
            if (opcode == "START_MATERIAL" && frame.fields.size() >= 3)
            {
                uint32 duration = 0, quantity = 0;
                try
                {
                    duration = static_cast<uint32>(std::stoul(frame.fields[2]));
                    if (frame.fields.size() >= 4) quantity = static_cast<uint32>(std::stoul(frame.fields[3]));
                }
                catch (std::exception const&)
                {
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ERROR", { frame.requestId, "INVALID_MATERIAL_GOAL" }));
                    return true;
                }
                std::string error;
                if (!StartMaterial(player, frame.fields[1], duration, quantity, &error))
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ERROR", { frame.requestId, error }));
                else
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ACK", { frame.requestId, opcode }));
                SendAddon(player, ReplyChatType(type), AddonMaterialStatus(player, frame.requestId));
                return true;
            }
            if (opcode == "START" && frame.fields.size() >= 2)
            {
                uint32 durationMinutes = 0;
                if (frame.fields.size() >= 3 && !frame.fields[2].empty())
                {
                    char* end = nullptr;
                    unsigned long const parsed = std::strtoul(frame.fields[2].c_str(), &end, 10);
                    if (!end || *end != '\0' || parsed > 10080)
                    {
                        SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ERROR", { frame.requestId, "DURATION_MUST_BE_0_TO_10080_MINUTES" }));
                        return true;
                    }
                    durationMinutes = static_cast<uint32>(parsed);
                }
                std::string error;
                if (!ConfigureFarm(player, frame.fields[0], frame.fields[1], durationMinutes, &error))
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ERROR", { frame.requestId, error }));
                else
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ACK", { frame.requestId, opcode }));
            }
            else if (opcode == "STOP")
            {
                Sbrpg::Stop(player);
                RequestMaterialStop(player);
                SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ACK", { frame.requestId, opcode }));
            }
            else if (opcode == "MATERIAL_STATUS")
            {
                SendAddon(player, ReplyChatType(type), AddonMaterialStatus(player, frame.requestId));
                return true;
            }
            else if (opcode == "SET" && frame.fields.size() >= 2)
            {
                uint32 value = static_cast<uint32>(std::strtoul(frame.fields[1].c_str(), nullptr, 10));
                std::string error;
                if (!Sbrpg::SetOption(player, frame.fields[0], value, &error))
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ERROR", { frame.requestId, error }));
                else
                {
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ACK", { frame.requestId, opcode }));
                    SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("SETTING", { frame.fields[0], frame.fields[1] }));
                }
            }
            else if (opcode != "STATUS")
                SendAddon(player, ReplyChatType(type), Sbrpg::Protocol::Build("ERROR", { frame.requestId, "UNKNOWN_OPCODE" }));

            SendAddon(player, ReplyChatType(type), AddonStatus(player));
            return true;
        }

        void OnPlayerBeforeSendChatMessage(Player* player, uint32& type, uint32& lang, std::string& msg) override
        {
            if (!OwnsFrame(lang, msg))
                return;

            // Playerbots' legacy chat script currently drops the LANG_ADDON
            // argument before calling HandleCommand. Execute our frame now,
            // then remove its command-like payload before that script runs.
            // Keep the owned prefix so the can-use-chat hook still blocks relay.
            TryHandle(player, type, lang, msg);
            msg = std::string(Sbrpg::Protocol::Prefix) + "\t";
        }

        bool OnPlayerCanUseChat(Player* /*player*/, uint32 /*type*/, uint32 lang, std::string& msg, Player* /*receiver*/) override
        {
            return !OwnsFrame(lang, msg);
        }

        bool OnPlayerCanUseChat(Player* /*player*/, uint32 /*type*/, uint32 lang, std::string& msg, Group* /*group*/) override
        {
            return !OwnsFrame(lang, msg);
        }
    };

    class SelfbotRpgCommand : public CommandScript
    {
    public:
        SelfbotRpgCommand() : CommandScript("SelfbotRpgCommand") { }
        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable table = {
                { "farm", HandleFarm, SEC_PLAYER, Console::No },
                { "material", HandleMaterial, SEC_PLAYER, Console::No },
                { "mstatus", HandleMaterialStatus, SEC_PLAYER, Console::No },
                { "stop", HandleStop, SEC_PLAYER, Console::No },
                { "status", HandleStatus, SEC_PLAYER, Console::No },
                { "set", HandleSet, SEC_PLAYER, Console::No },
            };
            static ChatCommandTable root = { { "sbrpg", table } };
            return root;
        }
        static bool HandleFarm(ChatHandler* handler, Tail args)
        {
            Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
            std::istringstream input{std::string(args)}; std::string profession, entries;
            input >> profession; std::getline(input, entries);
            std::string error;
            if (!ConfigureFarm(player, profession, entries, 0, &error))
            {
                handler->PSendSysMessage("SelfBot RPG farm start failed: {}", error);
                handler->SendSysMessage("Usage: .sbrpg farm <mining|herbalism> <resource>; `.sbrpg farm zone mining`; or `.sbrpg farm both zone`.");
            }
            else
            {
                handler->SendSysMessage(Sbrpg::Status(player));
                if (Sbrpg::FarmState const* state = Sbrpg::Get(player))
                    handler->PSendSysMessage("SBRPG settings: attempts {}, failed {}s, empty {}s, zone {}, settle {}ms.",
                        state->attemptsBeforeBlacklist, state->failedBlacklistSeconds,
                        state->emptyBlacklistSeconds, state->stayInCurrentZone ? 1 : 0,
                        state->gatherSettleDelayMs);
            }
            return true;
        }
        static bool HandleMaterialStatus(ChatHandler* handler)
        {
            Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
            auto it = player ? materialStates.find(player->GetGUID()) : materialStates.end();
            if (it == materialStates.end())
                handler->SendSysMessage("No material run is active.");
            else
                handler->PSendSysMessage("Material run: item {}, {} / {} items, {} kills, {} loot events, {} corpse timeouts, {} remaining, phase '{}', return {}, home map {} ({:.1f}, {:.1f}, {:.1f}), distance {:.1f}, reason '{}'.",
                    it->second.itemId, it->second.gatheredItems, it->second.quantityGoal,
                    it->second.kills, it->second.lootEvents, it->second.corpseTimeouts,
                    FormatDuration(Sbrpg::ActivityRemainingSeconds(it->second.session, getMSTime())),
                    it->second.phase,
                    it->second.session.returnRequested ? "yes" : "no",
                    it->second.session.startMapId, it->second.session.startX,
                    it->second.session.startY, it->second.session.startZ,
                    player->GetMapId() == it->second.session.startMapId ?
                        player->GetExactDist(it->second.session.startX, it->second.session.startY,
                            it->second.session.startZ) : -1.0f,
                    it->second.session.returnReason);
            return true;
        }
        static bool HandleMaterial(ChatHandler* handler, Tail args)
        {
            std::istringstream input{std::string(args)};
            std::string operation;
            input >> operation;
            if (operation == "status")
                return HandleMaterialStatus(handler);
            if (operation != "sources" && operation != "hotspots" && operation != "start")
            {
                handler->SendSysMessage("Usage: .sbrpg material sources|hotspots|start <material> [duration minutes] [quantity]");
                return true;
            }

            std::string target;
            std::getline(input, target);
            if (operation == "start")
            {
                std::istringstream startInput{target};
                std::vector<std::string> parts;
                std::string part;
                while (startInput >> part)
                    parts.push_back(part);
                uint32 durationMinutes = 0;
                uint32 quantityGoal = 0;
                auto parseTrailing = [&parts](uint32& value)
                {
                    if (parts.empty() || !std::all_of(parts.back().begin(), parts.back().end(),
                        [](unsigned char c) { return std::isdigit(c); }))
                        return false;
                    try { value = static_cast<uint32>(std::stoul(parts.back())); }
                    catch (...) { return false; }
                    parts.pop_back();
                    return true;
                };
                // Syntax: start <material> [duration-minutes] [quantity].
                parseTrailing(quantityGoal);
                parseTrailing(durationMinutes);
                std::ostringstream materialInput;
                bool firstPart = true;
                for (std::string const& namePart : parts)
                {
                    if (!firstPart)
                        materialInput << ' ';
                    materialInput << namePart;
                    firstPart = false;
                }
                std::string error;
                if (!StartMaterial(handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr,
                    materialInput.str(), durationMinutes, quantityGoal, &error))
                    handler->PSendSysMessage("Material farming start failed: {}", error);
                else
                    handler->PSendSysMessage("Material farming started ({} min, {} items).",
                        durationMinutes, quantityGoal);
                return true;
            }
            Sbrpg::Materials::MaterialDefinition const* material = Sbrpg::Materials::Find(target);
            uint32 itemId = material ? material->itemId : 0;
            if (!material)
            {
                std::string normalized = Sbrpg::Materials::Normalize(target);
                if (!normalized.empty() && std::all_of(normalized.begin(), normalized.end(),
                    [](unsigned char character) { return std::isdigit(character); }))
                {
                    try
                    {
                        unsigned long const parsed = std::stoul(normalized);
                        if (parsed <= std::numeric_limits<uint32>::max())
                            itemId = static_cast<uint32>(parsed);
                    }
                    catch (std::exception const&)
                    {
                        itemId = 0;
                    }
                }
                material = Sbrpg::Materials::Find(itemId);
            }
            if (!itemId)
            {
                handler->SendSysMessage("Unknown material. Use a catalog name such as linen, wool, or runecloth.");
                return true;
            }

            std::vector<Sbrpg::Materials::LootSource> const& sources =
                Sbrpg::Materials::LootSourceIndex::Find(itemId);
            if (operation == "hotspots")
            {
                Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
                std::vector<uint32> creatureEntries;
                for (Sbrpg::Materials::LootSource const& source : sources)
                    if (!source.questRequired && source.method == Sbrpg::Materials::AcquisitionMethod::CreatureLoot &&
                        std::find(creatureEntries.begin(), creatureEntries.end(), source.creatureEntry) == creatureEntries.end())
                        creatureEntries.push_back(source.creatureEntry);
                std::vector<Sbrpg::Materials::CreatureSpawn> spawns =
                    Sbrpg::Materials::CreatureSpawnRepository::Load(player, creatureEntries, true);
                uint32 const spawnCount = spawns.size();
                std::vector<Sbrpg::Materials::Hotspot> hotspots =
                    Sbrpg::Materials::HotspotPlanner::Plan(player,
                        Sbrpg::Materials::HotspotPlanner::Build(std::move(spawns)));
                handler->PSendSysMessage("{}: {} source entries, {} spawns, {} hotspots in zone {}",
                    material ? material->displayName : "item", creatureEntries.size(),
                    spawnCount, hotspots.size(), player ? player->GetZoneId() : 0);
                uint32 shownHotspots = 0;
                for (Sbrpg::Materials::Hotspot const& hotspot : hotspots)
                {
                    handler->PSendSysMessage("  hotspot {}: {:.1f}, {:.1f}, {:.1f}; {} spawns; {}s respawn; {}",
                        hotspot.id, hotspot.x, hotspot.y, hotspot.z, hotspot.spawnCount,
                        hotspot.averageRespawnSeconds, hotspot.reachable ? "reachable" : "unreachable");
                    if (++shownHotspots >= 30)
                    {
                        if (hotspots.size() > shownHotspots)
                            handler->SendSysMessage("  output limited to 30 hotspots.");
                        break;
                    }
                }
                return true;
            }
            handler->PSendSysMessage("{}: {} creature sources", material ? material->displayName : "item", sources.size());
            uint32 shown = 0;
            for (Sbrpg::Materials::LootSource const& source : sources)
            {
                if (source.questRequired)
                    continue;
                CreatureTemplate const* creature = sObjectMgr->GetCreatureTemplate(source.creatureEntry);
                handler->PSendSysMessage("  {} {} [{} ~{:.2f}% count {}-{}{}]",
                    source.creatureEntry, creature ? creature->Name : "unknown",
                    Sbrpg::Materials::MethodName(source.method), source.estimatedChance,
                    source.minCount, source.maxCount,
                    source.fromReference ? ", reference" : "");
                if (++shown >= 50)
                {
                    if (sources.size() > shown)
                        handler->SendSysMessage("  output limited to 50 sources.");
                    break;
                }
            }
            return true;
        }
        static bool HandleStop(ChatHandler* handler)
        {
            Player* p = handler->GetSession()->GetPlayer();
            Sbrpg::Stop(p);
            RequestMaterialStop(p);
            handler->SendSysMessage("SelfBot RPG stop requested; returning to the session start.");
            return true;
        }
        static bool HandleStatus(ChatHandler* handler)
        {
            Player* player = handler->GetSession()->GetPlayer();
            handler->SendSysMessage(Sbrpg::Status(player));
            HandleMaterialStatus(handler);
            return true;
        }
        static bool HandleSet(ChatHandler* handler, Tail args)
        {
            std::istringstream input{std::string(args)}; std::string key; uint32 value = 0; input >> key >> value;
            std::string error;
            if (!Sbrpg::SetOption(handler->GetSession()->GetPlayer(), key, value, &error))
                handler->PSendSysMessage("SBRPG setting rejected: {}", error);
            return true;
        }
    };
}

namespace Sbrpg
{
    bool Start(Player* player, Profession profession, std::vector<uint32> entries,
               uint32 durationMinutes, std::string* error, uint32 targetItemId,
               uint32 quantityGoal)
    {
        // Every node-farming entry point shares this guard. Do not replace an
        // active material session: its return target, equipment snapshot, and
        // loot ownership must remain intact until it finishes returning.
        auto const materialIt = materialStates.find(player ? player->GetGUID() : ObjectGuid::Empty);
        if (materialIt != materialStates.end() && materialIt->second.active)
        {
            if (error)
                *error = "Stop the active material session before starting node farming.";
            return false;
        }
        if (!RuntimeEnabled())
        { if (error) *error = "SBRPG is disabled by SelfBotRpg.Enable."; return false; }
        if (!player)
        { if (error) *error = "Player is unavailable."; return false; }
        auto existingMaterial = materialStates.find(player->GetGUID());
        if (existingMaterial != materialStates.end() && existingMaterial->second.active)
        { if (error) *error = "Stop the active material session first."; return false; }
        if (entries.empty()) { if (error) *error = "Select at least one node type."; return false; }
        bool enabledBySbrpg = false;
        if (!EnsureSelfBot(player, enabledBySbrpg))
        { if (error) *error = "Unable to enable self-bot mode."; return false; }
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        bool const needsMining = profession == Profession::Mining || profession == Profession::Both;
        bool const needsHerbalism = profession == Profession::Herbalism || profession == Profession::Both;
        bool const hasMining = ai->HasSkill(SKILL_MINING) && HasHarvestTool(player, SKILL_MINING);
        bool const hasHerbalism = ai->HasSkill(SKILL_HERBALISM);
        if (needsMining && !hasMining && profession == Profession::Mining)
        { DisableOwnedSelfBot(player, enabledBySbrpg); if (error) *error = "Mining requires the Mining skill and a mining pick."; return false; }
        if (needsHerbalism && !hasHerbalism && profession == Profession::Herbalism)
        { DisableOwnedSelfBot(player, enabledBySbrpg); if (error) *error = "Herbalism requires the Herbalism skill."; return false; }
        if (profession == Profession::Both && !hasMining && !hasHerbalism)
        { DisableOwnedSelfBot(player, enabledBySbrpg); if (error) *error = "Mining and Herbalism require at least one learned profession with its required tool."; return false; }
        FarmState& state = states[player->GetGUID()];
        state = FarmState();
        state.active = true;
        state.selfBotEnabledBySbrpg = enabledBySbrpg;
        state.runId = ++nextRunId;
        state.revision = 0;
        SetPhase(state, FarmPhase::Planning, "loading route nodes");
        state.profession = profession;
        state.entries = std::move(entries); state.currentSpawn = 0; state.harvested = 0;
        state.targetItemId = targetItemId;
        state.quantityGoal = quantityGoal;
        state.inventoryStart = targetItemId ? player->GetItemCount(targetItemId, true) : 0;
        state.inventoryCount = state.inventoryStart;
        Sbrpg::BeginActivitySession(state.session, player, durationMinutes, getMSTime());
        state.lastGatheredNode = ObjectGuid::Empty; state.activeGatherNode = ObjectGuid::Empty; state.gatheredItems = 0;
        state.zoneId = player->GetZoneId(); state.mapId = player->GetMapId();
        state.startedInside = !player->IsOutdoors();
        state.approachNode = ObjectGuid::Empty;
        state.pendingGatherNode = ObjectGuid::Empty;
        state.attemptsBeforeBlacklist = runtimeSettings.attemptsBeforeBlacklist;
        state.failedBlacklistSeconds = runtimeSettings.failedNodeBlacklistSeconds;
        state.emptyBlacklistSeconds = runtimeSettings.emptyNodeBlacklistSeconds;
        state.gatherSettleDelayMs = runtimeSettings.gatherSettleDelayMs;
        state.stayInCurrentZone = runtimeSettings.stayInCurrentZone;
        state.route = Sbrpg::NodeRepository::LoadRoute(player, state,
            [profession](uint32 entry) { return IsGatheringEntry(profession, entry); });
        Debug(player, Acore::StringFormat("loaded {} route nodes on map {} in zone {}", state.route.size(), player->GetMapId(), state.zoneId));
        state.routeIndex = 0;
        state.route = Sbrpg::BuildRoutePlan(std::move(state.route), player->GetPositionX(),
                                             player->GetPositionY(), player->GetPositionZ());
        ai = GET_PLAYERBOT_AI(player);
        state.lootStrategy.SetStrategy("loot");
        state.lootStrategy.Acquire(ai);
        state.gatherStrategy.SetStrategy("gather");
        state.gatherStrategy.Acquire(ai);
        state.mountStrategy.SetStrategy("mount");
        state.mountStrategy.Acquire(ai);
        ai->ChangeStrategy("+sbrpg farm", BOT_STATE_NON_COMBAT);
        return true;
    }
    void Finish(Player* player, std::string reason)
    {
        if (!player) return;
        auto it = states.find(player->GetGUID());
        if (it == states.end()) return;
        FarmState& state = it->second;
        bool const disableSelfBot = state.selfBotEnabledBySbrpg;
        Transition(state, FarmPhase::Stopped, std::move(reason));
        state.active = false;
        player->StopMoving();
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        {
            ai->ChangeStrategy("-sbrpg farm", BOT_STATE_NON_COMBAT);
            state.mountStrategy.Release(ai);
            state.lootStrategy.Release(ai);
            state.gatherStrategy.Release(ai);
        }
        PublishStatus(player);
        DisableOwnedSelfBot(player, disableSelfBot);
    }

    void Stop(Player* player)
    {
        if (!player) return;
        auto it = states.find(player->GetGUID());
        if (it == states.end())
            return;

        FarmState& state = it->second;
        if (!state.active)
            return;

        // Stop cancels new farming objectives but keeps the module state alive
        // until the bounded return route reaches the recorded session start.
        RequestActivityReturn(state.session, getMSTime(), "stopped by user");
        state.currentSpawn = 0;
        state.pendingGatherNode = ObjectGuid::Empty;
        state.pendingGatherSinceMs = 0;
        state.activeGatherNode = ObjectGuid::Empty;
        state.step = Sbrpg::RouteStep();
        state.stepIssued = false;
        state.targetSinceMs = 0;
        state.stuckChecks = 0;
        player->StopMoving();
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        {
            state.gatherStrategy.Suspend(ai);
            ai->ChangeStrategy("+sbrpg farm", BOT_STATE_NON_COMBAT);
        }
        SetPhase(state, FarmPhase::Returning, "stopped by user; returning to farm start");
        PublishStatus(player);
    }
    FarmState const* Get(Player* player)
    { auto it = player ? states.find(player->GetGUID()) : states.end(); return it == states.end() ? nullptr : &it->second; }
    bool IsActive(Player* player) { FarmState const* state = Get(player); return state && state->active; }
    bool SetOption(Player* player, std::string const& key, uint32 value, std::string* error)
    {
        FarmState const* found = Get(player);
        if (!found || !found->active) { if (error) *error = "start farming first"; return false; }
        FarmState& state = states[player->GetGUID()];
        if (key == "attempts" && value >= 1 && value <= 10) state.attemptsBeforeBlacklist = value;
        else if (key == "failedblacklist" && value <= 3600) state.failedBlacklistSeconds = value;
        else if (key == "emptyblacklist" && value <= 3600) state.emptyBlacklistSeconds = value;
        else if (key == "zone") state.stayInCurrentZone = value != 0;
        else if (key == "settledelay" && value <= 10000) state.gatherSettleDelayMs = value;
        else { if (error) *error = "use attempts, failedblacklist, emptyblacklist, zone, or settledelay"; return false; }
        return true;
    }

    std::string Status(Player* player)
    {
        FarmState const* state = Get(player);
        if (!state) return "SelfBot RPG: idle.";
        if (!state->active) return "SelfBot RPG: " + (state->lastReason.empty() ? std::string("idle.") : state->lastReason);
        uint32 const elapsedMs = std::max(1u, getMSTime() - state->session.startedMs);
        double const perMinute = state->gatheredItems * 60000.0 / elapsedMs;
        std::string const reason = state->lastReason.empty() ? std::string() : " — " + state->lastReason;
        std::string timer;
        if (state->session.durationMs != 0)
        {
            uint32 const remaining = Sbrpg::ActivityRemainingSeconds(state->session, getMSTime());
            timer = ", " + FormatDuration(remaining) + " remaining";
        }
        return "SelfBot RPG: " + std::string(PhaseName(state->phase)) + reason + " | farming " + std::string(ProfessionName(state->profession)) + " (" +
               std::to_string(state->route.size()) + " eligible route nodes, " +
               std::to_string(state->harvested) + " gathers / " + std::to_string(state->gatheredItems) + " items, " +
               Acore::StringFormat("{:.2f}/min {:.3f}/sec", perMinute, perMinute / 60.0) + timer + ").";
    }
}

void AddSelfbotRpgScripts()
{
    LoadRuntimeSettings();
    new SelfbotRpgRegistrar();
    new SelfbotRpgStatusScript();
    new SelfbotRpgLootScript();
    new SelfbotRpgAddonHook();
    new SelfbotRpgCommand();
}
