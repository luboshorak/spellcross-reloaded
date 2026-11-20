#include "../source/mission_debrief_layout.h"
#include <stdexcept>
#include <iostream>

using namespace spellcross_mission_debrief;

static void Check(bool value, const char* reason)
{
    if (!value) throw std::runtime_error(reason);
}

int main()
{
    // LEVEL_09 long narration used to grow below 480 and cover the HUD.
    for (int fontHeight : {9, 13, 17, 22})
    {
        const auto longText = Compute(fontHeight + 1, 80);
        Check(longText.panel_x >= 0 &&
            longText.panel_x + longText.panel_width <= CanvasWidth,
            "mission result exceeds canvas width");
        Check(longText.panel_y >= 80, "frame overlaps original mission banner");
        Check(longText.panel_y + longText.panel_height <= PanelBottom &&
            PanelBottom < CanvasHeight, "frame overlaps bottom tactical HUD");
        Check(longText.visible_lines > 0 && longText.visible_lines < 80,
            "long story is not paginated");
        Check(longText.max_scroll_line == 80 - longText.visible_lines,
            "last debrief page is unreachable");
        Check(TextY + longText.visible_lines * (fontHeight + 1) <=
            longText.panel_height - FooterHeight,
            "visible text reaches the frame footer");
        Check(ClampScroll(-100, longText) == 0 &&
            ClampScroll(1000, longText) == longText.max_scroll_line,
            "debrief scroll escapes text bounds");
        Check(ScrollFromTrack(0, longText) == 0 &&
            ScrollFromTrack(longText.ScrollbarHeight(), longText) ==
                longText.max_scroll_line,
            "scrollbar cannot reach both beginning and ending");
        Check(PageStep(longText) > 0, "page navigation did not advance");
    }

    // Short missions retain a compact frame and never gain a fake scrollbar.
    const auto shortText = Compute(14, 2);
    Check(shortText.max_scroll_line == 0 && shortText.visible_lines == 2,
        "short result acquired unnecessary scrolling");
    Check(shortText.panel_height >= MinPanelHeight &&
        shortText.panel_y + shortText.panel_height < PanelBottom,
        "short result stopped being compact");

    // Exactly a page and exactly one line more than a page.
    const auto bound = Compute(14, 999);
    const auto exact = Compute(14, bound.visible_lines);
    const auto next = Compute(14, bound.visible_lines + 1);
    Check(exact.max_scroll_line == 0 && next.max_scroll_line == 1,
        "pagination boundary dropped or duplicated final text line");

    std::cout << "PASS: bounded tactical mission-result frame, complete pages, "
                 "short messages, wheel/track clamping\n";
    return 0;
}
