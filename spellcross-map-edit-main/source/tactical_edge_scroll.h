#pragma once

#include <algorithm>
#include <cmath>

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

    // The old fixed "7 pixels every 25 ms" implementation became slower
    // whenever software rendering dropped frames. Integrate real wall time
    // instead. This gives approximately 1300 PHYSICAL pixels per second at
    // every tactical zoom/scale, independent of timer and paint cadence.
    //
    // fractional_pixels is per-axis state owned by the caller, preserving
    // subpixel movement across frequent wx timer ticks and high zoom levels.
    // A 250ms cap prevents a long breakpoint / background stall from moving
    // the camera several screens in a single event.
    inline int EdgeScrollStep(double elapsed_seconds, int zoom_percent,
        int display_scale_percent, double& fractional_pixels)
    {
        if (!std::isfinite(elapsed_seconds) || elapsed_seconds <= 0.0)
            return 0;
        elapsed_seconds = (std::clamp)(elapsed_seconds, 0.0, 0.25);
        zoom_percent = (std::clamp)(zoom_percent, 50, 300);
        display_scale_percent = display_scale_percent == 125 ? 125 : 100;
        constexpr double physical_pixels_per_second = 1300.0;
        const double logical_pixels_per_second =
            physical_pixels_per_second * 10000.0 /
            (zoom_percent * display_scale_percent);
        const double total = elapsed_seconds * logical_pixels_per_second +
            fractional_pixels;
        const int pixels = static_cast<int>(total);
        fractional_pixels = total - pixels;
        return pixels;
    }
}
