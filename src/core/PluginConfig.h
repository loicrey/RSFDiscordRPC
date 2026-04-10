#pragma once

#include <cstdint>
#include <string>

namespace rsf
{
    inline constexpr char kDiscordApplicationId[] = "1246151506843926598";
    inline constexpr char kDiscordLargeImageKey[] = "rbr_logo";
    inline constexpr char kDiscordLargeImageText[] = "Richard Burns Rally";
    inline constexpr char kDiscordIncognitoDetails[] = "Richard Burns Rally";
    inline constexpr int kDefaultUpdateIntervalMs = 5000;

    struct PluginConfig
    {
        int updateIntervalMs = kDefaultUpdateIntervalMs;
        bool incognitoMode = false;
        bool debugLogging = false;

        std::wstring moduleDirectory;
        std::wstring dataDirectory;
        std::wstring configPath;

        static PluginConfig Load(HMODULE moduleHandle);
    };
}
