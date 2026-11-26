#include "../source/tactical_zoom.h"
#include <cassert>
#include <climits>

int main()
{
    using namespace spellcross_tactical;
    Zoom zoom;
    assert(zoom.Percent() == 100);
    assert(!zoom.Wheel(60, 120));
    assert(zoom.Wheel(60, 120) && zoom.Percent() == 125);
    assert(zoom.Wheel(-120, 120) && zoom.Percent() == 100);
    assert(!zoom.Wheel(0, 0));

    assert(zoom.Wheel(INT_MAX, 120) && zoom.Percent() == 300);
    assert(!zoom.Wheel(120, 120));
    assert(zoom.Wheel(INT_MIN, 120) && zoom.Percent() == 50);
    assert(!zoom.Wheel(-120, 120));

    // Render sampling and hit testing must resolve the same source pixel.
    assert(MapPixel(0, 150) == 0);
    assert(MapPixel(1, 150) == 0);
    assert(MapPixel(2, 150) == 1);
    assert(MapPixel(639, 50) == 1278);
    assert(MapPixel(-1, 150) == -1);
    assert(MapExtent(641, 150) == 428);
    assert(MapExtent(1920, 50) == 3840);
    assert(MapExtent(0, 300) == 0);
    // A point under the mouse stays on the same world pixel after zooming.
    assert(AnchoredScroll(400, 300, 100, 150) == 500);
    assert(AnchoredScroll(500, 300, 150, 100) == 400);

    const unsigned char source[] = {10, 20, 30, 40};
    const unsigned char doubled[] = {10, 10, 20, 20, 30, 30, 40, 40};
    for (int x = 0; x < 8; ++x)
        assert(source[MapPixel(x, 200)] == doubled[x]);

    // The ceil-sized source viewport covers the last output pixel, including
    // odd canvas sizes and all supported percentages.
    for (int percent : {50, 75, 100, 125, 150, 175, 200, 250, 300})
        for (int width : {1, 639, 641, 1920, 3841})
            assert(MapPixel(width - 1, percent) < MapExtent(width, percent));
}
