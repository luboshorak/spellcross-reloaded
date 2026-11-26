#pragma once
#include <algorithm>

// Original EN SPELCROS.EXE action 13, 0x2CD98..0x2D051.
// Explicit attack values bypass the ordinary weapon's morale/status accuracy
// modifiers. Effect still randomizes attack/defence and uses difficulty.
namespace spellcross_tactical::predator_blast
{
inline bool Eligible(int type_flags, bool opposing, bool active, bool same_type,
                     double distance, int radius)
{
    // MapXY uses an isometric metric (diagonal adjacent tiles reach sqrt(2)).
    return opposing && active && !same_type && !(type_flags & 0x04) &&
           distance <= radius + 0.5;
}

inline int Attack(int flags, int armored, int light)
{
    return (flags & 0x20) ? armored : light;
}

inline int HitChance(int attack, int defence, int difficulty_bonus = 0)
{
    const int diff = attack - defence;
    const int chance = diff >= 0
        ? (diff > 8 ? 100 : 80 + diff * diff * 80 / 64)
        : (diff < -12 ? 20 : 80 - diff * diff * 60 / 144);
    return (std::clamp)(chance + difficulty_bonus, 20, 100);
}

inline int Lethality(int attack, int defence, int attack_roll, int defence_roll,
                     int difficulty_bonus = 0)
{
    const int diff = attack + attack_roll - defence - defence_roll;
    const int effect = diff >= 0
        ? (diff > 14 ? 100 : 16 + diff * diff * 84 / 196)
        : (diff < -10 ? 1 : 16 - diff * diff * 16 / 100);
    return (std::clamp)(effect + difficulty_bonus, 4, 100);
}

struct Damage { int active; int remainder; };

// Unlike an ordinary weapon hit, action 13 applies lethality to nominal
// company size, does not create wounded, and applies 10 damage units per
// percent to single-piece targets. Preserve special death-action targets
// using the original half-threshold adjustment at 0x2CED6 / 0x2CF42.
inline Damage Apply(int active, int nominal, int remainder, int effect,
                    bool protected_death_action)
{
    if (active <= 0 || nominal <= 0 || effect <= 0) return {active, remainder};
    if (protected_death_action)
    {
        const int threshold = nominal == 1
            ? (1000 - remainder) / 10 + 1 : active * 100 / nominal;
        if (effect >= threshold) effect = threshold / 2;
        if (effect == threshold) return {active, remainder};
    }
    if (nominal == 1)
    {
        const int damage = remainder + 10 * effect;
        return damage >= 1000 ? Damage{0, 0} : Damage{active, damage};
    }
    const int survivors = (std::max)(0, active - nominal * effect / 100);
    return {survivors, survivors == 0 ? 0 : remainder};
}
}
