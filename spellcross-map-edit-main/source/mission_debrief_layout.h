#pragma once

#include <algorithm>

// Geometry of the original RAM2* mission-result frame in its 640x480 logical
// canvas. This is NOT the native wx strategic Event dialog. A chapter's text
// can be arbitrarily long; the frame may not grow past the bottom HUD (#178).
namespace spellcross_mission_debrief
{
    constexpr int CanvasWidth = 640;
    constexpr int CanvasHeight = 480;
    constexpr int PanelWidth = 488;      // wider than the original 408
    constexpr int PanelX = (CanvasWidth - PanelWidth) / 2;
    constexpr int PanelY = 92;           // below M_ACCOMP / M_FAILED banner
    constexpr int PanelBottom = 400;     // finish above the tactical HUD (starts near y=410)
    constexpr int MinPanelHeight = 74;
    constexpr int TextX = 32;
    constexpr int TextY = 18;
    constexpr int TextWidth = PanelWidth - 2 * TextX;
    constexpr int FooterHeight = 32;    // frame, scrollbar and bottom padding
    constexpr int ScrollbarX = PanelWidth - 20;
    constexpr int ScrollbarY = 22;
    constexpr int ScrollbarBottomInset = 28;

    struct Layout
    {
        int panel_x = PanelX;
        int panel_y = PanelY;
        int panel_width = PanelWidth;
        int panel_height = MinPanelHeight;
        int text_width = TextWidth;
        int visible_lines = 1;
        int total_lines = 1;
        int max_scroll_line = 0;

        int ScrollbarHeight() const
        {
            return (std::max)(1, panel_height - ScrollbarY - ScrollbarBottomInset);
        }
    };

    inline Layout Compute(int line_height, int line_count)
    {
        const int lineH = (std::max)(1, line_height);
        const int total = (std::max)(1, line_count);
        constexpr int maxPanelHeight = PanelBottom - PanelY;
        const int linesPerPage = (std::max)(1,
            (maxPanelHeight - TextY - FooterHeight) / lineH);
        const int visible = (std::min)(total, linesPerPage);

        Layout out;
        out.panel_height = (std::clamp)(
            TextY + visible * lineH + FooterHeight,
            MinPanelHeight, maxPanelHeight);
        out.visible_lines = visible;
        out.total_lines = total;
        out.max_scroll_line = total - visible;
        return out;
    }

    inline int ClampScroll(int line, const Layout& layout)
    {
        return (std::clamp)(line, 0, layout.max_scroll_line);
    }

    inline int PageStep(const Layout& layout)
    {
        return (std::max)(1, layout.visible_lines - 1);
    }

    // Scrollbar track click: 0 at the top, last page at the bottom.
    inline int ScrollFromTrack(int relative_y, const Layout& layout)
    {
        const int h = layout.ScrollbarHeight();
        const int pos = (std::clamp)(relative_y, 0, h);
        return layout.max_scroll_line * pos / h;
    }
}
