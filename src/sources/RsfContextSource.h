#pragma once

#include "../core/PluginConfig.h"
#include "IDataSource.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace rsf
{
    class RsfContextSource final : public IDataSource
    {
    public:
        explicit RsfContextSource(const PluginConfig& config);

        const char* name() const override;
        bool isAvailable() override;
        bool update(GameData& data) override;

    private:
        struct StageEntry
        {
            std::string name;
        };

        struct CarEntry
        {
            std::string name;
        };

        struct SlotCarEntry
        {
            std::string name;
            std::string source;
            std::int32_t rsfCarId = -1;
        };

        static constexpr std::size_t kCarSlotCount = 8;

        std::filesystem::path resolveGameRoot() const;
        void ensureMetadataLoaded();
        void refreshSlotCarMappingsIfNeeded();
        void loadStageMappings(const std::filesystem::path& gameRoot);
        void loadCarMappings(const std::filesystem::path& gameRoot);
        void loadSlotCarMappings(const std::filesystem::path& gameRoot);
        std::string resolveStageNameByRbrId(std::int32_t rbrStageId) const;
        SlotCarEntry resolveCarBySlot(std::int32_t carSlotId) const;

        PluginConfig config_;
        std::filesystem::path gameRoot_;
        bool metadataLoaded_ = false;
        bool available_ = false;
        bool slotCarMappingsLoaded_ = false;
        std::filesystem::file_time_type slotCarsIniWriteTime_{};
        std::unordered_map<std::int32_t, StageEntry> stagesByRbrId_;
        std::unordered_map<std::int32_t, CarEntry> carsByRsfId_;
        std::array<SlotCarEntry, kCarSlotCount> slotCarsBySlotId_{};
    };
}
