#include "Integration/RuntimeDependencies.h"
#include "Core/ActivityRegistry.h"
#include "Fishing/FishingSourceRepository.h"
#include "Materials/MaterialSourceSelector.h"
#include "Nodes/NodeResources.h"
#include "Protocol/ResponsePublisher.h"

namespace Sbrpg::Runtime
{
    std::string AddonStatus(Player* player)
    {
        return Sbrpg::BuildStatusFrame(Sbrpg::Get(player), getMSTime());
    }

    std::string AddonMaterialStatus(Player* player, std::string const& requestId)
    {
        auto const it = player ? ActivityRegistry::Materials().find(player->GetGUID()) : ActivityRegistry::Materials().end();
        if (it == ActivityRegistry::Materials().end())
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
        Sbrpg::FarmState& state = ActivityRegistry::Nodes()[player->GetGUID()];
        SendAddon(player, CHAT_MSG_WHISPER, AddonStatus(player));
        state.lastPublishedRevision = state.revision;
        state.lastStatusPublishMs = getMSTime();
    }

}
