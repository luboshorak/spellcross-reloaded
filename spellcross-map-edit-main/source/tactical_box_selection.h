#pragma once

#include <algorithm>
#include <cstdlib>

namespace spellcross_tactical
{
    struct SelectionRect
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;

        bool Contains(int x, int y) const
        {
            return x >= left && x <= right && y >= top && y <= bottom;
        }
    };

    inline SelectionRect NormalizeSelectionRect(int x1, int y1, int x2, int y2)
    {
        return {
            (std::min)(x1, x2),
            (std::min)(y1, y2),
            (std::max)(x1, x2),
            (std::max)(y1, y2)
        };
    }

    inline bool IsSelectionDrag(int x1, int y1, int x2, int y2, int threshold)
    {
        threshold = (std::max)(0, threshold);
        return std::abs(x2 - x1) >= threshold || std::abs(y2 - y1) >= threshold;
    }
}
