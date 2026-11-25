// Spellcross strategic aftermath: wounded survivors recover after victory.
#pragma once

#include <algorithm>

namespace spellcross_strategy
{
    inline int RecoveredStrategicHealth(int ready, int wounded, int maximum,
        int damageRemainder, bool singleMember, bool victory)
    {
        // Single-member units store fractional damage instead of casualties.
        if (singleMember)
            return std::clamp((1000 - std::clamp(damageRemainder, 0, 999) + 5) / 10, 1, 100);
        if (maximum <= 0)
            return 100;

        // The original manual specifies that wounded survivors heal after a
        // successful mission. Killed members do not return for free.
        const int recovered = std::clamp(ready, 0, maximum) +
            (victory ? std::clamp(wounded, 0, maximum) : 0);
        const int alive = std::min(maximum, recovered);
        return std::clamp((alive * 100 + maximum / 2) / maximum, 1, 100);
    }
}
