#include "SBRPG.h"

#include "Action.h"
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
#include "Config.h"
#include "DatabaseEnv.h"
#include "Event.h"
#include "GameObject.h"
#include "Group.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "LootObjectStack.h"
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
#include <limits>
#include <sstream>
#include <unordered_map>

using namespace Acore::ChatCommands;

namespace
{
    std::unordered_map<ObjectGuid, Sbrpg::FarmState> states;

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

    void Debug(Player* bot, std::string const& message)
    {
        if (!sConfigMgr->GetOption<bool>("SelfBotRpg.Debug", false))
            return;
        LOG_DEBUG("module", "[SBRPG] {}: {}", bot->GetName(), message);
        if (WorldSession* session = bot->GetSession())
            ChatHandler(session).PSendSysMessage("[SBRPG] {}", message);
    }

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
            if (bot->IsInCombat())
            {
                mutableState.activity = "combat paused";
                return false;
            }
            if (bot->isDead())
            {
                mutableState.activity = "dead; waiting";
                return false;
            }
            if (state->mapId != bot->GetMapId())
            {
                Debug(bot, "farm stopped after map change; route coordinates are map-local");
                Sbrpg::Stop(bot);
                return false;
            }

            // A loaded selected node always wins over the recorded route point.
            // The stock open-loot action performs the profession/lock check and
            // casts the appropriate gathering spell, so combat/loot mechanics
            // remain wholly owned by mod-playerbots.
            uint32 const now = getMSTime();
            mutableState.activity = "scanning nearby nodes";
            // Refresh the live-node cache at most twice per second. GUIDs are
            // retained, not raw pointers, so unloaded/despawned objects are safe.
            float const liveScanRadius = std::max(30.0f, sPlayerbotAIConfig.sightDistance);
            if (now - mutableState.lastLiveScanMs >= 1500)
            {
                mutableState.liveNodes.clear();
                std::list<GameObject*> scanned;
                AnyGameObjectInObjectRangeCheck check(bot, liveScanRadius);
                Acore::GameObjectListSearcher<AnyGameObjectInObjectRangeCheck> searcher(bot, scanned, check);
                Cell::VisitObjects(bot, searcher, liveScanRadius);
                for (GameObject* go : scanned)
                    if (go && go->isSpawned() && HasEntry(*state, go->GetEntry()))
                        mutableState.liveNodes.push_back(go->GetGUID());
                mutableState.lastLiveScanMs = now;
            }
            if (!mutableState.activeGatherNode.IsEmpty())
            {
                if (now - mutableState.gatherAttemptedMs < 8000)
                {
                    mutableState.activity = "waiting for gather result";
                    return true;
                }
                mutableState.blacklistedUntilMs[mutableState.activeGatherNode.GetCounter()] =
                    now + 1000 * mutableState.emptyBlacklistSeconds;
                mutableState.activeGatherNode = ObjectGuid::Empty;
                mutableState.pendingGatherNode = ObjectGuid::Empty;
                mutableState.currentSpawn = 0;
                mutableState.activity = "gather timed out; replanning";
                return true;
            }
            for (ObjectGuid const& guid : mutableState.liveNodes)
            {
                GameObject* go = botAI->GetGameObject(guid);
                if (!go || !go->IsInWorld() || !go->isSpawned() || !HasEntry(*state, go->GetEntry()))
                    continue;
                LootObject loot(bot, go->GetGUID());
                if (!MatchesProfession(*state, loot) || !loot.IsLootPossible(bot))
                    continue;
                // The route point only gets us to the node's neighborhood.
                // Finish with playerbots' collision-aware approach before
                // issuing the profession cast; this avoids casting from 12yd.
                if (bot->GetDistance(go) > 5.0f)
                {
                    mutableState.activity = "approaching live node";
                    return MoveNear(go, 3.0f, MovementPriority::MOVEMENT_NORMAL);
                }
                if (mutableState.pendingGatherNode != go->GetGUID())
                {
                    mutableState.pendingGatherNode = go->GetGUID();
                    mutableState.activity = "settling at node";
                    mutableState.gatherReadyMs = now + mutableState.gatherSettleDelayMs;
                    bot->StopMoving();
                    return true;
                }
                if (now < mutableState.gatherReadyMs)
                    return true;
                mutableState.activity = "gathering";
                context->GetValue<LootObject>("loot target")->Set(loot);
                mutableState.activeGatherNode = go->GetGUID();
                mutableState.gatherAttemptedMs = now;
                bool const opened = botAI->DoSpecificAction("open loot", Event("sbrpg", "", bot), true);
                if (opened)
                    mutableState.currentSpawn = 0;
                if (opened && mutableState.lastGatheredNode != go->GetGUID())
                {
                    ++mutableState.harvested;
                    mutableState.lastGatheredNode = go->GetGUID();
                }
                return opened;
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
            if (spawn == 0 && !NextRoutePoint(mutableState, spawn, x, y, z))
            {
                mutableState.activity = "waiting for eligible route node";
                return true;
            }
            mutableState.activity = "travelling to route node";

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
                mutableState.activity = "route node empty; replanning";
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

            // Ask the navmesh before committing to a route point. A hard
            // NOPATH result is different from a temporarily slow path: skip it
            // immediately rather than burning all three progress windows.
            PathGenerator path(bot);
            path.CalculatePath(x, y, z);
            PathType const pathType = path.GetPathType();
            if (pathType & (PATHFIND_NOPATH | PATHFIND_NOT_USING_PATH | PATHFIND_SHORTCUT | PATHFIND_FARFROMPOLY))
            {
                mutableState.blacklistedUntilMs[spawn] = now + 1000 * mutableState.failedBlacklistSeconds;
                mutableState.currentSpawn = 0;
                mutableState.activity = "route blocked; replanning";
                Debug(bot, Acore::StringFormat("spawn {} has no navmesh path; blacklisted", spawn));
                return true;
            }
            // Use playerbots' normal terrain-aware movement implementation for
            // the complete destination. It performs ground-height correction,
            // nearby-Z recovery, duplicate suppression, movement pacing, and
            // spline generation without repeatedly resetting a short steering
            // waypoint every AI tick.
            if (bot->GetExactDist(x, y, z) > 2.0f)
                return MoveTo(bot->GetMapId(), x, y, z, false, false, false, false,
                              MovementPriority::MOVEMENT_NORMAL, false);

            // Keep the strategy alive while a valid destination is waiting for
            // the next movement attempt.
            return true;
        }

