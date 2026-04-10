#pragma once

#include <string>

#define RSF_LOG(message) do { if (::rsf::Logger::IsEnabled()) ::rsf::Logger::Write((message)); } while (0)

namespace rsf
{
    class Logger
    {
    public:
        static void Initialize(HMODULE moduleHandle, bool enabled);
        static bool IsEnabled();
        static void Write(const std::string& message);
        static void Shutdown();

    private:
        static bool enabled_;
        static std::wstring logPath_;
    };
}
