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
#include "NamedObjectContext.h"
#include "NearestGameObjects.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "PathGenerator.h"
#include "ScriptMgr.h"
#include "ServerFacade.h"
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
#include <utility>

using namespace Acore::ChatCommands;

namespace
{
    std::unordered_map<ObjectGuid, Sbrpg::FarmState> states;
    std::unordered_map<ObjectGuid, Sbrpg::Materials::MaterialFarmState> materialStates;
    uint64_t nextRunId = 0;

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
        if (!sConfigMgr->GetOption<bool>("SelfBotRpg.Debug", false))
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
                "[SBRPG] Material summary: {} ms, {} kills, {} requested items, {} loot events, {} corpse timeouts ({}).",
                elapsed, it->second.kills, it->second.gatheredItems, it->second.lootEvents,
                it->second.corpseTimeouts, reason);
        }
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        {
            if (it->second.lootStrategyOverridden)
            {
                ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Set(
                    LootStrategyValue::instance(it->second.previousLootStrategy));
                it->second.lootStrategyOverridden = false;
            }
            it->second.materialStrategy.Release(ai);
            it->second.materialLootStrategy.Release(ai);
            it->second.lootStrategy.Release(ai);
            it->second.gatherStrategy.Release(ai);
            it->second.grindStrategy.Release(ai);
            it->second.rpgStrategy.Release(ai);
        }
        materialStates.erase(it);
    }

    bool HasSkinningTool(Player* player)
    {
        return player && (player->HasItemCount(7005, 1) || player->HasItemCount(40772, 1) ||
            player->HasItemCount(40893, 1) || player->HasItemCount(12709, 1) ||
            player->HasItemCount(19901, 1));
    }

    bool StartMaterial(Player* player, std::string materialName, uint32 durationMinutes,
                       uint32 quantityGoal, std::string* error)
    {
        if (!player || !GET_PLAYERBOT_AI(player) || !IsSelfBot(player))
        { if (error) *error = "Enable self-bot mode first (.playerbots bot self)."; return false; }
        if (Sbrpg::IsActive(player))
        { if (error) *error = "Stop the node-farming run first."; return false; }
        if (materialStates.contains(player->GetGUID()))
            StopMaterial(player);

        uint32 itemId = 0;
        Sbrpg::Materials::MaterialDefinition const* material = ResolveMaterial(std::move(materialName), itemId);
        if (!material)
        { if (error) *error = "Unknown material; use a catalog name (cloth or leather) or an exact item ID."; return false; }

        float const minimumChance = sConfigMgr->GetOption<float>("SelfBotRpg.MaterialMinimumChance", 1.0f);
        auto supportsMethod = [material](Sbrpg::Materials::AcquisitionMethod method)
        {
            return std::find(material->methods.begin(), material->methods.end(), method) != material->methods.end();
        };
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        bool const needsSkinning = supportsMethod(Sbrpg::Materials::AcquisitionMethod::Skinning);
        if (needsSkinning && !ai->HasSkill(SKILL_SKINNING))
        { if (error) *error = "This material requires the Skinning skill."; return false; }
        if (needsSkinning && !HasSkinningTool(player))
        { if (error) *error = "This material requires a skinning knife or compatible tool."; return false; }
        std::vector<uint32> sourceEntries;
        for (Sbrpg::Materials::LootSource const& source : Sbrpg::Materials::LootSourceIndex::Find(itemId))
            if (!source.questRequired && supportsMethod(source.method) &&
                source.estimatedChance >= minimumChance &&
                std::find(sourceEntries.begin(), sourceEntries.end(), source.creatureEntry) == sourceEntries.end())
                sourceEntries.push_back(source.creatureEntry);
        if (sourceEntries.empty())
        { if (error) *error = Acore::StringFormat("No supported creature sources meet the {:.2f}% minimum chance.", minimumChance); return false; }

        std::vector<Sbrpg::Materials::Hotspot> hotspots =
            Sbrpg::Materials::HotspotPlanner::Build(
                Sbrpg::Materials::CreatureSpawnRepository::Load(player, sourceEntries));
        hotspots = Sbrpg::Materials::HotspotPlanner::Plan(player, std::move(hotspots));
        if (hotspots.empty())
        { if (error) *error = "No reachable creature hotspots found in the current zone."; return false; }

        Sbrpg::Materials::MaterialFarmState& state = materialStates[player->GetGUID()];
        state = Sbrpg::Materials::MaterialFarmState();
        state.active = true;
        state.itemId = itemId;
        state.quantityGoal = quantityGoal;
        state.inventoryStart = player->GetItemCount(itemId, true);
        state.inventoryCount = state.inventoryStart;
        Sbrpg::BeginActivitySession(state.session, player, durationMinutes, getMSTime());
        state.mapId = player->GetMapId();
        state.zoneId = player->GetZoneId();
        state.creatureEntries = std::move(sourceEntries);
        state.needsSkinning = needsSkinning;
        state.hotspots = std::move(hotspots);
        state.lootStrategy.SetStrategy("loot");
        state.lootStrategy.Suspend(ai);
        state.previousLootStrategy = ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Get()->GetName();
        ai->GetAiObjectContext()->GetValue<LootStrategy*>("loot strategy")->Set(
            LootStrategyValue::instance("all"));
        state.lootStrategyOverridden = true;
        state.materialLootStrategy.SetStrategy("sbrpg material loot");
        state.materialLootStrategy.Acquire(ai);
        if (needsSkinning)
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
        return true;
    }

    void SetMaterialPhase(Player* player, Sbrpg::Materials::MaterialFarmState& state, std::string phase)
    {
        if (state.phase != phase)
        {
            state.phase = std::move(phase);
            Debug(player, Acore::StringFormat("material phase: {}", state.phase));
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
        uint32 const reservedBagPercent = sConfigMgr->GetOption<uint32>(
            "SelfBotRpg.MaterialReservedBagPercent", 0);
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

    class SelfbotMaterialTravel : public MovementAction
    {
    public:
        SelfbotMaterialTravel(PlayerbotAI* ai) : MovementAction(ai, "sbrpg material travel") { }
        bool MoveToHotspot(uint32 mapId, float x, float y, float z)
        {
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
            uint32 const actionDelayMs = sConfigMgr->GetOption<uint32>(
                "SelfBotRpg.ActionDelayMs", 1000);
            if (state.lastActionMs != 0 && now - state.lastActionMs < actionDelayMs)
                return false;
            state.lastActionMs = now;
            UpdateMaterialReturn(bot);
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
                if (activeLoot.IsLootPossible(bot))
                {
                    state.invalidLootSinceMs = 0;
                    SetMaterialPhase(bot, state, state.session.returnRequested ?
                        "finishing loot before return" : "looting");
                    return false;
                }
                if (state.invalidLootSinceMs == 0)
                    state.invalidLootSinceMs = now;
                if (now - state.invalidLootSinceMs < 5000 || !bot->GetLootGUID().IsEmpty())
                {
                    SetMaterialPhase(bot, state, "waiting for stock loot target recovery");
                    return false;
                }
                Debug(bot, Acore::StringFormat("material releasing stale loot target: guid {}",
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

            if (!state.target.IsEmpty())
            {
                Creature* corpse = botAI->GetCreature(state.target);
                if (corpse && corpse->getDeathState() == DeathState::Corpse)
                {
                    bool const needsLoot = corpse->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                    LootObject corpseLoot(bot, state.target);
                    // Normal corpse loot always has priority. Do not expose a
                    // skinnable target to stock gather until the lootable flag
                    // is gone, otherwise a ranged bot can skin/interrupt while
                    // regular corpse items remain.
                    bool const skinningReady = !needsLoot && corpseLoot.skillId == SKILL_SKINNING &&
                        HasSkinningTool(bot) && botAI->HasSkill(SKILL_SKINNING) &&
                        bot->GetSkillValue(SKILL_SKINNING) >= corpseLoot.reqSkillValue;
                    bool const corpseActionable = needsLoot || skinningReady;
                    if (corpseActionable)
                    {
                        if (state.corpseWaitSinceMs == 0)
                            state.corpseWaitSinceMs = now;
                        // Ranged kills commonly land outside the stock
                        // LootDistance (15yd by default), where `loot` will
                        // never select the queued corpse. Approach it only
                        // while stock has no selected/open loot, using the same
                        // bounded mmap movement as hotspot travel.
                        if (bot->GetDistance(corpse) > sPlayerbotAIConfig.lootDistance - 2.0f)
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
                        uint32 const corpseWaitLimit = state.session.returnRequested ? 10000 : 30000;
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
            uint32 const actionDelayMs = sConfigMgr->GetOption<uint32>(
                "SelfBotRpg.ActionDelayMs", 1000);
            if (mutableState.lastActionMs != 0 && actionNow - mutableState.lastActionMs < actionDelayMs)
                return false;
            mutableState.lastActionMs = actionNow;
            if (bot->IsInCombat())
            {
                SetPhase(mutableState, Sbrpg::FarmPhase::CombatPaused,
                    mutableState.session.returnRequested ? "combat active; return-home objective retained" : "combat active; farm destination retained");
                mutableState.step.valid = false;
                mutableState.stepIssued = false;
                return false;
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
            if ((!stockLootTarget.IsEmpty() && stockLootTarget.IsLootPossible(bot)) || !bot->GetLootGUID().IsEmpty())
            {
                SetPhase(mutableState, Sbrpg::FarmPhase::Looting,
                    mutableState.session.returnRequested ? "finishing active loot before return" : "stock playerbots owns active loot target");
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
            }
            if (!mutableState.activeGatherNode.IsEmpty())
            {
                GameObject* activeNode = botAI->GetGameObject(mutableState.activeGatherNode);
                if (!activeNode || !activeNode->IsInWorld() || !activeNode->isSpawned())
                {
                    ++mutableState.harvested;
                    mutableState.currentSpawn = 0;
                    mutableState.pendingGatherNode = ObjectGuid::Empty;
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
                if (now - mutableState.gatherStartedMs < 30000)
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
                mutableState.currentSpawn = 0;
                SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "gather timed out; replanning");
                return true;
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
                    mutableState.gatherReadyMs = now + 6000;
                }
                if (stockOwnsNode)
                {
                    mutableState.gatherReadyMs = now + 6000;
                    SetPhase(mutableState, Sbrpg::FarmPhase::Looting, "stock gather owns live node");
                    return false;
                }
                if (now < mutableState.gatherReadyMs)
                {
                    SetPhase(mutableState, Sbrpg::FarmPhase::GatherPending, "waiting for stock gather claim");
                    return false;
                }
                mutableState.blacklistedUntilMs[go->GetSpawnId()] = now + 30000;
                mutableState.pendingGatherNode = ObjectGuid::Empty;
                SetPhase(mutableState, Sbrpg::FarmPhase::SelectingNode, "stock gather did not claim live node");
                continue;
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
            auto materialIt = player ? materialStates.find(player->GetGUID()) : materialStates.end();
            if (materialIt != materialStates.end() && materialIt->second.active)
            {
                uint32 const current = player->GetItemCount(materialIt->second.itemId, true);
                if (current > materialIt->second.inventoryCount)
                {
                    uint32 const gained = current - materialIt->second.inventoryCount;
                    materialIt->second.gatheredItems += gained;
                    Debug(player, Acore::StringFormat("material inventory gain: {} (total {})",
                        gained, materialIt->second.gatheredItems));
                }
                materialIt->second.inventoryCount = current;
            }
            UpdateMaterialReturn(player);

            auto it = player ? states.find(player->GetGUID()) : states.end();
            if (it == states.end() || !it->second.active)
                return;
            uint32 const now = getMSTime();
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
            state.needsSkinning ? "1" : "0"
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
        uint32 index = 0;
        for (auto const& source : sources)
        {
            SendAddon(player, type, Sbrpg::Protocol::Build("MATERIAL_SOURCE", {
                requestId, std::to_string(index++), std::to_string(sources.size()),
                std::to_string(source.creatureEntry), Sbrpg::Materials::MethodName(source.method),
                Acore::StringFormat("{:.3f}", source.estimatedChance),
                source.questRequired ? "quest" : "normal"
            }));
        }
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
                    frame.requestId, "1", "MATERIAL_CATALOG,MATERIAL_SOURCES,START_MATERIAL,MATERIAL_STATUS"
                }));
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
                handler->PSendSysMessage("Material run: item {}, {} / {} items, {} kills, {} loot events, {} corpse timeouts, {} sec remaining, phase '{}', return {}, home map {} ({:.1f}, {:.1f}, {:.1f}), distance {:.1f}, reason '{}'.",
                    it->second.itemId, it->second.gatheredItems, it->second.quantityGoal,
                    it->second.kills, it->second.lootEvents, it->second.corpseTimeouts,
                    Sbrpg::ActivityRemainingSeconds(it->second.session, getMSTime()),
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
               uint32 durationMinutes, std::string* error)
    {
        if (!player || !GET_PLAYERBOT_AI(player) || !IsSelfBot(player))
        { if (error) *error = "Enable self-bot mode first (.playerbots bot self)."; return false; }
        if (entries.empty()) { if (error) *error = "Select at least one node type."; return false; }
        FarmState& state = states[player->GetGUID()];
        state = FarmState();
        state.active = true;
        state.runId = ++nextRunId;
        state.revision = 0;
        SetPhase(state, FarmPhase::Planning, "loading route nodes");
        state.profession = profession;
        state.entries = std::move(entries); state.currentSpawn = 0; state.harvested = 0;
        Sbrpg::BeginActivitySession(state.session, player, durationMinutes, getMSTime());
        state.lastGatheredNode = ObjectGuid::Empty; state.activeGatherNode = ObjectGuid::Empty; state.gatheredItems = 0;
        state.zoneId = player->GetZoneId(); state.mapId = player->GetMapId();
        state.startedInside = !player->IsOutdoors();
        state.approachNode = ObjectGuid::Empty;
        state.pendingGatherNode = ObjectGuid::Empty;
        state.attemptsBeforeBlacklist = sConfigMgr->GetOption<uint32>("SelfBotRpg.AttemptsBeforeBlacklist", 3);
        state.failedBlacklistSeconds = sConfigMgr->GetOption<uint32>("SelfBotRpg.FailedNodeBlacklistSeconds", 120);
        state.emptyBlacklistSeconds = sConfigMgr->GetOption<uint32>("SelfBotRpg.EmptyNodeBlacklistSeconds", 120);
        state.gatherSettleDelayMs = sConfigMgr->GetOption<uint32>("SelfBotRpg.GatherSettleDelayMs", 500);
        state.stayInCurrentZone = sConfigMgr->GetOption<bool>("SelfBotRpg.StayInCurrentZone", true);
        state.route = Sbrpg::NodeRepository::LoadRoute(player, state,
            [profession](uint32 entry) { return IsGatheringEntry(profession, entry); });
        Debug(player, Acore::StringFormat("loaded {} route nodes on map {} in zone {}", state.route.size(), player->GetMapId(), state.zoneId));
        state.routeIndex = 0;
        state.route = Sbrpg::BuildRoutePlan(std::move(state.route), player->GetPositionX(),
                                             player->GetPositionY(), player->GetPositionZ());
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        state.lootStrategy.SetStrategy("loot");
        state.lootStrategy.Acquire(ai);
        state.gatherStrategy.SetStrategy("gather");
        state.gatherStrategy.Acquire(ai);
        ai->ChangeStrategy("+sbrpg farm", BOT_STATE_NON_COMBAT);
        return true;
    }
    void Finish(Player* player, std::string reason)
    {
        if (!player) return;
        auto it = states.find(player->GetGUID());
        if (it == states.end()) return;
        FarmState& state = it->second;
        Transition(state, FarmPhase::Stopped, std::move(reason));
        state.active = false;
        player->StopMoving();
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        {
            ai->ChangeStrategy("-sbrpg farm", BOT_STATE_NON_COMBAT);
            state.lootStrategy.Release(ai);
            state.gatherStrategy.Release(ai);
        }
        PublishStatus(player);
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
            timer = Acore::StringFormat(", {}:{:02} remaining", remaining / 60, remaining % 60);
        }
        return "SelfBot RPG: " + std::string(PhaseName(state->phase)) + reason + " | farming " + std::string(ProfessionName(state->profession)) + " (" +
               std::to_string(state->route.size()) + " eligible route nodes, " +
               std::to_string(state->harvested) + " gathers / " + std::to_string(state->gatheredItems) + " items, " +
               Acore::StringFormat("{:.2f}/min {:.3f}/sec", perMinute, perMinute / 60.0) + timer + ").";
    }
}

void AddSelfbotRpgScripts()
{
    new SelfbotRpgRegistrar();
    new SelfbotRpgStatusScript();
    new SelfbotRpgLootScript();
    new SelfbotRpgAddonHook();
    new SelfbotRpgCommand();
}
