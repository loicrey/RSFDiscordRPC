#include "pch.h"

#include "RsfContextSource.h"

#include "../core/Logger.h"

#include <array>
#include <cwctype>
#include <fstream>
#include <optional>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <sstream>

namespace
{
    constexpr DWORD kRbrGameConfigPointerAddress = 0x007EAC48;
    constexpr DWORD kRbrGameModeExtPointerAddress = 0x00893634;
    constexpr DWORD kRbrGameModeExt2PointerSourceAddress = 0x007EA678;
    constexpr DWORD kRbrMapSettingsAddress = 0x01660800;
    constexpr DWORD kRbrMapLocationNameAddress = 0x007D1D64;
    constexpr DWORD kRbrGameModeOffset = 0x728;
    constexpr DWORD kRbrGameModeExtTrackIdOffset = 0x14;
    constexpr DWORD kRbrGameModeExtCarIdOffset = 0x18;
    constexpr DWORD kRbrGameModeExt2PointerOffset = 0x70;
    constexpr DWORD kRbrGameModeExt2CarIdOffset = 0x1C;
    constexpr DWORD kRbrGameModeExt2TrackIdOffset = 0x20;
    constexpr DWORD kRbrMapSettingsTrackIdOffset = 0x04;
    constexpr DWORD kRbrMapSettingsCarIdOffset = 0x08;

    struct MemoryStageState
    {
        bool isValid = false;
        bool isOnStage = false;
        std::int32_t gameMode = -1;
        std::int32_t trackId = -1;
        std::int32_t carSlotId = -1;
        std::int32_t mapSettingsCarId = -1;
        std::int32_t gameModeExt2CarId = -1;
        std::int32_t mapSettingsTrackId = -1;
        std::int32_t gameModeExt2TrackId = -1;
        DWORD gameConfigBase = 0;
        DWORD gameModeExtBase = 0;
        DWORD gameModeExt2Base = 0;
        std::string stageName;
    };

    std::wstring ReadIniString(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key)
    {
        wchar_t buffer[256]{};
        GetPrivateProfileStringW(section, key, L"", buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
        return std::wstring(buffer);
    }

    std::int32_t ParseIntField(const rapidjson::Value& value, const char* key)
    {
        if (!value.IsObject() || !value.HasMember(key))
        {
            return -1;
        }

        const rapidjson::Value& field = value[key];
        if (field.IsInt())
        {
            return field.GetInt();
        }

        if (field.IsUint())
        {
            return static_cast<std::int32_t>(field.GetUint());
        }

        if (field.IsString())
        {
            try
            {
                return std::stoi(field.GetString());
            }
            catch (...)
            {
                return -1;
            }
        }

        return -1;
    }

    std::string ParseStringField(const rapidjson::Value& value, const char* key)
    {
        if (!value.IsObject() || !value.HasMember(key) || !value[key].IsString())
        {
            return {};
        }

        return value[key].GetString();
    }

    std::string Trim(const std::string& value)
    {
        std::size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
        {
            ++begin;
        }

        std::size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
            --end;
        }

        return value.substr(begin, end - begin);
    }

    const char* BoolText(bool value)
    {
        return value ? "true" : "false";
    }

    std::string QuoteForLog(const std::string& value)
    {
        return "\"" + value + "\"";
    }

    std::wstring StripQuotes(const std::wstring& value)
    {
        if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"')
        {
            return value.substr(1, value.size() - 2);
        }

        return value;
    }

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (required <= 0)
        {
            return {};
        }

