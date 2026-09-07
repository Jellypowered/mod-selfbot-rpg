#include "Integration/RuntimeDependencies.h"
#include "Integration/ActionRegistration.h"
#include "Integration/WorldScriptHandlers.h"

namespace Sbrpg::Runtime
{
    class SelfbotRpgRegistrar : public WorldScript
    {
    public:
        SelfbotRpgRegistrar() : WorldScript("SelfbotRpgRegistrar") { }
        void OnUpdate(uint32 /*diff*/) override
        {
            if (_done) return;
            _done = true;
            RegisterPlayerbotContexts();
        }
    private:
        bool _done = false;
    };

void RegisterSelfbotRpgRegistrar() { new SelfbotRpgRegistrar(); }

}
