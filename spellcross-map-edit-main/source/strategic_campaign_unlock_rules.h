#pragma once

namespace spellcross_strategy
{
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