        std::string result(static_cast<std::size_t>(required), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);
        return result;
    }

    std::string DeriveCarNameFromFileName(const std::wstring& fileNameValue)
    {
        const std::wstring normalized = StripQuotes(fileNameValue);
        if (normalized.empty())
        {
            return {};
        }

        const std::filesystem::path filePath(normalized);
        std::string derived = WideToUtf8(filePath.stem().wstring());
        if (derived.empty())
        {
            derived = WideToUtf8(filePath.parent_path().filename().wstring());
        }

        return Trim(derived);
    }

    bool IsReadableMemory(const void* address, std::size_t size)
    {
        if (address == nullptr || size == 0)
        {
            return false;
        }

        const auto begin = reinterpret_cast<std::uintptr_t>(address);
        const auto end = begin + size;
        std::uintptr_t current = begin;

        while (current < end)
        {
            MEMORY_BASIC_INFORMATION memoryInfo{};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(current), &memoryInfo, sizeof(memoryInfo)) == 0)
            {
                return false;
            }

            if (memoryInfo.State != MEM_COMMIT)
            {
                return false;
            }

            if ((memoryInfo.Protect & PAGE_GUARD) != 0 || (memoryInfo.Protect & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            const DWORD access = memoryInfo.Protect & 0xff;
            const bool readable =
                access == PAGE_READONLY ||
                access == PAGE_READWRITE ||
                access == PAGE_WRITECOPY ||
                access == PAGE_EXECUTE_READ ||
                access == PAGE_EXECUTE_READWRITE ||
                access == PAGE_EXECUTE_WRITECOPY;
            if (!readable)
            {
                return false;
            }

            const auto regionBase = reinterpret_cast<std::uintptr_t>(memoryInfo.BaseAddress);
            const auto regionEnd = regionBase + memoryInfo.RegionSize;
            if (regionEnd <= current)
            {
                return false;
            }

            current = regionEnd;
        }

        return true;
    }

    std::string ReadWideStringAt(const wchar_t* address)
    {
        if (!IsReadableMemory(address, sizeof(wchar_t)))
        {
            return {};
        }

        std::array<wchar_t, 256> buffer{};
        for (std::size_t index = 0; index < buffer.size() - 1; ++index)
        {
            if (!IsReadableMemory(address + index, sizeof(wchar_t)))
            {
                break;
            }

            const wchar_t value = address[index];
            buffer[index] = value;
            if (value == L'\0')
            {
                break;
            }
        }

        return Trim(WideToUtf8(buffer.data()));
    }

    std::string ReadAnsiStringAt(const char* address)
    {
        if (!IsReadableMemory(address, sizeof(char)))
        {
            return {};
        }

        std::array<char, 256> buffer{};
        for (std::size_t index = 0; index < buffer.size() - 1; ++index)
        {
            if (!IsReadableMemory(address + index, sizeof(char)))
            {
                break;
            }

            const unsigned char value = static_cast<unsigned char>(address[index]);
            if (value == '\0')
            {
                break;
            }

            if (value < 0x20 || value > 0x7e)
            {
                return {};
            }

            buffer[index] = static_cast<char>(value);
        }

        return Trim(buffer.data());
    }

    std::string ReadTextAtAddress(DWORD address)
    {
        const std::string wideValue = ReadWideStringAt(reinterpret_cast<const wchar_t*>(address));
        if (!wideValue.empty())
        {
            return wideValue;
        }

        return ReadAnsiStringAt(reinterpret_cast<const char*>(address));
    }

    bool IsMemoryOnStageGameMode(std::int32_t gameMode)
    {
        switch (gameMode)
        {
        case 0x01:
        case 0x02:
        case 0x05:
        case 0x08:
        case 0x0A:
            return true;
        default:
            return false;
        }
    }

    std::int32_t SelectStableTrackId(const MemoryStageState& state, std::string& source)
    {
        if (state.mapSettingsTrackId > 0 && state.mapSettingsTrackId == state.gameModeExt2TrackId)
        {
            source = "memory.secondary_track_id";
            return state.mapSettingsTrackId;
        }

        if (state.trackId > 0)
        {
            source = "memory.track_id";
            return state.trackId;
        }

        if (state.gameModeExt2TrackId > 0)
        {
            source = "memory.game_mode_ext2_track_id";
            return state.gameModeExt2TrackId;
        }

        if (state.mapSettingsTrackId > 0)
        {
            source = "memory.map_settings_track_id";
            return state.mapSettingsTrackId;
        }

        return -1;
    }

    std::int32_t SelectStableCarSlotId(const MemoryStageState& state, std::string& source)
    {
        if (state.mapSettingsCarId >= 0 && state.mapSettingsCarId == state.gameModeExt2CarId)
        {
            source = "memory.secondary_car_slot_id";
            return state.mapSettingsCarId;
        }

        if (state.carSlotId >= 0)
        {
            source = "memory.car_slot_id";
            return state.carSlotId;
        }

        if (state.gameModeExt2CarId >= 0)
        {
            source = "memory.game_mode_ext2_car_id";
            return state.gameModeExt2CarId;
        }

        if (state.mapSettingsCarId >= 0)
        {
            source = "memory.map_settings_car_id";
            return state.mapSettingsCarId;
        }

        return -1;
    }

    std::optional<MemoryStageState> TryReadMemoryStageState()
    {
        if (!IsReadableMemory(reinterpret_cast<const void*>(kRbrGameConfigPointerAddress), sizeof(DWORD)))
        {
            return std::nullopt;
        }

        const auto gameConfigBase = *reinterpret_cast<const DWORD*>(kRbrGameConfigPointerAddress);
        if (gameConfigBase == 0 ||
            !IsReadableMemory(reinterpret_cast<const void*>(gameConfigBase + kRbrGameModeOffset), sizeof(std::int32_t)))
        {
            return std::nullopt;
        }

        MemoryStageState result;
        result.isValid = true;
        result.gameConfigBase = gameConfigBase;
        result.gameMode = *reinterpret_cast<const std::int32_t*>(gameConfigBase + kRbrGameModeOffset);
        result.isOnStage = IsMemoryOnStageGameMode(result.gameMode);

        if (IsReadableMemory(reinterpret_cast<const void*>(kRbrGameModeExtPointerAddress), sizeof(DWORD)))
        {
            const auto gameModeExtBase = *reinterpret_cast<const DWORD*>(kRbrGameModeExtPointerAddress);
            if (gameModeExtBase != 0 &&
                IsReadableMemory(reinterpret_cast<const void*>(gameModeExtBase + kRbrGameModeExtTrackIdOffset), sizeof(std::int32_t)))
            {
                result.gameModeExtBase = gameModeExtBase;
                result.trackId = *reinterpret_cast<const std::int32_t*>(gameModeExtBase + kRbrGameModeExtTrackIdOffset);

                if (IsReadableMemory(reinterpret_cast<const void*>(gameModeExtBase + kRbrGameModeExtCarIdOffset), sizeof(std::int32_t)))
                {
                    result.carSlotId = *reinterpret_cast<const std::int32_t*>(gameModeExtBase + kRbrGameModeExtCarIdOffset);
                }
            }
        }

        if (IsReadableMemory(reinterpret_cast<const void*>(kRbrMapSettingsAddress + kRbrMapSettingsTrackIdOffset), sizeof(std::int32_t)))
        {
            result.mapSettingsTrackId = *reinterpret_cast<const std::int32_t*>(kRbrMapSettingsAddress + kRbrMapSettingsTrackIdOffset);
        }

        if (IsReadableMemory(reinterpret_cast<const void*>(kRbrMapSettingsAddress + kRbrMapSettingsCarIdOffset), sizeof(std::int32_t)))
        {
            result.mapSettingsCarId = *reinterpret_cast<const std::int32_t*>(kRbrMapSettingsAddress + kRbrMapSettingsCarIdOffset);
        }

        if (IsReadableMemory(reinterpret_cast<const void*>(kRbrGameModeExt2PointerSourceAddress), sizeof(DWORD)))
        {
            const auto gameModeExt2SourceBase = *reinterpret_cast<const DWORD*>(kRbrGameModeExt2PointerSourceAddress);
            if (gameModeExt2SourceBase != 0 &&
                IsReadableMemory(reinterpret_cast<const void*>(gameModeExt2SourceBase + kRbrGameModeExt2PointerOffset), sizeof(DWORD)))
            {
                const auto gameModeExt2Base = *reinterpret_cast<const DWORD*>(gameModeExt2SourceBase + kRbrGameModeExt2PointerOffset);
                if (gameModeExt2Base != 0 &&
                    IsReadableMemory(reinterpret_cast<const void*>(gameModeExt2Base + kRbrGameModeExt2CarIdOffset), sizeof(std::int32_t)))
                {
                    result.gameModeExt2Base = gameModeExt2Base;
                    result.gameModeExt2CarId = *reinterpret_cast<const std::int32_t*>(gameModeExt2Base + kRbrGameModeExt2CarIdOffset);

                    if (IsReadableMemory(reinterpret_cast<const void*>(gameModeExt2Base + kRbrGameModeExt2TrackIdOffset), sizeof(std::int32_t)))
                    {
                        result.gameModeExt2TrackId = *reinterpret_cast<const std::int32_t*>(gameModeExt2Base + kRbrGameModeExt2TrackIdOffset);
                    }
                }
            }
        }

        result.stageName = ReadTextAtAddress(kRbrMapLocationNameAddress);
        return result;
    }
}

