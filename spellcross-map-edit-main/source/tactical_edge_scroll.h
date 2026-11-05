#pragma once

#include <algorithm>

namespace spellcross_tactical
{
    struct EdgeScrollDirection
    {
        int x = 0;
        int y = 0;
    };

    // Physical canvas coordinates: the lower edge is the *battlefield* just
    // above the irregular HUD, not the bottom of the native window. Do not
    // scroll while the pointer is over HUD buttons or outside the canvas.
    inline EdgeScrollDirection EdgeScrollAt(
        int mouse_x, int mouse_y, int canvas_width, int canvas_height,
        int hud_top, int edge_margin)
    {
        EdgeScrollDirection result;
        if (canvas_width <= 0 || canvas_height <= 0 ||
            mouse_x < 0 || mouse_x >= canvas_width ||
            mouse_y < 0 || mouse_y >= canvas_height)
            return result;

        const int playable_bottom = (std::clamp)(hud_top, 0, canvas_height);
        if (playable_bottom <= 0 || mouse_y >= playable_bottom)
            return result;

        const int horizontal_margin = (std::clamp)(edge_margin, 1,
            (std::max)(1, canvas_width / 2));
        const int vertical_margin = (std::clamp)(edge_margin, 1,
            (std::max)(1, playable_bottom / 2));

        if (mouse_x < horizontal_margin)
            result.x = -1;
        else if (mouse_x >= canvas_width - horizontal_margin)
            result.x = 1;

        if (mouse_y < vertical_margin)
            result.y = -1;
        else if (mouse_y >= playable_bottom - vertical_margin)
            result.y = 1;

        return result;
    }

    // Keep edge scroll visually steady across tactical zoom and the optional
    // 125% nearest-neighbour display scale (map units -> physical pixels).
    inline int EdgeScrollStep(int zoom_percent, int display_scale_percent)
    {
        zoom_percent = (std::clamp)(zoom_percent, 50, 300);
        display_scale_percent = display_scale_percent == 125 ? 125 : 100;
        const int denominator = zoom_percent * display_scale_percent;
        return (std::max)(1, (7 * 10000 + denominator / 2) / denominator);
    }
}
