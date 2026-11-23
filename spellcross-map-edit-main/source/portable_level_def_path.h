#pragma once

// Resolve chapter identity from a save without using the author's installation
// directory. std::filesystem::path does not split Windows backslashes on Linux.
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace spellcross_save
{
    inline std::string PortableFileName(std::string_view source)
    {
        const auto sep = source.find_last_of("/\\");
        return std::string(source.substr(sep == std::string_view::npos ? 0 : sep + 1));
    }

    inline std::string Upper(std::string value)
    {
        for (char& ch : value)
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return value;
    }

    inline std::string PortableStem(std::string_view path)
    {
        std::string name = PortableFileName(path);
        const auto dot = name.find_last_of('.');
        if (dot != std::string::npos)
            name.resize(dot);
        return Upper(std::move(name));
    }

    inline std::string CanonicalLevelDefName(std::string_view storedPath)
    {
        std::string name = Upper(PortableFileName(storedPath));
        if (name.size() != 12 || name.compare(0, 6, "LEVEL_") != 0 ||
            !std::isdigit(static_cast<unsigned char>(name[6])) ||
            !std::isdigit(static_cast<unsigned char>(name[7])) ||
            name.compare(8, 4, ".DEF") != 0)
            return {};
        return name;
    }

    inline std::string LevelDefFromMissionToken(std::string_view missionPath)
    {
        const std::string token = PortableStem(missionPath);
        // Scripted M03_03/M03_03A and generated R0303 both belong to chapter 03.
        const bool scripted = token.size() >= 6 && token[0] == 'M' && token[3] == '_' &&
            std::isdigit(static_cast<unsigned char>(token[4])) &&
            std::isdigit(static_cast<unsigned char>(token[5]));
        const bool generated = token.size() == 5 && token[0] == 'R' &&
            std::isdigit(static_cast<unsigned char>(token[3])) &&
            std::isdigit(static_cast<unsigned char>(token[4]));
        if ((!scripted && !generated) ||
            !std::isdigit(static_cast<unsigned char>(token[1])) ||
            !std::isdigit(static_cast<unsigned char>(token[2])))
            return {};
        return "LEVEL_" + token.substr(1, 2) + ".DEF";
    }

    inline std::string ExpectedLevelDefName(std::string_view savedPath,
        std::string_view missionToken)
    {
        const std::string savedName = CanonicalLevelDefName(savedPath);
        return !savedName.empty() ? savedName : LevelDefFromMissionToken(missionToken);
    }
}
