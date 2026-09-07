#include "Integration/RuntimeDependencies.h"
#include "Core/SbrpgConfig.h"
#include "Fishing/FishingStart.h"
#include "Materials/MaterialLifecycle.h"
#include "Materials/MaterialSourceSelector.h"
#include "Materials/MaterialStart.h"
#include "Protocol/RequestConfiguration.h"
#include "Protocol/RequestHandlers.h"
#include "Protocol/ResponsePublisher.h"

namespace Sbrpg::Runtime
{
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

void RegisterSelfbotRpgAddonHook() { new SelfbotRpgAddonHook(); }

}
