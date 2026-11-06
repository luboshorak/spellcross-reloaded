#include "../source/tactical_edge_pan.h"
#include <cassert>
#include <cstdlib>

int main()
{
    using namespace spellcross_tactical;

    // The hot zone is measured in client pixels and excludes the HUD.
    assert(EdgePanAt(400, 200, 800, 480, 28).x == 0.0);
    assert(EdgePanAt(0, 200, 800, 480, 28).x == -1.0);
    assert(EdgePanAt(799, 200, 800, 480, 28).x == 1.0);
    assert(EdgePanAt(400, 0, 800, 480, 28).y == -1.0);
    assert(EdgePanAt(400, 479, 800, 480, 28).y == 1.0);
    assert(EdgePanAt(0, 480, 800, 480, 28).x == 0.0);
    assert(EdgePanAt(0, 550, 800, 480, 28).y == 0.0);
    assert(EdgePanAt(-1, 100, 800, 480, 28).x == 0.0);
    assert(EdgePanAt(800, 100, 800, 480, 28).x == 0.0);
    assert(EdgePanAt(27, 100, 800, 480, 28).x < 0.0);
    assert(EdgePanAt(28, 100, 800, 480, 28).x == 0.0);
    assert(EdgePanAt(0, 0, 800, 480, 28).x == -1.0);
    assert(EdgePanAt(0, 0, 800, 480, 28).y == -1.0);
    assert(EdgePanAt(0, 0, 20, 20, 28).x == -1.0);
    assert(EdgePanAt(10, 10, 20, 20, 28).x == 0.0);
    assert(EdgePanAxis(0, 0, 800, 0) == 0.0);

    // Fractional logical pixels accumulate over timer ticks: no speed
    // dependency on how many wx mouse-move events the OS delivered.
    EdgePanAccumulator pan;
    int total = 0;
    for (int i = 0; i < 100; ++i)
        total += pan.Advance({1.0, 0.0}, 0.001, 1.0).first;
    assert(total == 48);
    pan.Reset();
    total = 0;
    for (int i = 0; i < 100; ++i)
        total += pan.Advance({1.0, 0.0}, 0.001, 0.5).first;
    assert(total == 24);

    pan.Reset();
    // One delayed event cannot teleport the camera across the map.
    assert(pan.Advance({1.0, 0.0}, 10.0, 1.0).first == 24);
    pan.Reset();
    pan.Advance({0.01, 0.0}, 0.01, 1.0);
    assert(pan.Advance({0.0, 0.0}, 0.01, 1.0).first == 0);
    assert(pan.Advance({-1.0, 0.0}, 0.01, 1.0).first < 0);
    return 0;
}