namespace rsf
{
    RsfContextSource::RsfContextSource(const PluginConfig& config)
        : config_(config)
        , gameRoot_(resolveGameRoot())
    {
        RSF_LOG("RsfContextSource created.");
    }

    const char* RsfContextSource::name() const
    {
        return "RsfContext";
    }

    bool RsfContextSource::isAvailable()
    {
        ensureMetadataLoaded();
        return available_;
    }

    bool RsfContextSource::update(GameData& data)
    {
        if (!available_)
        {
            RSF_LOG("Context evaluation skipped. source=RsfContext available=false");
            return false;
        }

        refreshSlotCarMappingsIfNeeded();

        const std::optional<MemoryStageState> memoryState = TryReadMemoryStageState();
        const bool memoryIsValid = memoryState.has_value() && memoryState->isValid;
        const bool memorySaysStage = memoryIsValid && memoryState->isOnStage;

#ifndef NDEBUG
        std::ostringstream evaluation;
        evaluation
            << "Context evaluation. "
            << "memory.valid=" << BoolText(memoryIsValid)
            << " memory.gameConfigPtrAddress=0x" << std::hex << kRbrGameConfigPointerAddress
            << " memory.gameConfigBase=0x" << (memoryState ? memoryState->gameConfigBase : 0)
            << " memory.gameModeOffset=0x" << kRbrGameModeOffset
            << " memory.gameModeExtPtrAddress=0x" << kRbrGameModeExtPointerAddress
            << " memory.gameModeExtBase=0x" << (memoryState ? memoryState->gameModeExtBase : 0)
            << " memory.gameModeExt2PtrSourceAddress=0x" << kRbrGameModeExt2PointerSourceAddress
            << " memory.gameModeExt2Base=0x" << (memoryState ? memoryState->gameModeExt2Base : 0)
            << " memory.mapSettingsAddress=0x" << kRbrMapSettingsAddress
            << " memory.trackIdOffset=0x" << kRbrGameModeExtTrackIdOffset
            << " memory.carSlotIdOffset=0x" << kRbrGameModeExtCarIdOffset
            << " memory.gameModeExt2PtrOffset=0x" << kRbrGameModeExt2PointerOffset
            << " memory.gameModeExt2TrackIdOffset=0x" << kRbrGameModeExt2TrackIdOffset
            << " memory.gameModeExt2CarIdOffset=0x" << kRbrGameModeExt2CarIdOffset
            << " memory.mapSettingsTrackIdOffset=0x" << kRbrMapSettingsTrackIdOffset
            << " memory.mapSettingsCarIdOffset=0x" << kRbrMapSettingsCarIdOffset
            << " memory.stageNameAddress=0x" << kRbrMapLocationNameAddress
            << std::dec
            << " memory.gameMode=" << (memoryState ? std::to_string(memoryState->gameMode) : std::string("<null>"))
            << " memory.onStage=" << BoolText(memorySaysStage)
            << " memory.trackId=" << (memoryState ? std::to_string(memoryState->trackId) : std::string("<null>"))
            << " memory.carSlotId=" << (memoryState ? std::to_string(memoryState->carSlotId) : std::string("<null>"))
            << " memory.mapSettingsTrackId=" << (memoryState ? std::to_string(memoryState->mapSettingsTrackId) : std::string("<null>"))
            << " memory.mapSettingsCarId=" << (memoryState ? std::to_string(memoryState->mapSettingsCarId) : std::string("<null>"))
            << " memory.gameModeExt2TrackId=" << (memoryState ? std::to_string(memoryState->gameModeExt2TrackId) : std::string("<null>"))
            << " memory.gameModeExt2CarId=" << (memoryState ? std::to_string(memoryState->gameModeExt2CarId) : std::string("<null>"))
            << " memory.stage=" << QuoteForLog(memoryState ? memoryState->stageName : std::string());
        RSF_LOG(evaluation.str());
#endif

        data.inGame = false;
        data.mode = "menu";
        data.stage.clear();
        data.car.clear();
        data.carId = -1;
        data.rsfCarId = -1;
        data.trackId = -1;
        data.rawGameMode = memoryState ? memoryState->gameMode : -1;
        data.mapSettingsCarId = memoryState ? memoryState->mapSettingsCarId : -1;
        data.gameModeExt2CarId = memoryState ? memoryState->gameModeExt2CarId : -1;
        data.mapSettingsTrackId = memoryState ? memoryState->mapSettingsTrackId : -1;
        data.gameModeExt2TrackId = memoryState ? memoryState->gameModeExt2TrackId : -1;

        if (!memorySaysStage)
        {
            RSF_LOG("Context decision. state=menu reason=" + std::string(memoryIsValid ? "memory_game_mode" : "memory_unavailable"));
            return true;
        }

        data.inGame = true;
        data.mode = data.rawGameMode == 8 ? "replay" : "stage";

        std::string trackIdSource = "unknown";
        std::string stageSource = "unknown";
        std::string carSlotSource = "unknown";
        std::string carNameSource = "unknown";

        data.trackId = SelectStableTrackId(*memoryState, trackIdSource);
        data.carId = SelectStableCarSlotId(*memoryState, carSlotSource);

        if (!memoryState->stageName.empty())
        {
            data.stage = memoryState->stageName;
            stageSource = "memory.stage_name";
        }
        else if (data.trackId >= 0)
        {
            data.stage = resolveStageNameByRbrId(data.trackId);
            if (!data.stage.empty())
            {
                stageSource = "stages_data_json_by_track_id";
            }
        }

        if (data.carId >= 0)
        {
            const SlotCarEntry slotCar = resolveCarBySlot(data.carId);
            data.car = slotCar.name;
            data.rsfCarId = slotCar.rsfCarId;
            carNameSource = slotCar.source.empty() ? "unknown" : slotCar.source;
        }

        if (data.mode == "replay")
        {
            data.stage.clear();
            data.car.clear();
            data.trackId = -1;
            data.carId = -1;
            data.rsfCarId = -1;
            stageSource = "replay_hidden";
            carNameSource = "replay_hidden";
        }

#ifndef NDEBUG
        std::ostringstream decision;
        decision
            << "Context decision. state=" << data.mode
            << " reason=memory_game_mode"
            << " trackId=" << data.trackId
            << " mapSettingsTrackId=" << data.mapSettingsTrackId
            << " gameModeExt2TrackId=" << data.gameModeExt2TrackId
            << " trackIdSource=" << trackIdSource
            << " stage=" << QuoteForLog(data.stage)
            << " stageSource=" << stageSource
            << " carId=" << data.carId
            << " rsfCarId=" << data.rsfCarId
            << " mapSettingsCarId=" << data.mapSettingsCarId
            << " gameModeExt2CarId=" << data.gameModeExt2CarId
            << " carSlotSource=" << carSlotSource
            << " car=" << QuoteForLog(data.car)
            << " carNameSource=" << carNameSource;
        RSF_LOG(decision.str());
#endif

        return true;
    }

