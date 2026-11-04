#pragma once

#include <string>
#include <algorithm>
#include <cctype>

namespace spellcross_strategy
{
inline bool IsGeneratedMissionToken(const std::string& token)
{
    return token.size() == 5 && (token[0] == 'r' || token[0] == 'R') &&
        std::all_of(token.begin() + 1, token.end(),
            [](unsigned char c) { return std::isdigit(c) != 0; });
}

// DOS generated R0410.MIS from the M04_10.DTA terrain. Never use the
// terrain's initial A/B story mission as the identity of an Army battle.
inline std::string GeneratedMissionToken(const std::string& terrainToken)
{
    std::string token = terrainToken;
    std::transform(token.begin(), token.end(), token.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (IsGeneratedMissionToken(token))
        return token;
    if (token.size() >= 6 && token[0] == 'm' && token[3] == '_' &&
        std::isdigit(static_cast<unsigned char>(token[1])) &&
        std::isdigit(static_cast<unsigned char>(token[2])) &&
        std::isdigit(static_cast<unsigned char>(token[4])) &&
        std::isdigit(static_cast<unsigned char>(token[5])))
        return "r" + token.substr(1, 2) + token.substr(4, 2);
    return {};
}

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
// Use the same battle identity for briefing, launch, deadlines and results.
// AttackRange is the saved CounterAttackState collection (also usable by CI
// without wxWidgets or commercial assets).
template<class AttackRange>
inline std::string ResolveTerritoryMissionToken(
    const std::string& baseMission, const std::string& introMission,
    const std::string& currentMission, int launchCount, int territoryId,
    const AttackRange& attacks)
{
    const auto lower = [](std::string token) {
        std::transform(token.begin(), token.end(), token.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return token;
    };
    for (auto attack = attacks.rbegin(); attack != attacks.rend(); ++attack)
    {
        if (attack->territory_id != territoryId ||
            !((attack->triggered && !attack->completed) || attack->territory_lost))
            continue;
        const std::string counter = lower(attack->counter_mission);
        const bool generated = !attack->enemy_units.empty() || attack->territory_lost ||
            IsGeneratedMissionToken(counter) || counter == lower(baseMission) ||
            counter == lower(introMission);
        const std::string token = GeneratedMissionToken(baseMission);
        if (generated && !token.empty())
            return token;
        if (!counter.empty() && counter != "none")
            return counter; // explicitly authored legacy defence mission
    }
    return ResolveActiveMissionToken(baseMission, introMission, currentMission, launchCount);
}
} // namespace spellcross_strategy
