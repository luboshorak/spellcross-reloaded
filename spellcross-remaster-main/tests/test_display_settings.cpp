// Regression tests for display defaults, filtering, and exact pixel geometry.
#include "../source/display_settings.h"
#include <cassert>
#include <cmath>

int main()
{
    using namespace spellcross_display;
    const Settings defaults = Current();
    assert(!(defaults.post_bloom||defaults.post_contrast||defaults.post_vignette||defaults.post_grain));
    Settings custom=defaults;custom.brightness=125;custom.scale_percent=100;custom.gpu_acceleration=true;
    const auto maximum=Preset(GraphicsPreset::Maximum,custom);
    assert(maximum.ambient_occlusion&&maximum.advanced_rendering&&maximum.material_v2&&maximum.post_bloom>0);
    assert(maximum.brightness==125&&maximum.scale_percent==100&&maximum.gpu_acceleration&&maximum.atmosphere==0);
    const auto classic=Preset(GraphicsPreset::Original,maximum);
    assert(!classic.advanced_rendering&&!classic.directional_shadows&&!classic.animated_water&&!classic.ambient_occlusion);
    assert(!classic.post_bloom&&!classic.post_contrast&&!classic.post_vignette&&!classic.post_grain&&classic.original_shadows);
    const auto balanced=Preset(GraphicsPreset::Balanced,custom);
    assert(balanced.advanced_rendering&&balanced.directional_shadows&&!balanced.ambient_occlusion&&!balanced.material_v2);
    assert(Preset(GraphicsPreset::Defaults,maximum).brightness==defaults.brightness);
    Settings post=defaults;post.post_bloom=80;post.post_contrast=45;post.post_vignette=30;post.post_grain=15;
    auto postRevision=Revision();Set(post);
    assert(Revision()>postRevision&&Current().post_bloom==80&&Current().post_grain==15);
    post.post_bloom=200;post.post_grain=-1;
    assert(Normalize(post).post_bloom==100&&Normalize(post).post_grain==0);
    Set(defaults);
    assert(defaults.atmosphere==0 && defaults.atmosphere_intensity==45);
    assert(!defaults.atmosphere_v2 && !defaults.ambient_occlusion && defaults.ambient_intensity==55 && defaults.ambient_radius==8);
    Settings weather=defaults;weather.atmosphere=4;weather.atmosphere_intensity=80;
    weather.atmosphere_v2=true;weather.ambient_occlusion=true;weather.ambient_intensity=70;weather.ambient_radius=10;
    const auto weatherRevision=Revision();Set(weather);
    assert(Revision()>weatherRevision&&Current().atmosphere==4&&Current().atmosphere_intensity==80);
    assert(Current().atmosphere_v2&&Current().ambient_occlusion&&Current().ambient_intensity==70&&Current().ambient_radius==10);
    weather.ambient_radius=50;weather.ambient_intensity=-1;
    assert(Normalize(weather).ambient_radius==12&&Normalize(weather).ambient_intensity==0);
    weather.atmosphere=99;weather.atmosphere_intensity=-10;
    assert(Normalize(weather).atmosphere==4&&Normalize(weather).atmosphere_intensity==0);
    Set(defaults);
    assert(defaults.scale_percent == 125);
    assert(defaults.smooth_scaling);
    assert(UseSmoothScaling());
    assert(ResizeQuality() == wxIMAGE_QUALITY_BILINEAR);
    assert(defaults.brightness == 100);
    assert(std::abs(defaults.gamma - 1.3) < 1e-9);
    assert(defaults.temperature == 6500);
    assert(!defaults.advanced_rendering);
    assert(defaults.shadow_direction==0 && defaults.shadow_length==100 && defaults.shadow_softness==33 && defaults.contact_shadows);
    assert(!defaults.directional_shadows && defaults.original_shadows && defaults.lighting_intensity==100 && defaults.shadow_strength==60);
    assert(!defaults.procedural_materials && !defaults.procedural_upscaling);
    assert(!defaults.water_v2);
    assert(maximum.water_v2&&balanced.water_v2&&!classic.water_v2);
    assert(!defaults.material_v2 && !defaults.animated_water && defaults.material_intensity==100 && defaults.water_intensity==60);
    Settings surfaces=defaults; surfaces.material_v2=true; surfaces.animated_water=true;
    surfaces.material_intensity=75;surfaces.water_intensity=45;surfaces.water_v2=true;
    const auto surfaceRev=Revision();Set(surfaces);
    assert(Revision()>surfaceRev && Current().animated_water && Current().water_v2 && Current().material_v2);
    assert(Current().material_intensity==75 && Current().water_intensity==45);
    surfaces.water_intensity=-10;surfaces.material_intensity=110;
    assert(Normalize(surfaces).water_intensity==0 && Normalize(surfaces).material_intensity==100);
    Set(defaults);
    assert(defaults.tactical_filter==spellcross_filters::Legacy && !defaults.tactical_dedither && defaults.tactical_sharpen==0);
    Settings enhanced = defaults;
    enhanced.advanced_rendering = true;
    const auto revisionBeforeEnhanced = Revision();
    Set(enhanced);
    assert(Current().advanced_rendering && Revision() > revisionBeforeEnhanced);
    Set(defaults);
    assert(!Current().advanced_rendering);
    Settings shadowOnly=defaults;
    shadowOnly.directional_shadows=true;
    shadowOnly.original_shadows=false;
    shadowOnly.contact_shadows=false;shadowOnly.shadow_direction=2;shadowOnly.shadow_length=150;shadowOnly.shadow_softness=80;
    shadowOnly.shadow_strength=88;
    shadowOnly.lighting_intensity=25;
    const auto shadowRevision=Revision();
    Set(shadowOnly);
    assert(Current().directional_shadows && !Current().advanced_rendering && !Current().original_shadows);
    assert(Current().shadow_strength==88 && Current().lighting_intensity==25 && Revision()>shadowRevision);
    assert(!Current().contact_shadows && Current().shadow_direction==2 && Current().shadow_length==150 && Current().shadow_softness==80);
    shadowOnly.shadow_strength=180;shadowOnly.lighting_intensity=-10;
    assert(Normalize(shadowOnly).shadow_strength==100 && Normalize(shadowOnly).lighting_intensity==0);
    Set(defaults);
    for (unsigned mask=0;mask<8;++mask)
    {
        Settings independent=defaults;
        independent.advanced_rendering=(mask&1)!=0;
        independent.procedural_materials=(mask&2)!=0;
        independent.procedural_upscaling=(mask&4)!=0;
        Set(independent);
        assert(Current().advanced_rendering==independent.advanced_rendering);
        assert(Current().procedural_materials==independent.procedural_materials);
        assert(Current().procedural_upscaling==independent.procedural_upscaling);
    }
    Set(defaults);

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

    // The sharp mode is still user-selectable and persists until changed.
    Settings sharp = defaults;
    sharp.smooth_scaling = false;
    const auto beforeFilterChange = Revision();
    Set(sharp);
    assert(!UseSmoothScaling());
    assert(ResizeQuality() == wxIMAGE_QUALITY_NEAREST);
    assert(Revision() > beforeFilterChange);
    Set(defaults);
    assert(UseSmoothScaling());
    assert(ResizeQuality() == wxIMAGE_QUALITY_BILINEAR);

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
    invalid.tactical_filter = 99;
    invalid.tactical_sharpen = 120;
    Set(invalid);
    assert(Current().brightness == 150);
    assert(Current().temperature == 3500);
    assert(Current().tactical_filter==spellcross_filters::Legacy && Current().tactical_sharpen==100);
    Settings filterSettings=defaults;
    for(int id : {3,4,5,6,7,8,12,15,16}) { Settings retired=defaults; retired.tactical_filter=id;
        assert(Normalize(retired).tactical_filter==spellcross_filters::Legacy); }
    filterSettings.tactical_filter=spellcross_filters::CubicSharp;
    filterSettings.tactical_dedither=true;
    filterSettings.tactical_sharpen=25;
    const auto before=Revision();
    Set(filterSettings);
    assert(Revision()>before && Current().tactical_dedither && Current().tactical_sharpen==25);
    assert(Current().tactical_filter==spellcross_filters::CubicSharp);
    assert(Current().scale_percent == 125);

    Set(defaults);
    return 0;
}
