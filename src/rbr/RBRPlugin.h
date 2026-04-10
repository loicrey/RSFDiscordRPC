#pragma once

#include "IPlugin.h"
#include "IRBRGame.h"

#include "../core/PluginRuntime.h"

#include <string>

namespace rsf
{
    class RBRPlugin final : public IPlugin
    {
    public:
        RBRPlugin(IRBRGame* game, HMODULE moduleHandle);
        ~RBRPlugin() override;

        const char* GetName(void) override;
        void DrawFrontEndPage(void) override;
        void DrawResultsUI(void) override;
        void HandleFrontEndEvents(char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect) override;
        void TickFrontEndPage(float fTimeDelta) override;
        void StageStarted(int iMap, const char* ptxtPlayerName, bool bWasFalseStart) override;
        void HandleResults(float fCheckPoint1, float fCheckPoint2, float fFinishTime, const char* ptxtPlayerName) override;
        void CheckPoint(float fCheckPointTime, int iCheckPointID, const char* ptxtPlayerName) override;

    private:
        IRBRGame* game_ = nullptr;
        PluginRuntime runtime_{};
        std::string pluginName_ = "RSFDiscordRPC - Display current game activity with Discord Rich Presence. \xC2\xA9"" Reyo, 0.0.1";
    };
}
