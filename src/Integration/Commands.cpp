#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Core/SbrpgLogging.h"
#include "Integration/Commands.h"
#include "Materials/MaterialLifecycle.h"
#include "Materials/MaterialStart.h"
#include "Protocol/RequestConfiguration.h"

namespace Sbrpg::Runtime
{
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
                { "force-stop", HandleForceStop, SEC_PLAYER, Console::No },
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
            if (!ConfigureFarm(player, profession, entries, 0, 0, &error))
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
            auto it = player ? ActivityRegistry::Materials().find(player->GetGUID()) : ActivityRegistry::Materials().end();
            if (it == ActivityRegistry::Materials().end())
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
        static bool HandleForceStop(ChatHandler* handler, Tail /*args*/)
        {
            Player* p = handler->GetSession()->GetPlayer();
            Sbrpg::ForceStop(p);
            ForceStopMaterial(p);
            handler->SendSysMessage("SelfBot RPG force-stopped; activity and module-owned state cleared without returning home.");
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

void RegisterSelfbotRpgCommand() { new SelfbotRpgCommand(); }

}
