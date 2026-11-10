//=============================================================================
// Spellcross restoration - original strategic random counter-attack rules.
// Retail reverse engineering: SPELCROS.EXE 0x4C440 (chance) and
// 0x2EC8E (frontline mission frequency).
// Keep these pure rules free of wxWidgets for Linux/Windows CI.
//=============================================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

namespace spellcross_strategy
{
struct CounterAttackFrontier
{
    // (player territory, neighbouring enemy territory), distinct and sorted.
    std::vector<std::pair<int, int>> borders;
    std::vector<int> player_targets;
    std::vector<int> enemy_sources;
};

// LEVEL_XX.CLK stores a territory index, optionally with its high-bit marker.
inline int StrategicClkTerritory(uint8_t raw)
{
    return raw >= 129 ? static_cast<int>(raw) - 128 : static_cast<int>(raw);
}

inline CounterAttackFrontier FindCounterAttackFrontier(
    const std::vector<uint8_t>& clk, int width, int height,
    const std::vector<int>& playerOwned)
{
    CounterAttackFrontier result;
    if (width <= 0 || height <= 0 ||
        static_cast<uint64_t>(width) * static_cast<uint64_t>(height) != clk.size())
        return result;

    const std::unordered_set<int> owned(playerOwned.begin(), playerOwned.end());
    std::set<std::pair<int, int>> borders;
    auto territory = [&](int x, int y) {
        return StrategicClkTerritory(clk[static_cast<size_t>(y) *
            static_cast<size_t>(width) + static_cast<size_t>(x)]);
    };
    auto include = [&](int a, int b) {
        if (a <= 0 || b <= 0 || a == b)
            return;
        if (owned.count(a) != 0 && owned.count(b) == 0)
            borders.emplace(a, b);
        if (owned.count(b) != 0 && owned.count(a) == 0)
            borders.emplace(b, a);
    };

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const int a = territory(x, y);
            if (x + 1 < width) include(a, territory(x + 1, y));
            if (y + 1 < height) include(a, territory(x, y + 1));
        }

    std::set<int> targets, sources;
    result.borders.assign(borders.begin(), borders.end());
    for (const auto& edge : borders)
    {
        targets.insert(edge.first);
        sources.insert(edge.second);
    }
    result.player_targets.assign(targets.begin(), targets.end());
    result.enemy_sources.assign(sources.begin(), sources.end());
    return result;
}

// Retail 0x4C440: wait B turns, then test random(100) against
// (currentTurn - lastAttackTurn - B - 1) * A. A is a percentage-point
// increment, NOT a constant per-turn probability.
inline int RandomCounterAttackChance(int currentTurn, int lastAttackTurn,
    int rate, int minimumGap)
{
    if (rate <= 0 || minimumGap < 0 || currentTurn <= lastAttackTurn)
        return 0;
    const int64_t elapsed =
        static_cast<int64_t>(currentTurn) - static_cast<int64_t>(lastAttackTurn);
    if (elapsed <= minimumGap)
        return 0;
    const int64_t chance =
        (elapsed - static_cast<int64_t>(minimumGap) - 1) * rate;
    return static_cast<int>(std::clamp<int64_t>(chance, 0, 100));
}

inline bool RandomCounterAttackRoll(int currentTurn, int lastAttackTurn,
    int rate, int minimumGap, int random0to99)
{
    return random0to99 >= 0 && random0to99 < 100 &&
        random0to99 < RandomCounterAttackChance(
            currentTurn, lastAttackTurn, rate, minimumGap);
}

// Existing generated-DTA loader accepts concrete type IDs; keep the force
// in CounterAttackState so decline/loss/recapture use the SAME units.
// Retail uses AttackFlags[0..1] for force size and chooses at most one
// special company. The special-selection weighting remains approximate.
template<class RandomInt>
inline std::vector<int> BuildRandomCounterAttackForce(
    const std::vector<int>& regularTypes,
    const std::vector<int>& specialTypes,
    const std::vector<int>& attackFlags, RandomInt randomBelow)
{
    std::vector<int> force;
    if (regularTypes.empty() || attackFlags.size() < 2)
        return force;

    const int lower = std::clamp(attackFlags[0], 1, 256);
    const int upper = std::clamp(attackFlags[1], lower, 256);
    const int count = lower + randomBelow(upper - lower + 1);
    force.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        force.push_back(regularTypes[static_cast<size_t>(
            randomBelow(static_cast<int>(regularTypes.size())))]);

    // One special replaces a regular company; this preserves the count
    // specified by the retail AttackFlags bounds.
    if (!specialTypes.empty() && randomBelow(2) == 0)
    {
        const int position = randomBelow(count);
        force[static_cast<size_t>(position)] = specialTypes[static_cast<size_t>(
            randomBelow(static_cast<int>(specialTypes.size())))];
    }
    return force;
}
} // namespace spellcross_strategy
