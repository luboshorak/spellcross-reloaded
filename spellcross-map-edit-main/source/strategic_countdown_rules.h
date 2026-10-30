#pragma once
// Original Spellcross BIG_MAP.SAV stores a remaining Time(n) value even for
// inaccessible territories. The countdown must not advance while the
// strategic objective is unavailable, unlike a global absolute-turn timer.
#include <limits>

namespace spellcross_strategy
{
inline int PauseDeadlineForInactiveTurn(int absoluteDeadline, bool accessible)
{
    if (absoluteDeadline > 0 && !accessible &&
        absoluteDeadline < std::numeric_limits<int>::max())
        return absoluteDeadline + 1;
    return absoluteDeadline;
}
}
