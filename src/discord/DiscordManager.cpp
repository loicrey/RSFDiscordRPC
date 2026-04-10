#include "pch.h"

#include "DiscordManager.h"

#include "../core/Logger.h"
#include "../core/StringUtils.h"

#include <ctime>
#include <sstream>

namespace
{
    bool IsStableStageGameMode(std::int32_t rawGameMode)
    {
        return rawGameMode == 0x01 || rawGameMode == 0x02;
    }

    bool ShouldSetTimestamp(const rsf::PluginConfig& config, const rsf::GameData& data)
    {
        if (config.incognitoMode)
        {
            return false;
        }

        if (!data.inGame)
        {
            return true;
        }

        if (data.mode == "replay")
        {
            return true;
        }

        return data.mode == "stage" && IsStableStageGameMode(data.rawGameMode);
    }

    std::string BuildDetailsText(const rsf::PluginConfig& config, const rsf::GameData& data)
    {
        if (config.incognitoMode)
        {
            return rsf::kDiscordIncognitoDetails;
        }

        if (data.mode == "replay")
        {
            return "Watching a replay";
        }

        if (!data.inGame)
        {
            return "In the menus";
        }

        return "On stage";
    }
}

namespace rsf
{
    DiscordManager::DiscordManager(const PluginConfig& config)
        : config_(config)
    {
    }

    DiscordManager::~DiscordManager()
    {
        shutdown();
    }

    bool DiscordManager::initialize()
    {
        if (initialized_)
        {
            return true;
        }

        DiscordEventHandlers handlers{};
        Discord_Initialize(kDiscordApplicationId, &handlers, 0, nullptr);
        initialized_ = true;
        lastCallbackTick_ = std::chrono::steady_clock::now();
        RSF_LOG("Discord_Initialize called using hardcoded application id.");
        return true;
    }

    void DiscordManager::shutdown()
    {
        if (initialized_)
        {
            Discord_ClearPresence();
            Discord_Shutdown();
            RSF_LOG("Discord shutdown completed.");
        }

        initialized_ = false;
        lastPresenceHadTimestamp_ = false;
        sessionStartTimestamp_ = 0;
        lastTimestampKey_.clear();
        lastSignature_.clear();
    }

    void DiscordManager::tick()
    {
        if (!initialized_)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - lastCallbackTick_ >= std::chrono::milliseconds(100))
        {
            Discord_RunCallbacks();
            lastCallbackTick_ = now;
        }
    }

    bool DiscordManager::isAvailable() const
    {
        return initialized_;
    }

    void DiscordManager::resetSessionIfNeeded(const GameData& data)
    {
        const std::string timestampKey = buildTimestampKey(data);
        if (!timestampKey.empty() && timestampKey != lastTimestampKey_)
        {
            sessionStartTimestamp_ = static_cast<std::int64_t>(std::time(nullptr));
        }
        else if (timestampKey.empty())
        {
            sessionStartTimestamp_ = 0;
        }

        lastTimestampKey_ = timestampKey;
    }

    std::string DiscordManager::buildTimestampKey(const GameData& data) const
    {
        if (!ShouldSetTimestamp(config_, data))
        {
            return {};
        }

        if (!data.inGame)
        {
            return "menu";
        }

        if (data.mode == "replay")
        {
            return "replay";
        }

        return "stage";
    }

    std::string DiscordManager::buildSignature(const GameData& data) const
    {
        std::ostringstream stream;
        stream << data.mode << '|'
               << data.car << '|'
               << data.stage << '|'
               << data.largeImageKey << '|'
               << data.smallImageKey << '|'
               << data.inGame;
        return stream.str();
    }

    void DiscordManager::update(const GameData& data)
    {
        if (!initialized_)
        {
            return;
        }

        resetSessionIfNeeded(data);

        const std::string signature = buildSignature(data);
        if (signature == lastSignature_)
        {
            tick();
            return;
        }

        const std::string details = strings::Limit(BuildDetailsText(config_, data), 128);

        const std::string largeImageKey =
            !data.largeImageKey.empty() ? data.largeImageKey : kDiscordLargeImageKey;
        const std::string largeImageText = strings::Limit(
            !data.largeImageText.empty() ? data.largeImageText : kDiscordLargeImageText,
            128);
        const std::string smallImageKey = data.smallImageKey;
        const std::string smallImageText = strings::Limit(data.smallImageText, 128);
        const bool shouldSetTimestamp = ShouldSetTimestamp(config_, data);

        if (!shouldSetTimestamp && lastPresenceHadTimestamp_)
        {
            Discord_ClearPresence();
            RSF_LOG("Discord presence cleared to remove stale timestamp.");
        }

        DiscordRichPresence presence{};
        presence.state = nullptr;
        presence.details = details.empty() ? nullptr : details.c_str();
        presence.startTimestamp = shouldSetTimestamp ? sessionStartTimestamp_ : 0;
        presence.largeImageKey = largeImageKey.empty() ? nullptr : largeImageKey.c_str();
        presence.largeImageText = largeImageText.empty() ? nullptr : largeImageText.c_str();
        presence.smallImageKey = smallImageKey.empty() ? nullptr : smallImageKey.c_str();
        presence.smallImageText = smallImageText.empty() ? nullptr : smallImageText.c_str();
        presence.instance = 0;

        Discord_UpdatePresence(&presence);
        lastPresenceHadTimestamp_ = shouldSetTimestamp;
        lastSignature_ = signature;
        RSF_LOG("Discord presence updated. details=" + details);
        tick();
    }
}
