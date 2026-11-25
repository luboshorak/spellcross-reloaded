#pragma once
#include <array>

namespace spellcross_menu_video
{
    // Names referenced by the original Czech and English executables.
    // Other retail editions use the SMK-first resolver, with legacy aliases.
    constexpr const char* Intro(bool czech)
    {
        return czech ? "INTRO.CAN" : "INTRO.SMK";
    }

    constexpr const char* Credits(bool czech)
    {
        return czech ? "CREDITS.DPK" : "CREDITS.SMK";
    }

    // Retail CZ 0x110CA..0x11113; EN 0x11385..0x113CE.
    // Both TITAN films are the victory ending, not alternative endings.
    constexpr std::array<const char*, 5> Victory(bool czech)
    {
        return {czech ? "TITAN1.CAN" : "TITAN1.SMK",
                czech ? "TITAN2.CAN" : "TITAN2.SMK",
                "S_10.DP2",
                czech ? "LOGO.CAN" : "LOGO.SMK",
                Credits(czech)};
    }

    constexpr const char* Defeat()
    {
        return "S_OVER.DP2";
    }
}
