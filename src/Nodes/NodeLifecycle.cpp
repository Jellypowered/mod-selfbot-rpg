#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityLifecycle.h"
#include "Core/ActivityRegistry.h"
#include "Core/SbrpgConfig.h"
#include "Core/SbrpgLogging.h"
#include "Integration/PlayerbotIntegration.h"
#include "Nodes/NodeResources.h"
#include "Protocol/ResponsePublisher.h"
#include "Safety/EligibilityPolicy.h"
using namespace Sbrpg::Runtime;
namespace Sbrpg
{
    bool Start(Player* player, Profession profession, std::vector<uint32> entries,
               uint32 durationMinutes, std::string* error, uint32 targetItemId,
               uint32 quantityGoal)
    {
        // Every node-farming entry point shares this guard. Do not replace an
        // active material session: its return target, equipment snapshot, and
        // loot ownership must remain intact until it finishes returning.
        auto const materialIt = ActivityRegistry::Materials().find(player ? player->GetGUID() : ObjectGuid::Empty);
        if (materialIt != ActivityRegistry::Materials().end() && materialIt->second.active)
        {
            if (error)
                *error = "Stop the active material session before starting node farming.";
            return false;
        }
        if (!RuntimeEnabled())
        { if (error) *error = "SBRPG is disabled by SelfBotRpg.Enable."; return false; }
        if (!player)
        { if (error) *error = "Player is unavailable."; return false; }
        auto existingMaterial = ActivityRegistry::Materials().find(player->GetGUID());
        if (existingMaterial != ActivityRegistry::Materials().end() && existingMaterial->second.active)
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
        FarmState& state = ActivityRegistry::Nodes()[player->GetGUID()];
        state = FarmState();
        state.active = true;
        state.selfBotEnabledBySbrpg = enabledBySbrpg;
        state.runId = ++ActivityRegistry::RunSequence();
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
        SetPhase(state, FarmPhase::Planning, Acore::StringFormat("{}. {} route loaded: {} nearby node spawns.",
            enabledBySbrpg ? "Selfbot enabled" : "Selfbot already active", ProfessionName(profession), state.route.size()));
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
        auto it = ActivityRegistry::Nodes().find(player->GetGUID());
        if (it == ActivityRegistry::Nodes().end()) return;
        FarmState& state = it->second;
        bool const disableSelfBot = state.selfBotEnabledBySbrpg;
        std::string const completedReason = Acore::StringFormat("Completed: {} gathers, {} items. {}",
            state.harvested, state.gatheredItems, reason);
        Transition(state, FarmPhase::Stopped, completedReason);
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

    void ForceStop(Player* player)
    {
        if (!player) return;
        auto it = ActivityRegistry::Nodes().find(player->GetGUID());
        if (it == ActivityRegistry::Nodes().end() || !it->second.active)
            return;
        FarmState& state = it->second;
        bool const disableSelfBot = state.selfBotEnabledBySbrpg;
        Transition(state, FarmPhase::Stopped, "Force stopped by user.");
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
        auto it = ActivityRegistry::Nodes().find(player->GetGUID());
        if (it == ActivityRegistry::Nodes().end())
            return;

        FarmState& state = it->second;
        if (!state.active)
            return;
        if (!runtimeSettings.returnHomeOnStop)
        {
            ForceStop(player);
            return;
        }

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
        SetPhase(state, FarmPhase::Returning, "Returning home: stopped by user.");
        PublishStatus(player);
    }
    FarmState const* Get(Player* player)
    { auto it = player ? ActivityRegistry::Nodes().find(player->GetGUID()) : ActivityRegistry::Nodes().end(); return it == ActivityRegistry::Nodes().end() ? nullptr : &it->second; }
    bool IsActive(Player* player) { FarmState const* state = Get(player); return state && state->active; }
    bool SetOption(Player* player, std::string const& key, uint32 value, std::string* error)
    {
        FarmState const* found = Get(player);
        if (!found || !found->active) { if (error) *error = "start farming first"; return false; }
        FarmState& state = ActivityRegistry::Nodes()[player->GetGUID()];
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
