#pragma once

#include <algorithm>
#include <cstdint>

namespace spellcross_night
{
    using Palette = std::uint8_t[256][3];

    // Restore the DOS NightMission green-only palette. Use the intended
    // maximum of all three channels without the original routine's stray read.
    inline const std::uint8_t (*SelectPalette(
        const Palette& source, Palette& night, bool is_night))[3]
    {
        if (!is_night)
            return source;

        for (int i = 0; i < 256; ++i)
        {
            const auto green = (std::max)({source[i][0], source[i][1], source[i][2]});
            night[i][0] = 0;
            night[i][1] = green;
            night[i][2] = 0;
        }
        return night;
    }
}
