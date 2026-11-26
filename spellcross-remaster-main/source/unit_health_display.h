#pragma once

#include <algorithm>

namespace spellcross_health
{
struct BarWidths { int active = 0; int present = 0; };

// Single-member units use the DOS damage accumulator (thousandths), not the
// infantry wounded counter. Multi-member units retain active/wounded counts.
inline BarWidths GetBars(int maximum, int active, int wounded, int damage, int width)
{
    if (maximum <= 0 || active <= 0 || width <= 0)
        return {};
    if (maximum == 1)
        return {width * (1000 - std::clamp(damage, 0, 999)) / 1000, width};
    const int living = std::clamp(active, 0, maximum);
    const int present = living + std::clamp(wounded, 0, maximum - living);
    return {width * living / maximum, width * present / maximum};
}
}
