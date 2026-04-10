#pragma once

#include "../core/GameData.h"
#include "../core/PluginConfig.h"
#include <discord_rpc.h>

#include <chrono>
#include <string>

namespace rsf
{
    class DiscordManager
    {
    public:
        explicit DiscordManager(const PluginConfig& config);
        ~DiscordManager();

        bool initialize();
        void shutdown();
        void tick();
        void update(const GameData& data);

        bool isAvailable() const;

    private:
        std::string buildSignature(const GameData& data) const;
        void resetSessionIfNeeded(const GameData& data);
        std::string buildTimestampKey(const GameData& data) const;

        PluginConfig config_;

        bool initialized_ = false;
        bool lastPresenceHadTimestamp_ = false;
        std::int64_t sessionStartTimestamp_ = 0;
        std::string lastTimestampKey_;
        std::string lastSignature_;
        std::chrono::steady_clock::time_point lastCallbackTick_{};
    };
}
