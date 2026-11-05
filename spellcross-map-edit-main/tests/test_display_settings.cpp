// Regression tests for display preferences and exact nearest-pixel geometry.
#include "../source/display_settings.h"
#include <cassert>
#include <cmath>

int main()
{
    using namespace spellcross_display;
    const Settings defaults = Current();
    assert(defaults.scale_percent == 125);
    assert(!defaults.smooth_scaling);
    assert(!UseSmoothScaling());
    assert(ResizeQuality() == wxIMAGE_QUALITY_NEAREST);
    assert(defaults.brightness == 100);
    assert(std::abs(defaults.gamma - 1.3) < 1e-9);
    assert(defaults.temperature == 6500);

    for (const int scale : {100, 125})
    {
        Settings settings = defaults;
        settings.scale_percent = scale;
        Set(settings);
        const int width = 1662;
        const int logicalWidth = LogicalExtent(width);
        assert(logicalWidth == (width * 100 + scale - 1) / scale);
        for (int pixel = 0; pixel < width; ++pixel)
        {
            const int logical = ToLogical(pixel);
            assert(logical >= 0 && logical < logicalWidth);
            // The logical hitbox from a framebuffer pixel must include exactly
            // the physical pixel sampled by the nearest-neighbour renderer.
            assert(ToDevice(logical) <= pixel);
            assert(pixel < ToDevice(logical + 1));
        }
    }

    // Bilinear sampling uses source-pixel centers and clamps the four edges.
    const auto left = BilinearSample(0, 125, 4);
    assert(left.first == 0 && left.second == 0 && left.weight == 0);
    const auto middle = BilinearSample(2, 125, 4);
    assert(middle.first == 1 && middle.second == 2 && middle.weight == 128);
    const auto right = BilinearSample(100, 125, 4);
    assert(right.first == 3 && right.second == 3 && right.weight == 0);
    assert(BlendBilinear(0, 255, 0, 255, 128, 0) == 128);
    assert(BlendBilinear(0, 0, 255, 255, 0, 128) == 128);
    assert(BlendBilinear(42, 42, 42, 42, 128, 128) == 42);

    Settings smooth = defaults;
    smooth.smooth_scaling = true;
    const auto beforeSmoothing = Revision();
    Set(smooth);
    assert(UseSmoothScaling());
    assert(ResizeQuality() == wxIMAGE_QUALITY_BILINEAR);
    assert(Revision() > beforeSmoothing);
    Set(defaults);
    assert(!UseSmoothScaling());
    assert(ResizeQuality() == wxIMAGE_QUALITY_NEAREST);

    Settings oneToOne = defaults;
    oneToOne.scale_percent = 100;
    oneToOne.smooth_scaling = true;
    Set(oneToOne);
    assert(Current().scale_percent == 100 && !Current().smooth_scaling);
    assert(!UseSmoothScaling());
    Set(defaults);

    Settings warm = defaults;
    warm.brightness = 120;
    warm.gamma = 1.1;
    warm.temperature = 3500;
    const auto previous = Revision();
    Set(warm);
    assert(Revision() > previous);
    assert(Tables().revision == Revision());
    assert(Tables().tactical[2][200] < Tables().tactical[0][200]);
    assert(Current().scale_percent == 125);

    Settings invalid = defaults;
    invalid.brightness = 500;
    invalid.temperature = 100;
    invalid.scale_percent = 101;
    Set(invalid);
    assert(Current().brightness == 150);
    assert(Current().temperature == 3500);
    assert(Current().scale_percent == 125);

    Set(defaults);
    return 0;
}
