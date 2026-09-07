#include "Integration/RuntimeDependencies.h"
#include "Core/SbrpgConfig.h"
#include "Core/SbrpgLogging.h"

namespace Sbrpg::Runtime
{
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

    void Debug(Player* bot, std::string const& message)
    {
        if (!runtimeSettings.debug)
            return;
        LOG_DEBUG("module", "[SBRPG] {}: {}", bot->GetName(), message);
        if (WorldSession* session = bot->GetSession())
            ChatHandler(session).PSendSysMessage("[SBRPG] {}", message);
    }

}
