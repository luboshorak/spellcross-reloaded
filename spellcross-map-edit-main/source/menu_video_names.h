#pragma once

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
}
