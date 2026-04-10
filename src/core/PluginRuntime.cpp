#include "pch.h"

#include "PluginRuntime.h"

#include "DataSourceManager.h"
#include "Logger.h"
#include "PluginConfig.h"
#include "../discord/DiscordManager.h"
#include "../sources/RsfContextSource.h"

namespace
{
    constexpr const char* kDiscordStageImageBaseUrl = "https://rbrrsf-discordrpc.loicrey.com/images/stages/";
    constexpr const char* kDiscordCarImageBaseUrl = "https://rbrrsf-discordrpc.loicrey.com/images/cars/";

    std::string BuildExternalAssetUrl(const char* baseUrl, std::int32_t id)
    {
        if (baseUrl == nullptr || id < 0)
        {
            return {};
        }

        return std::string(baseUrl) + std::to_string(id) + ".jpg";
    }

    bool IsStableStageGameMode(std::int32_t rawGameMode)
    {
        return rawGameMode == 0x01 || rawGameMode == 0x02;
    }

    rsf::GameData BuildStaticState(const char* mode)
    {
        rsf::GameData data;
        data.mode = mode;
        data.largeImageKey = rsf::kDiscordLargeImageKey;
        data.largeImageText = rsf::kDiscordLargeImageText;
        return data;
    }

    rsf::GameData BuildMenuState()
    {
        return BuildStaticState("menu");
    }

    rsf::GameData BuildIncognitoState()
    {
        return BuildStaticState("incognito");
    }

    void ApplyDiscordImages(rsf::GameData& data)
    {
        if (data.inGame && data.mode == "stage")
        {
            data.largeImageKey = BuildExternalAssetUrl(kDiscordStageImageBaseUrl, data.trackId);
            if (data.largeImageKey.empty())
            {
                data.largeImageKey = rsf::kDiscordLargeImageKey;
            }

            data.largeImageText = data.stage.empty() ? rsf::kDiscordLargeImageText : data.stage;
            data.smallImageKey = data.car.empty() ? std::string() : BuildExternalAssetUrl(kDiscordCarImageBaseUrl, data.rsfCarId);
            data.smallImageText = data.smallImageKey.empty() ? std::string() : data.car;
            return;
        }

        data.largeImageKey = rsf::kDiscordLargeImageKey;
        data.largeImageText = rsf::kDiscordLargeImageText;
        data.smallImageKey.clear();
        data.smallImageText.clear();
    }

    bool ShouldPublishPresence(const rsf::GameData& data)
    {
        if (!data.inGame || data.mode == "replay")
        {
            return true;
        }

        if (data.stage.empty() || data.car.empty())
        {
            return false;
        }

        // Only publish a live stage once RBR has reached a stable in-stage state.
        return IsStableStageGameMode(data.rawGameMode);
    }
}

namespace rsf
{
    bool PluginRuntime::start(HMODULE moduleHandle)
    {
        if (workerThread_ != nullptr)
        {
            return true;
        }

        moduleHandle_ = moduleHandle;
        stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (stopEvent_ == nullptr)
        {
            return false;
        }

        workerThread_ = CreateThread(nullptr, 0, &PluginRuntime::ThreadProc, this, 0, nullptr);
        if (workerThread_ == nullptr)
        {
            CloseHandle(stopEvent_);
            stopEvent_ = nullptr;
            return false;
        }

        return true;
    }

    void PluginRuntime::stop()
    {
        if (stopEvent_ != nullptr)
        {
            SetEvent(stopEvent_);
        }

        if (workerThread_ != nullptr)
        {
            WaitForSingleObject(workerThread_, INFINITE);
            CloseHandle(workerThread_);
            workerThread_ = nullptr;
        }

        if (stopEvent_ != nullptr)
        {
            CloseHandle(stopEvent_);
            stopEvent_ = nullptr;
        }
    }

    DWORD WINAPI PluginRuntime::ThreadProc(LPVOID parameter)
    {
        auto* runtime = static_cast<PluginRuntime*>(parameter);
        runtime->run();
        return 0;
    }

    void PluginRuntime::run()
    {
        PluginConfig config = PluginConfig::Load(moduleHandle_);
        Logger::Initialize(moduleHandle_, config.debugLogging);
        RSF_LOG("PluginRuntime thread started.");
        RSF_LOG(
            "Config loaded. UpdateIntervalMs=" + std::to_string(config.updateIntervalMs) +
            " IncognitoMode=" + std::string(config.incognitoMode ? "true" : "false") +
            " DebugLogging=" + std::string(config.debugLogging ? "true" : "false"));

        DiscordManager discord(config);
        if (discord.initialize())
        {
            RSF_LOG("Discord manager initialized.");
        }
        else
        {
            RSF_LOG("Discord manager initialization failed.");
        }

        if (config.incognitoMode)
        {
            discord.update(BuildIncognitoState());

            while (WaitForSingleObject(stopEvent_, 250) == WAIT_TIMEOUT)
            {
                discord.tick();
            }

            discord.shutdown();
            RSF_LOG("PluginRuntime thread stopped.");
            Logger::Shutdown();
            return;
        }

        DataSourceManager manager;
        manager.addSource<RsfContextSource>(config);
        RSF_LOG("RSF context source registered.");

        while (WaitForSingleObject(stopEvent_, 0) == WAIT_TIMEOUT)
        {
            GameData current;
            const bool updated = manager.update(current);

            if (updated)
            {
                ApplyDiscordImages(current);
            }
            else
            {
                current = BuildMenuState();
            }

            discord.tick();
            if (ShouldPublishPresence(current))
            {
                discord.update(current);
            }

            if (WaitForSingleObject(stopEvent_, static_cast<DWORD>(config.updateIntervalMs)) != WAIT_TIMEOUT)
            {
                break;
            }
        }

        discord.shutdown();
        RSF_LOG("PluginRuntime thread stopped.");
        Logger::Shutdown();
    }
}
