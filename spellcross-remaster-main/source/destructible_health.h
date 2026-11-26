#pragma once

#include <algorithm>
#include <cstdint>

namespace spellcross_tactical
{
    // A positive HP value must never be presented as 0% (integer truncation
    // previously made 1/140 HP or 1/240 HP appear destroyed).
    inline int DestructibleHealthPercent(int currentHp, int maximumHp)
    {
        if (currentHp <= 0 || maximumHp <= 0)
            return 0;
        const std::int64_t percent = 100LL * currentHp / maximumHp;
        return static_cast<int>((std::clamp)(percent, std::int64_t{1}, std::int64_t{100}));
    }

    inline bool DestructibleIsDead(int hp)
    {
        return hp <= 0;
    }

    // A one-stage destroyed sprite has an alternate pointer back to the
    // *intact* sprite. Reapplying destruction must not resurrect a gate or
    // bridge when two shots finish their HIT/UPDATE animations out of order.
    // Two-stage walls are different: their damaged sprite must still be
    // removed completely on reaching zero HP.
    inline bool ShouldFinishDestruction(int hp, bool twoStage, bool alreadyDestroyed)
    {
        return DestructibleIsDead(hp) && (twoStage || !alreadyDestroyed);
    }

    inline int ApplyDestructibleDamage(int hp, int hit)
    {
        if (hp <= 0)
            return 0;
        if (hit <= 0)
            return hp;
        return hit >= hp ? 0 : hp - hit;
    }
}
