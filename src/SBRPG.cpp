#include "Core/SbrpgConfig.h"
#include "Integration/Commands.h"
#include "Integration/PlayerScriptHandlers.h"
#include "Integration/WorldScriptHandlers.h"
#include "Loot/LootEvents.h"
#include "Protocol/RequestHandlers.h"

void AddSelfbotRpgScripts()
{
    using namespace Sbrpg::Runtime;
    LoadRuntimeSettings();
    RegisterSelfbotRpgRegistrar();
    RegisterSelfbotRpgStatusScript();
    RegisterSelfbotRpgLootScript();
    RegisterSelfbotRpgAddonHook();
    RegisterSelfbotRpgCommand();
}
