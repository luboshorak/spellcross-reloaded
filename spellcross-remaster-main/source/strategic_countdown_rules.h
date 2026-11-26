#pragma once
// Original Spellcross BIG_MAP.SAV stores a remaining Time(n) value even for
// inaccessible territories. The countdown must not advance while the
// strategic objective is unavailable, unlike a global absolute-turn timer.
#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <vector>

namespace spellcross_strategy
{
inline int PauseDeadlineForInactiveTurn(int absoluteDeadline, bool accessible)
{
    if (absoluteDeadline > 0 && !accessible &&
        absoluteDeadline < std::numeric_limits<int>::max())
        return absoluteDeadline + 1;
    return absoluteDeadline;
}

// Original VM_0..VM_9 sprites are individual variable-width decimal glyphs.
// Lay them out as one centered group so Time(10), Time(12), Time(15), etc.
// display their true values instead of clamping the image to VM_9.
struct CountdownDigitGlyph
{
    int digit = 0;
    int x_offset = 0;    // sprite left edge relative to countdown center
    int scaled_width = 0;
};

inline std::vector<CountdownDigitGlyph> LayoutCountdownDigits(
    int remaining,
    const std::array<int, 10>& scaledWidths,
    int gapPixels)
{
    if (remaining < 0)
        return {};

    const std::string decimal = std::to_string(remaining);
    const int gap = std::max(0, gapPixels);
    int totalWidth = gap * static_cast<int>(decimal.size() - 1);
    for (const char ch : decimal)
    {
        const int digit = ch - '0';
        if (scaledWidths[digit] <= 0)
            return {}; // Required retail sprite missing: caller draws text.
        totalWidth += scaledWidths[digit];
    }

    std::vector<CountdownDigitGlyph> glyphs;
    glyphs.reserve(decimal.size());
    int left = -totalWidth / 2;
    for (const char ch : decimal)
    {
        const int digit = ch - '0';
        const int width = scaledWidths[digit];
        glyphs.push_back({digit, left, width});
        left += width + gap;
    }
    return glyphs;
}
} // namespace spellcross_strategy
