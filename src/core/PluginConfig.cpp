#include "pch.h"

#include "PluginConfig.h"

#include <array>
#include <string>

namespace
{
    int ReadIniInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int defaultValue)
    {
        return static_cast<int>(GetPrivateProfileIntW(section, key, defaultValue, path.c_str()));
    }

    bool ReadIniBool(const std::wstring& path, const wchar_t* section, const wchar_t* key, bool defaultValue)
    {
        return ReadIniInt(path, section, key, defaultValue ? 1 : 0) != 0;
    }

    int SanitizeUpdateIntervalMs(int updateIntervalMs)
    {
        return updateIntervalMs > 0 ? updateIntervalMs : rsf::kDefaultUpdateIntervalMs;
    }

    std::wstring GetModuleDirectory(HMODULE moduleHandle)
    {
        std::array<wchar_t, MAX_PATH> buffer{};
        GetModuleFileNameW(moduleHandle, buffer.data(), static_cast<DWORD>(buffer.size()));

        std::wstring path(buffer.data());
        const std::size_t lastSlash = path.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos)
        {
            path.resize(lastSlash);
        }

        return path;
    }
}

namespace rsf
{
    PluginConfig PluginConfig::Load(HMODULE moduleHandle)
    {
        PluginConfig config;
        config.moduleDirectory = GetModuleDirectory(moduleHandle);
        config.dataDirectory = config.moduleDirectory + L"\\RSFDiscordRPC";
        CreateDirectoryW(config.dataDirectory.c_str(), nullptr);
        config.configPath = config.dataDirectory + L"\\RSFDiscordRPC.ini";

        const int configuredUpdateIntervalMs =
            ReadIniInt(config.configPath, L"Runtime", L"UpdateIntervalMs", kDefaultUpdateIntervalMs);
        config.updateIntervalMs = SanitizeUpdateIntervalMs(configuredUpdateIntervalMs);
        config.incognitoMode = ReadIniBool(config.configPath, L"Runtime", L"IncognitoMode", false);
        config.debugLogging = ReadIniBool(config.configPath, L"Runtime", L"DebugLogging", false);

        return config;
    }
}
