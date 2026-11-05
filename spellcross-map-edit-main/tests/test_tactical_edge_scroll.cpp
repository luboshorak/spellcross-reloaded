#include "../source/tactical_edge_scroll.h"

#include <cassert>

int main()
{
    using namespace spellcross_tactical;
    constexpr int width = 1600;
    constexpr int height = 900;
    constexpr int hud = 830;
    constexpr int margin = 24;

    auto d = EdgeScrollAt(800, 400, width, height, hud, margin);
    assert(d.x == 0 && d.y == 0);

    d = EdgeScrollAt(0, 0, width, height, hud, margin);
    assert(d.x == -1 && d.y == -1);

    d = EdgeScrollAt(1599, 827, width, height, hud, margin);
    assert(d.x == 1 && d.y == 1);

    d = EdgeScrollAt(800, hud - 1, width, height, hud, margin);
    assert(d.x == 0 && d.y == 1);

    // Mouse on the HUD, off-canvas, or on a zero-sized canvas never pans.
    d = EdgeScrollAt(0, hud, width, height, hud, margin);
    assert(d.x == 0 && d.y == 0);
    d = EdgeScrollAt(1599, 899, width, height, hud, margin);
    assert(d.x == 0 && d.y == 0);
    d = EdgeScrollAt(-1, 0, width, height, hud, margin);
    assert(d.x == 0 && d.y == 0);
    d = EdgeScrollAt(width, 100, width, height, hud, margin);
    assert(d.x == 0 && d.y == 0);
    d = EdgeScrollAt(0, 0, 0, height, hud, margin);
    assert(d.x == 0 && d.y == 0);

    // If there is no HUD, the lower canvas edge is the scroll trigger.
    d = EdgeScrollAt(800, height - 1, width, height, height, margin);
    assert(d.x == 0 && d.y == 1);

    // Physical scrolling speed stays roughly the same at different zoom.
    assert(EdgeScrollStep(100, 100) == 7);
    assert(EdgeScrollStep(200, 100) == 4);
    assert(EdgeScrollStep(50, 100) == 14);
    assert(EdgeScrollStep(100, 125) == 6);
    assert(EdgeScrollStep(300, 125) >= 1);
    return 0;
}
