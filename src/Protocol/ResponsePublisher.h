#pragma once
#include "Core/ServiceTypes.h"

namespace Sbrpg::Runtime
{
std::string AddonStatus(Player* player);
std::string AddonMaterialStatus(Player* player, std::string const& requestId);
void SendMaterialCatalog(Player* player, ChatMsg type, std::string const& requestId);
void SendMaterialSources(Player* player, ChatMsg type, std::string const& requestId,
                             std::string const& materialName);
ChatMsg ReplyChatType(uint32 /*type*/);
void SendAddon(Player* player, ChatMsg chatType, std::string const& payload);
void PublishStatus(Player* player);
}
