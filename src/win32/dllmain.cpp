#include "pch.h"

#include "src/rbr/IRBRGame.h"
#include "src/rbr/RBRPlugin.h"

namespace
{
    HMODULE g_moduleHandle = nullptr;
    rsf::RBRPlugin* g_plugin = nullptr;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_moduleHandle = hModule;
        DisableThreadLibraryCalls(hModule);
        break;

    case DLL_PROCESS_DETACH:
        if (lpReserved == nullptr)
        {
            delete g_plugin;
            g_plugin = nullptr;
        }
        break;

    default:
        break;
    }

    return TRUE;
}

extern "C" __declspec(dllexport) IPlugin* RBR_CreatePlugin(IRBRGame* game)
{
    if (g_plugin == nullptr)
    {
        g_plugin = new rsf::RBRPlugin(game, g_moduleHandle);
    }

    return g_plugin;
}
