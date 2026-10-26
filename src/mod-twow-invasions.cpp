#include "ScriptObjects.h"
#include "Log.h"

extern void RegisterInvasionScripts();

namespace
{
    class ModTwowInvasionsStartupScript : public WorldScript
    {
    public:
        ModTwowInvasionsStartupScript()
            : WorldScript("mod-twow-invasions_startup", { WORLDHOOK_ON_STARTUP })
        {
        }

        void OnStartup() override
        {
            sLog.outString("[mod-twow-invasions] World PvP Invasions & PlayerBot Phantoms module loaded successfully.");
        }
    };
}

void Addmod_twow_invasionsScripts()
{
    new ModTwowInvasionsStartupScript();
    RegisterInvasionScripts();
}
