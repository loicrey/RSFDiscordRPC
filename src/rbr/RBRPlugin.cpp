#include "pch.h"

#include "../core/Logger.h"
#include "../core/PluginConfig.h"
#include "RBRPlugin.h"

namespace rsf
{
    RBRPlugin::RBRPlugin(IRBRGame* game, HMODULE moduleHandle)
        : game_(game)
    {
        const PluginConfig config = PluginConfig::Load(moduleHandle);
        Logger::Initialize(moduleHandle, config.debugLogging);
        RSF_LOG("RBRPlugin created.");
        if (runtime_.start(moduleHandle))
        {
            RSF_LOG("Plugin runtime started.");
        }
        else
        {
            RSF_LOG("Plugin runtime failed to start.");
        }
    }

    RBRPlugin::~RBRPlugin()
    {
        RSF_LOG("RBRPlugin destroyed.");
        runtime_.stop();
    }

    const char* RBRPlugin::GetName(void)
    {
        return pluginName_.c_str();
    }

    void RBRPlugin::DrawFrontEndPage(void)
    {
    }

    void RBRPlugin::DrawResultsUI(void)
    {
    }

    void RBRPlugin::HandleFrontEndEvents(char, bool, bool, bool, bool, bool)
    {
    }

    void RBRPlugin::TickFrontEndPage(float)
    {
    }

    void RBRPlugin::StageStarted(int iMap, const char*, bool)
    {
        RSF_LOG("RBR StageStarted. mapId=" + std::to_string(iMap));
    }

    void RBRPlugin::HandleResults(float, float, float, const char*)
    {
        RSF_LOG("RBR HandleResults called.");
    }

    void RBRPlugin::CheckPoint(float, int, const char*)
    {
    }
}
