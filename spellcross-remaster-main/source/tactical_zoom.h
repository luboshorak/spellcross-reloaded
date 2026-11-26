#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace spellcross_tactical
{
    // Integer percentages keep render sampling and mouse picking identical.
    inline int MapPixel(int screenPixel, int percent)
    {
        const std::int64_t value = static_cast<std::int64_t>(screenPixel) * 100;
        return static_cast<int>(value >= 0 ? value / percent : -((-value + percent - 1) / percent));
    }

    inline int MapExtent(int screenExtent, int percent)
    {
        return static_cast<int>((static_cast<std::int64_t>(screenExtent) * 100 + percent - 1) / percent);
    }

    inline int AnchoredScroll(int scroll, int screenPixel, int oldPercent, int newPercent)
    {
        return scroll + MapPixel(screenPixel, oldPercent) - MapPixel(screenPixel, newPercent);
    }

    class Zoom
    {
        inline static constexpr std::array<int, 9> Levels{50, 75, 100, 125, 150, 175, 200, 250, 300};
        int m_level = 2;
        std::int64_t m_remainder = 0;

    public:
        int Percent() const { return Levels[m_level]; }

        bool Wheel(int rotation, int delta)
        {
            if (delta <= 0)
                return false;
            m_remainder += rotation;
            const auto steps = m_remainder / delta;
            m_remainder %= delta;
            const int next = static_cast<int>(std::clamp<std::int64_t>(
                m_level + steps, 0, Levels.size() - 1));
            if (next == m_level)
                return false;
            m_level = next;
            return true;
        }
    };
}
