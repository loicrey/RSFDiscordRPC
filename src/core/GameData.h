#pragma once

#include <cstdint>
#include <string>

namespace rsf
{
    struct GameData
    {
        bool inGame = false;
        std::string car;
        std::string stage;
        std::string mode = "menu";

        std::string largeImageKey;
        std::string largeImageText;
        std::string smallImageKey;
        std::string smallImageText;

        std::int32_t carId = -1;
        std::int32_t rsfCarId = -1;
        std::int32_t trackId = -1;
        std::int32_t rawGameMode = -1;
        std::int32_t mapSettingsCarId = -1;
        std::int32_t gameModeExt2CarId = -1;
        std::int32_t mapSettingsTrackId = -1;
        std::int32_t gameModeExt2TrackId = -1;
    };
}
