#pragma once
#include <filesystem>
#include <string>

namespace spellcross_demo
{
    inline bool& ActiveProfile() { static bool demo = false; return demo; }
    inline std::filesystem::path RuntimeRoot()
    {
        auto root = std::filesystem::current_path();
        return ActiveProfile() ? root / "demo_profile" : root;
    }
    inline bool RequiredArchive(bool optional, const std::string& filename, bool demo)
    {
        return !optional && !(demo && (filename == "PUST.FS" || filename == "DEVAST.FS"));
    }
}