    std::filesystem::path RsfContextSource::resolveGameRoot() const
    {
        std::filesystem::path moduleDirectory(config_.moduleDirectory);
        if (!moduleDirectory.empty())
        {
            std::wstring lowered = moduleDirectory.wstring();
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t ch) {
                return static_cast<wchar_t>(::towlower(ch));
            });

            if (lowered.find(L"\\plugins") != std::wstring::npos)
            {
                return moduleDirectory.parent_path();
            }
        }

        return moduleDirectory.parent_path();
    }

    void RsfContextSource::ensureMetadataLoaded()
    {
        if (metadataLoaded_)
        {
            return;
        }

        metadataLoaded_ = true;
        available_ = false;

        if (gameRoot_.empty())
        {
            RSF_LOG("RsfContextSource disabled because the game root could not be resolved.");
            return;
        }

        const std::filesystem::path rsfCache = gameRoot_ / "rsfdata" / "cache";
        if (!std::filesystem::exists(rsfCache))
        {
            RSF_LOG("RsfContextSource disabled because rsfdata\\cache was not found.");
            return;
        }

        loadStageMappings(gameRoot_);
        loadCarMappings(gameRoot_);
        loadSlotCarMappings(gameRoot_);
        available_ = true;

        RSF_LOG(
            "RsfContextSource metadata loaded. stages=" + std::to_string(stagesByRbrId_.size()) +
            " cars=" + std::to_string(carsByRsfId_.size()));
    }

    void RsfContextSource::refreshSlotCarMappingsIfNeeded()
    {
        const std::filesystem::path carsIniPath = gameRoot_ / "Cars" / "Cars.ini";
        std::error_code error;
        const bool exists = std::filesystem::exists(carsIniPath, error);
        if (error || !exists)
        {
            return;
        }

        const auto currentWriteTime = std::filesystem::last_write_time(carsIniPath, error);
        if (error)
        {
            return;
        }

        if (slotCarMappingsLoaded_ && currentWriteTime == slotCarsIniWriteTime_)
        {
            return;
        }

        loadSlotCarMappings(gameRoot_);
    }

    void RsfContextSource::loadStageMappings(const std::filesystem::path& gameRoot)
    {
        const std::filesystem::path path = gameRoot / "rsfdata" / "cache" / "stages_data.json";
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            RSF_LOG("RsfContextSource could not open stages_data.json.");
            return;
        }

        rapidjson::IStreamWrapper streamWrapper(stream);
        rapidjson::Document document;
        document.ParseStream(streamWrapper);
        if (document.HasParseError() || !document.IsArray())
        {
            RSF_LOG("RsfContextSource could not parse stages_data.json.");
            return;
        }

        stagesByRbrId_.reserve(document.Size());
        for (const auto& entryValue : document.GetArray())
        {
            const std::int32_t rbrStageId = ParseIntField(entryValue, "stage_id");
            const std::string stageName = ParseStringField(entryValue, "name");
            if (rbrStageId < 0 || stageName.empty())
            {
                continue;
            }

            stagesByRbrId_.emplace(rbrStageId, StageEntry{ stageName });
        }
    }

    void RsfContextSource::loadCarMappings(const std::filesystem::path& gameRoot)
    {
        const std::filesystem::path path = gameRoot / "rsfdata" / "cache" / "cars.json";
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
        {
            RSF_LOG("RsfContextSource could not open cars.json.");
            return;
        }

        rapidjson::IStreamWrapper streamWrapper(stream);
        rapidjson::Document document;
        document.ParseStream(streamWrapper);
        if (document.HasParseError() || !document.IsArray())
        {
            RSF_LOG("RsfContextSource could not parse cars.json.");
            return;
        }

        carsByRsfId_.reserve(document.Size());
        for (const auto& entryValue : document.GetArray())
        {
            const std::int32_t rsfCarId = ParseIntField(entryValue, "id");
            const std::string carName = ParseStringField(entryValue, "name");
            if (rsfCarId < 0 || carName.empty())
            {
                continue;
            }

            carsByRsfId_.emplace(rsfCarId, CarEntry{ carName });
        }
    }

    void RsfContextSource::loadSlotCarMappings(const std::filesystem::path& gameRoot)
    {
        slotCarsBySlotId_.fill(SlotCarEntry{});

        const std::filesystem::path carsIniPath = gameRoot / "Cars" / "Cars.ini";
        if (!std::filesystem::exists(carsIniPath))
        {
            slotCarMappingsLoaded_ = false;
            RSF_LOG("RsfContextSource could not open Cars\\Cars.ini.");
            return;
        }

        for (std::int32_t carSlotId = 0; carSlotId < static_cast<std::int32_t>(slotCarsBySlotId_.size()); ++carSlotId)
        {
            SlotCarEntry entry;

            wchar_t sectionName[16]{};
            swprintf_s(sectionName, L"Car%02d", carSlotId);

            entry.rsfCarId = _wtoi(ReadIniString(carsIniPath, sectionName, L"RSFCarID").c_str());
            if (entry.rsfCarId >= 0)
            {
                if (const auto iterator = carsByRsfId_.find(entry.rsfCarId); iterator != carsByRsfId_.end())
                {
                    entry.name = iterator->second.name;
                    entry.source = "cars_json_by_rsf_id";
                    slotCarsBySlotId_[static_cast<std::size_t>(carSlotId)] = std::move(entry);
                    continue;
                }
            }

            entry.name = Trim(WideToUtf8(ReadIniString(carsIniPath, sectionName, L"CarName")));
            if (!entry.name.empty())
            {
                entry.source = "cars_ini_car_name";
                slotCarsBySlotId_[static_cast<std::size_t>(carSlotId)] = std::move(entry);
                continue;
            }

            entry.name = DeriveCarNameFromFileName(ReadIniString(carsIniPath, sectionName, L"FileName"));
            if (!entry.name.empty())
            {
                entry.source = "cars_ini_file_name";
            }

            slotCarsBySlotId_[static_cast<std::size_t>(carSlotId)] = std::move(entry);
        }

        std::error_code error;
        slotCarsIniWriteTime_ = std::filesystem::last_write_time(carsIniPath, error);
        slotCarMappingsLoaded_ = !error;
        RSF_LOG(
            slotCarMappingsLoaded_
                ? "RsfContextSource slot car mappings reloaded from Cars\\Cars.ini."
                : "RsfContextSource slot car mappings reloaded, but Cars\\Cars.ini write time could not be read.");
    }

    std::string RsfContextSource::resolveStageNameByRbrId(std::int32_t rbrStageId) const
    {
        if (const auto iterator = stagesByRbrId_.find(rbrStageId); iterator != stagesByRbrId_.end())
        {
            return iterator->second.name;
        }

        return {};
    }

    RsfContextSource::SlotCarEntry RsfContextSource::resolveCarBySlot(std::int32_t carSlotId) const
    {
        if (carSlotId < 0 || carSlotId >= static_cast<std::int32_t>(slotCarsBySlotId_.size()))
        {
            return {};
        }

        return slotCarsBySlotId_[static_cast<std::size_t>(carSlotId)];
    }
}
