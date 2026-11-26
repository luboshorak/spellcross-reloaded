#pragma once

#include <algorithm>

namespace spellcross_strategy
{
// Engine, weapon and armour may be ordered together, but share one unit
// availability timer: the longest scheduled operation wins.
inline bool HasPendingTechUpgrade(const int (&pendingIds)[3])
{
    return pendingIds[0] >= 0 || pendingIds[1] >= 0 || pendingIds[2] >= 0;
}

inline bool CanStartExclusiveUnitOperation(int cooldown, int pendingKind,
    const int (&pendingIds)[3])
{
    return cooldown <= 0 && pendingKind == 0 && !HasPendingTechUpgrade(pendingIds);
}

inline bool CanQueueTechUpgrade(int cooldown, int pendingKind,
    const int (&pendingIds)[3], int slot)
{
    return pendingKind == 0 && slot >= 0 && slot < 3 && pendingIds[slot] < 0 &&
        (cooldown <= 0 || HasPendingTechUpgrade(pendingIds));
}

inline int ExtendUnitCooldown(int remaining, int operationTurns)
{
    return std::max(remaining, operationTurns);
}

inline int AdvanceUnitCooldown(int remaining)
{
    return remaining > 0 ? remaining - 1 : 0;
}
} // namespace spellcross_strategy