    private:
        bool NextRoutePoint(Sbrpg::FarmState& state, uint32& spawn, float& x, float& y, float& z)
        {
            if (state.entries.empty())
                return false;

            // Follow the pre-sorted route sequentially, skipping visited/blacklisted
            // points. Reset visited flags only when the full cycle completes.
            if (!state.route.empty())
            {
                uint32 const now = getMSTime();
                // Refresh path costs periodically for accuracy.
                if (now - state.lastRoutePlanMs >= 30000)
                {
                    state.pathCostCache.clear();
                    state.lastRoutePlanMs = now;
                }
                uint32 start = state.routeIndex;
                uint32 attempts = 0;
                while (attempts < state.route.size())
                {
                    Sbrpg::RoutePoint& point = state.route[state.routeIndex];
                    // Skip already visited points in this cycle.
                    if (!point.visited)
                    {
                        auto blocked = state.blacklistedUntilMs.find(point.spawn);
                        bool valid = (blocked == state.blacklistedUntilMs.end() || now >= blocked->second);
                        if (valid)
                        {
                            point.visited = true;
                            spawn = point.spawn; x = point.x; y = point.y; z = point.z;
                            state.routeIndex = (state.routeIndex + 1) % state.route.size();
                            return true;
                        }
                    }
                    state.routeIndex = (state.routeIndex + 1) % state.route.size();
                    if (state.routeIndex == start) break; // full cycle, all blacklisted
                    ++attempts;
                }
                // Cycle complete: reset visited flags and start fresh.
                for (Sbrpg::RoutePoint& p : state.route) p.visited = false;
                state.routeIndex = 0;
                return false; // pick next on the following tick
            }

            std::ostringstream ids;
            for (size_t i = 0; i < state.entries.size(); ++i)
            {
                if (i) ids << ',';
                ids << state.entries[i];
            }
            QueryResult result = WorldDatabase.Query(
                "SELECT guid, position_x, position_y, position_z FROM gameobject "
                "WHERE map = {} AND id IN ({})", bot->GetMapId(), ids.str());
            if (!result)
                return false;

            uint32 const now = getMSTime();
            float bestDistance = std::numeric_limits<float>::max();
            do
            {
                Field* fields = result->Fetch();
                uint32 const candidate = fields[0].Get<uint32>();
                if (candidate == state.currentSpawn)
                    continue;
                auto blocked = state.blacklistedUntilMs.find(candidate);
                if (blocked != state.blacklistedUntilMs.end() && now < blocked->second)
                    continue;
                float const cx = fields[1].Get<float>();
                float const cy = fields[2].Get<float>();
                float const cz = fields[3].Get<float>();
                if (state.stayInCurrentZone &&
                    bot->GetMap()->GetZoneId(bot->GetPhaseMask(), cx, cy, cz) != state.zoneId)
                    continue;
                float const distance = bot->GetExactDist(cx, cy, cz);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    spawn = candidate;
                    x = cx;
                    y = cy;
                    z = cz;
                }
            } while (result->NextRow());
            return spawn != 0;
        }
    };

    class SelfbotRpgFarmStrategy : public Strategy
    {
    public:
        SelfbotRpgFarmStrategy(PlayerbotAI* ai) : Strategy(ai) { }
        std::string const getName() override { return "sbrpg farm"; }
        uint32 GetType() const override { return STRATEGY_TYPE_NONCOMBAT; }
        std::vector<NextAction> getDefaultActions() override { return { NextAction("sbrpg farm", 12.0f) }; }
    };

    class SelfbotRpgActionContext : public NamedObjectContext<Action>
    {
    public:
        SelfbotRpgActionContext() { creators["sbrpg farm"] = &CreateFarm; }
    private:
        static Action* CreateFarm(PlayerbotAI* ai) { return new SelfbotRpgFarmAction(ai); }
    };

    class SelfbotRpgStrategyContext : public NamedObjectContext<Strategy>
    {
    public:
        SelfbotRpgStrategyContext() : NamedObjectContext<Strategy>(false, false)
        { creators["sbrpg farm"] = &CreateFarm; }
    private:
        static Strategy* CreateFarm(PlayerbotAI* ai) { return new SelfbotRpgFarmStrategy(ai); }
    };

    template <class Ctx>
    void RegisterContexts()
    {
        Ctx::sharedActionContexts.Add(new SelfbotRpgActionContext());
        Ctx::sharedStrategyContexts.Add(new SelfbotRpgStrategyContext());
    }

    class SelfbotRpgActivityScript final : public PlayerScript
    {
    public:
        SelfbotRpgActivityScript() : PlayerScript("SelfbotRpgActivityScript", { PLAYERHOOK_ON_UPDATE }) { }

        void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
        {
            auto it = player ? states.find(player->GetGUID()) : states.end();
            if (it == states.end() || !it->second.active)
                return;

            // Do not call ToggleAFK(): playerbots may set AFK again during its
            // passive pass, producing an on/off chat loop. Directly clear the
            // flag and keep the farmer standing while this run owns activity.
            if (player->isAFK())
                player->RemovePlayerFlag(PLAYER_FLAGS_AFK);
            if (player->IsSitState())
                player->SetStandState(UNIT_STAND_STATE_STAND);
        }
    };

    class SelfbotRpgLootScript : public PlayerScript
    {
    public:
        SelfbotRpgLootScript() : PlayerScript("SelfbotRpgLootScript", { PLAYERHOOK_ON_LOOT_ITEM }) { }
        void OnPlayerLootItem(Player* player, Item* /*item*/, uint32 count, ObjectGuid lootGuid) override
        {
            auto it = player ? states.find(player->GetGUID()) : states.end();
            if (it != states.end() && it->second.active && it->second.activeGatherNode == lootGuid)
            {
                it->second.gatheredItems += count;
                it->second.currentSpawn = 0;
                it->second.pendingGatherNode = ObjectGuid::Empty;
                it->second.activeGatherNode = ObjectGuid::Empty;
            }
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

    bool ConfigureFarm(Player* player, std::string const& professionText, std::string const& entries, std::string* error)
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
        return Sbrpg::Start(player, type, std::move(nodeEntries), error);
    }

    std::string AddonStatus(Player* player)
    {
        Sbrpg::FarmState const* state = Sbrpg::Get(player);
        if (!state || !state->active) return "1\tSTATUS\t0\tidle\t0\t0\t0\t0\t0\t0";
        uint32 elapsed = std::max(1u, getMSTime() - state->startedMs);
        double perMinute = state->gatheredItems * 60000.0 / elapsed;
        return Acore::StringFormat("1\tSTATUS\t1\t{}\t{}\t{}\t{}\t{:.2f}\t{:.3f}\t{}",
            ProfessionName(state->profession), state->route.size(), state->harvested, state->gatheredItems,
            perMinute, perMinute / 60.0, state->currentSpawn);
    }

    ChatMsg ReplyChatType(uint32 type)
    {
        switch (type)
        {
            case CHAT_MSG_PARTY:
            case CHAT_MSG_RAID:
            case CHAT_MSG_WHISPER:
                return static_cast<ChatMsg>(type);
            default:
                return CHAT_MSG_WHISPER;
        }
    }

    void SendAddon(Player* player, ChatMsg chatType, std::string const& payload)
    {
        if (!player || !player->GetSession()) return;
        std::string const wire = "JLYRPG\t" + payload;
        WorldPacket data;
        ChatHandler::BuildChatPacket(data, chatType, LANG_ADDON, player, nullptr, wire.c_str());
        player->SendDirectMessage(&data);
    }

    class SelfbotRpgAddonHook final : public PlayerScript
    {
    public:
        SelfbotRpgAddonHook() : PlayerScript("SelfbotRpgAddonHook") { }

        bool TryHandle(Player* player, uint32 type, uint32 lang, std::string& msg)
        {
            if (!player || lang != LANG_ADDON || msg.rfind("JLYRPG\t", 0) != 0)
                return false;

            std::vector<std::string> fields;
            std::istringstream input(msg.substr(6));
            for (std::string field; std::getline(input, field, '\t');)
                fields.push_back(field);
            if (fields.size() < 2 || fields[0] != "1")
                return true;

            std::string const& opcode = fields[1];
            if (opcode == "START" && fields.size() >= 4)
            {
                std::string error;
                if (!ConfigureFarm(player, fields[2], fields[3], &error))
                    SendAddon(player, ReplyChatType(type), "1\tERROR\t" + error);
            }
            else if (opcode == "STOP")
                Sbrpg::Stop(player);
            else if (opcode == "SET" && fields.size() >= 4)
            {
                uint32 value = static_cast<uint32>(std::strtoul(fields[3].c_str(), nullptr, 10));
                std::string error;
                if (!Sbrpg::SetOption(player, fields[2], value, &error))
                    SendAddon(player, ReplyChatType(type), "1\tERROR\t" + error);
                else
                    SendAddon(player, ReplyChatType(type), "1\tSETTING\t" + fields[2] + "\t" + fields[3]);
            }
            else if (opcode != "STATUS")
                SendAddon(player, ReplyChatType(type), "1\tERROR\tUNKNOWN_OPCODE");

            SendAddon(player, ReplyChatType(type), AddonStatus(player));
            return true;
        }

        bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Player* /*receiver*/) override
        {
            return !TryHandle(player, type, lang, msg);
        }

        bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 lang, std::string& msg, Group* /*group*/) override
        {
            return !TryHandle(player, type, lang, msg);
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
            if (!ConfigureFarm(player, profession, entries, &error))
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
        static bool HandleStop(ChatHandler* handler)
        { Player* p = handler->GetSession()->GetPlayer(); Sbrpg::Stop(p); handler->SendSysMessage("SelfBot RPG farming stopped."); return true; }
        static bool HandleStatus(ChatHandler* handler)
        { handler->SendSysMessage(Sbrpg::Status(handler->GetSession()->GetPlayer())); return true; }
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
    bool Start(Player* player, Profession profession, std::vector<uint32> entries, std::string* error)
    {
        if (!player || !GET_PLAYERBOT_AI(player) || !IsSelfBot(player))
        { if (error) *error = "Enable self-bot mode first (.playerbots bot self)."; return false; }
        if (entries.empty()) { if (error) *error = "Select at least one node type."; return false; }
        FarmState& state = states[player->GetGUID()]; state.active = true; state.profession = profession;
        state.entries = std::move(entries); state.currentSpawn = 0; state.harvested = 0;
        state.startedMs = getMSTime(); state.activity = "planning route"; state.lastGatheredNode = ObjectGuid::Empty; state.activeGatherNode = ObjectGuid::Empty; state.gatheredItems = 0;
        state.zoneId = player->GetZoneId(); state.mapId = player->GetMapId();
        state.startedInside = !player->IsOutdoors();
        state.pendingGatherNode = ObjectGuid::Empty;
        state.attemptsBeforeBlacklist = sConfigMgr->GetOption<uint32>("SelfBotRpg.AttemptsBeforeBlacklist", 3);
        state.failedBlacklistSeconds = sConfigMgr->GetOption<uint32>("SelfBotRpg.FailedNodeBlacklistSeconds", 120);
        state.emptyBlacklistSeconds = sConfigMgr->GetOption<uint32>("SelfBotRpg.EmptyNodeBlacklistSeconds", 120);
        state.gatherSettleDelayMs = sConfigMgr->GetOption<uint32>("SelfBotRpg.GatherSettleDelayMs", 1000);
        state.stayInCurrentZone = sConfigMgr->GetOption<bool>("SelfBotRpg.StayInCurrentZone", true);
        state.route.clear();
        std::ostringstream ids;
        for (size_t i = 0; i < state.entries.size(); ++i) { if (i) ids << ','; ids << state.entries[i]; }
        if (QueryResult routeRows = WorldDatabase.Query("SELECT guid, id, position_x, position_y, position_z FROM gameobject WHERE map = {} AND id IN ({})", player->GetMapId(), ids.str()))
        {
            do
            {
                Field* f = routeRows->Fetch();
                float x = f[2].Get<float>(), y = f[3].Get<float>(), z = f[4].Get<float>();
                if ((!state.stayInCurrentZone || player->GetMap()->GetZoneId(player->GetPhaseMask(), x, y, z) == state.zoneId) &&
                    IsGatheringEntry(profession, f[1].Get<uint32>()))
                    state.route.push_back({f[0].Get<uint32>(), f[1].Get<uint32>(), x, y, z, false});
            } while (routeRows->NextRow());
        }
        Debug(player, Acore::StringFormat("loaded {} route nodes on map {} in zone {}", state.route.size(), player->GetMapId(), state.zoneId));
        // Build a one-time nearest-neighbour route. Unlike radial sorting,
        // this chooses each next point from the previous point and avoids the
        // large return legs that caused visible backtracking.
        state.routeIndex = 0;
        if (!state.route.empty())
        {
            std::vector<Sbrpg::RoutePoint> ordered;
            ordered.reserve(state.route.size());
            std::vector<bool> used(state.route.size(), false);
            float lastX = player->GetPositionX(), lastY = player->GetPositionY(), lastZ = player->GetPositionZ();
            for (size_t step = 0; step < state.route.size(); ++step)
            {
                size_t best = state.route.size();
                float bestDistance = std::numeric_limits<float>::max();
                for (size_t i = 0; i < state.route.size(); ++i)
                {
                    if (used[i]) continue;
                    float const dx = state.route[i].x - lastX, dy = state.route[i].y - lastY, dz = state.route[i].z - lastZ;
                    float const distance = dx * dx + dy * dy + dz * dz;
                    if (distance < bestDistance) { bestDistance = distance; best = i; }
                }
                if (best == state.route.size()) break;
                used[best] = true;
                ordered.push_back(state.route[best]);
                lastX = state.route[best].x; lastY = state.route[best].y; lastZ = state.route[best].z;
            }
            state.route = std::move(ordered);

            // Bounded 2-opt refinement removes obvious crossings from the
            // nearest-neighbour tour. Keep the work bounded: route planning
            // runs on the map thread and must not evaluate every permutation.
            size_t const limit = std::min<size_t>(state.route.size(), 96);
            auto leg = [](Sbrpg::RoutePoint const& a, Sbrpg::RoutePoint const& b)
            {
                float const dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
                return std::sqrt(dx * dx + dy * dy + dz * dz);
            };
            for (uint32 pass = 0; pass < 4 && limit > 3; ++pass)
            {
                bool changed = false;
                for (size_t i = 1; i + 2 < limit; ++i)
                    for (size_t j = i + 1; j + 1 < limit; ++j)
                    {
                        float const before = leg(state.route[i - 1], state.route[i]) +
                            leg(state.route[j], state.route[j + 1]);
                        float const after = leg(state.route[i - 1], state.route[j]) +
                            leg(state.route[i], state.route[j + 1]);
                        if (after + 1.0f < before)
                        {
                            std::reverse(state.route.begin() + i, state.route.begin() + j + 1);
                            changed = true;
                        }
                    }
                if (!changed) break;
            }
        }
        PlayerbotAI* ai = GET_PLAYERBOT_AI(player);
        state.addedLootStrategy = !ai->HasStrategy("loot", BOT_STATE_NON_COMBAT);
        if (state.addedLootStrategy)
            ai->ChangeStrategy("+loot", BOT_STATE_NON_COMBAT);
        ai->ChangeStrategy("+sbrpg farm", BOT_STATE_NON_COMBAT);
        return true;
    }
    void Stop(Player* player)
    {
        if (!player) return;
        bool const removeLoot = states.count(player->GetGUID()) && states[player->GetGUID()].addedLootStrategy;
        // Cancel the active route spline immediately; removing a strategy alone
        // does not cancel a movement generator that it already installed.
        player->StopMoving();
        states.erase(player->GetGUID());
        if (PlayerbotAI* ai = GET_PLAYERBOT_AI(player))
        {
            ai->ChangeStrategy("-sbrpg farm", BOT_STATE_NON_COMBAT);
            if (removeLoot) ai->ChangeStrategy("-loot", BOT_STATE_NON_COMBAT);
        }
    }
    FarmState const* Get(Player* player)
    { auto it = player ? states.find(player->GetGUID()) : states.end(); return it == states.end() ? nullptr : &it->second; }
    bool IsActive(Player* player) { FarmState const* state = Get(player); return state && state->active; }
    bool SetOption(Player* player, std::string const& key, uint32 value, std::string* error)
    {
        FarmState const* found = Get(player);
        if (!found) { if (error) *error = "start farming first"; return false; }
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
        if (!state || !state->active) return "SelfBot RPG: idle.";
        uint32 const elapsedMs = std::max(1u, getMSTime() - state->startedMs);
        double const perMinute = state->harvested * 60000.0 / elapsedMs;
        return "SelfBot RPG: " + state->activity + " | farming " + std::string(ProfessionName(state->profession)) + " (" +
               std::to_string(state->route.size()) + " eligible route nodes, " +
               std::to_string(state->harvested) + " gathers / " + std::to_string(state->gatheredItems) + " items, " +
               Acore::StringFormat("{:.2f}/min {:.3f}/sec).", perMinute, perMinute / 60.0);
    }
}

void AddSelfbotRpgScripts()
{
    new SelfbotRpgRegistrar();
    new SelfbotRpgActivityScript();
    new SelfbotRpgLootScript();
    new SelfbotRpgAddonHook();
    new SelfbotRpgCommand();
}
