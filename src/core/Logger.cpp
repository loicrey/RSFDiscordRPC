#include "pch.h"

#include "Logger.h"

#include <array>
#include <cstdio>
#include <mutex>

namespace
{
    std::mutex g_logMutex;
    constexpr const wchar_t* kPluginDataFolderName = L"RSFDiscordRPC";

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
    bool Logger::enabled_ = false;
    std::wstring Logger::logPath_;

    void Logger::Initialize(HMODULE moduleHandle, bool enabled)
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        enabled_ = enabled;
        if (!enabled_)
        {
            logPath_.clear();
            return;
        }

        if (!logPath_.empty())
        {
            return;
        }

        const std::wstring moduleDirectory = GetModuleDirectory(moduleHandle);
        const std::wstring dataDirectory = moduleDirectory + L"\\" + kPluginDataFolderName;
        CreateDirectoryW(dataDirectory.c_str(), nullptr);
        logPath_ = dataDirectory + L"\\RSFDiscordRPC.log";
    }

    bool Logger::IsEnabled()
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        return enabled_;
    }

    void Logger::Write(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (!enabled_ || logPath_.empty())
        {
            return;
        }

        SYSTEMTIME localTime{};
        GetLocalTime(&localTime);

        char buffer[2048]{};
        const int count = sprintf_s(
            buffer,
            "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\r\n",
            localTime.wYear,
            localTime.wMonth,
            localTime.wDay,
            localTime.wHour,
            localTime.wMinute,
            localTime.wSecond,
            localTime.wMilliseconds,
            message.c_str());

        if (count <= 0)
        {
            return;
        }

        HANDLE file = CreateFileW(
            logPath_.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            return;
        }

        DWORD written = 0;
        WriteFile(file, buffer, static_cast<DWORD>(count), &written, nullptr);
        CloseHandle(file);
    }

    void Logger::Shutdown()
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        enabled_ = false;
        logPath_.clear();
    }
}
