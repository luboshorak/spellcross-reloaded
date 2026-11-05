#include "../source/tactical_edge_scroll.h"

#include <cassert>

int main()
{
    using namespace spellcross_tactical;
    constexpr int width = 1600;
    constexpr int height = 900;
    constexpr int hud = 830;
    constexpr int margin = 72;

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

    // A wider active region makes it practical to hold the pointer near
    // the physical edge, with enough space to work in fullscreen.
    d = EdgeScrollAt(50, 420, width, height, hud, margin);
    assert(d.x == -1 && d.y == 0);
    d = EdgeScrollAt(width - 50, 420, width, height, hud, margin);
    assert(d.x == 1 && d.y == 0);
    d = EdgeScrollAt(800, hud - 50, width, height, hud, margin);
    assert(d.x == 0 && d.y == 1);
    d = EdgeScrollAt(800, hud - margin - 1, width, height, hud, margin);
    assert(d.x == 0 && d.y == 0);

    // Real wall-clock time, not the number of paint/timer frames, determines
    // travel distance. 100 x 10ms and 10 x 100ms should be almost equal.
    auto simulate = [](int frames, double dt, int zoom, int scale) {
        double remainder = 0.0;
        int total = 0;
        for (int i = 0; i < frames; ++i)
            total += EdgeScrollStep(dt, zoom, scale, remainder);
        return total;
    };
    const int smooth = simulate(100, 0.01, 100, 100);
    const int choppy = simulate(10, 0.1, 100, 100);
    assert(smooth >= 1299 && smooth <= 1301);
    assert(choppy >= 1299 && choppy <= 1301);
    assert(smooth == choppy);

    // Preserve similar PHYSICAL speed across 50%-300% zoom and 125% scale.
    assert(simulate(100, 0.01, 100, 125) == 1040);
    assert(simulate(100, 0.01, 200, 100) == 650);
    assert(simulate(100, 0.01, 50, 100) == 2600);
    assert(simulate(100, 0.01, 300, 125) >= 345);

    double fraction = 0.0;
    assert(EdgeScrollStep(0.0, 100, 100, fraction) == 0);
    // One 5-second pause must not teleport the camera across multiple maps.
    assert(EdgeScrollStep(5.0, 100, 100, fraction) == 325);
    return 0;
}
