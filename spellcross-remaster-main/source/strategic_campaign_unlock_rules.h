#pragma once

#include <regex>
#include <set>
#include <sstream>
#include <string>

namespace spellcross_strategy
{
// Read SetResearchFlag(N) commands, ignoring comments in LEVEL_XX.DEF.
// A commented-out unlock must not count as an actual campaign unlock.
inline std::set<int> ParseCampaignUnlockFlags(const std::string& script)
{
    static const std::regex re(R"(SetResearchFlag\s*\(\s*(\d+)\s*\))");
    std::set<int> flags;
    std::istringstream input(script);
    std::string line;
    while (std::getline(input, line))
    {
        const size_t comment = line.find(';');
        if (comment != std::string::npos) line.erase(comment);
        for (auto it = std::sregex_iterator(line.begin(), line.end(), re);
            it != std::sregex_iterator(); ++it)
            flags.insert(std::stoi((*it)[1].str()));
    }
    return flags;
}

// A retail RESEARCH.DEF UnitType record with Time(0), Level(0) and no
// prerequisites is a starting unit ONLY when no retail campaign chapter
// introduces that type later with SetResearchFlag(type).
//
// Requiring the complete chapter inventory is important: missing script
// files must not accidentally make advanced equipment available early.
inline bool IsUnscriptedBaseUnit(
    bool researchable,
    int researchLevel,
    bool hasPrerequisites,
    bool campaignScriptsComplete,
    bool hasScriptedUnlock)
{
    return campaignScriptsComplete
        && !researchable
        && researchLevel == 0
        && !hasPrerequisites
        && !hasScriptedUnlock;
}
} // namespace spellcross_strategy
