#pragma once

// Original RESEARCH.DEF and LEVEL_XX.DEF use two different kinds of flags:
//  * ORconnections: at least ONE completed project or scripted Special flag
//  * WaitForResearch: ALL listed research projects must be completed
// Keep those semantics separate and restore flags from persisted event IDs.
#include <algorithm>
#include <set>
#include <unordered_set>
#include <vector>

namespace spellcross_strategy
{
template <typename Events, typename TriggeredEventIds>
inline std::set<int> EffectiveResearchFlags(
    const std::vector<int>& initialFlags,
    const Events& events,
    const TriggeredEventIds& triggered)
{
    std::set<int> flags(initialFlags.begin(), initialFlags.end());
    for (const auto& evt : events)
    {
        if (triggered.count(evt.id) == 0)
            continue;
        flags.insert(evt.research_flags.begin(), evt.research_flags.end());
    }
    return flags;
}

inline bool AnyResearchPrerequisiteUnlocked(
    const std::vector<int>& prerequisites,
    const std::unordered_set<int>& completed,
    const std::set<int>& flags)
{
    if (prerequisites.empty())
        return true;
    return std::any_of(prerequisites.begin(), prerequisites.end(), [&](int id) {
        return completed.count(id) != 0 || flags.count(id) != 0;
    });
}

inline bool AllResearchCompleted(
    const std::vector<int>& prerequisites,
    const std::unordered_set<int>& completed)
{
    return std::all_of(prerequisites.begin(), prerequisites.end(), [&](int id) {
        return completed.count(id) != 0;
    });
}

template <typename Events>
inline bool FinalTerritoryResearchGateSatisfied(
    const Events& events,
    const std::unordered_set<int>& completed)
{
    // Only events that open a territory after research guard access to the
    // chapter finale; unrelated research events must not lock the map.
    for (const auto& evt : events)
    {
        if (evt.set_player_territory > 0 && !evt.wait_for_research.empty() &&
            !AllResearchCompleted(evt.wait_for_research, completed))
            return false;
    }
    return true;
}
} // namespace spellcross_strategy
