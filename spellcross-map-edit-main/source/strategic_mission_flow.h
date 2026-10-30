#pragma once

#include <string>

namespace spellcross_strategy
{
// Original LEVEL_XX.DEF uses Territory(n, intro, base, music).
// The intro token is only the *initial* variant. EndBadMission,
// EndOKMission, ChangeMission and BIG_MAP.SAV can explicitly replace it
// even when the player has not launched any battle on that territory.
//
// The previous implementation unconditionally returned introMission
// whenever launchCount == 0, hiding the mission state transition and
// restarting Time(n) after expiry forever.
inline std::string ResolveActiveMissionToken(
    const std::string& baseMission,
    const std::string& introMission,
    const std::string& currentMission,
    int launchCount)
{
    const auto hasToken = [](const std::string& token)
    {
        return !token.empty() && token != "none";
    };

    // An explicit successor variant overrides the initial briefing token.
    if (hasToken(currentMission) && currentMission != baseMission)
        return currentMission;

    // Existing new-game/legacy saves store baseMission in the current slot
    // until the first battle or scripted mission change.
    if (launchCount <= 0 && hasToken(introMission))
        return introMission;

    return hasToken(currentMission) ? currentMission : baseMission;
}
} // namespace spellcross_strategy
