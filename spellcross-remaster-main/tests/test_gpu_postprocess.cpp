#include "../source/gpu_postprocess.h"
#include "../source/tactical_filters.h"
#include <cassert>
#include <iostream>
int main()
{
    using namespace spellcross_gpu;
    Backend gpu;
    uint8_t pal[256][3]{};
    for(int i=0;i<256;++i) {pal[i][0]=uint8_t(i);pal[i][1]=uint8_t(i/2);pal[i][2]=uint8_t(i/3);}
    std::array<uint8_t,16> art{};art.fill(100);
    std::array<int,4> hud{};hud.fill(4);
    std::array<uint32_t,16> tags{};
    Frame frame{8,8,4,4,4,4,100,200,0,14,1,50,0,0,0,0,0,0,0,0};
    for(int mode=0;mode<=spellcross_filters::LastMode;++mode)
    {
        assert(Supports(mode)==spellcross_filters::Available(mode));
        if(!Supports(mode)) continue;
        frame.mode=mode;
        bool valid=true;int rows=0;
        if(!gpu.Run(frame,art.data(),art.data(),pal,tags.data(),hud.data(),[&](int,const uint32_t* data){
            ++rows;for(int x=0;x<8;++x) {
                // CRT intentionally darkens the interior, even on a flat field.
                if(mode==20) {valid &= (data[x]&255u)<=100u;continue;}
                valid &= (data[x]&0xffffffu)==(100u|(50u<<8)|(33u<<16));
            }
        },true)) {std::cerr<<gpu.Error()<<'\n';return 1;}
        assert(valid&&rows==8);
    }
    // Mixed-colour comparison, interior pixels unaffected by the HUD guard.
    // 3x zoom gives both subpixel corners and pixel centres.
    std::array<uint8_t,64> mixed{};
    for(int y=0;y<8;++y) for(int x=0;x<8;++x)
        mixed[y*8+x]=uint8_t(x==0?0:((x+y)%3==0?160:((x-y)>1?100:60)));
    std::array<uint8_t,24*24> logical{};
    for(int y=0;y<24;++y) for(int x=0;x<24;++x) logical[y*24+x]=mixed[(y/3)*8+x/3];
    std::array<int,24> fullHud{};fullHud.fill(24);
    Frame comparison{24,24,24,24,8,8,300,100,0,0,0,0,0,0,0,0,0,0,0,0};
    spellcross_filters::Image image;
    image.Prepare(mixed.data(),8,8,pal,false,0);
    for(int mode=1;mode<=spellcross_filters::LastMode;++mode)
    {
        if(!Supports(mode)) continue;
        comparison.mode=mode;
        bool valid=true;
        if(!gpu.Run(comparison,mixed.data(),logical.data(),pal,nullptr,fullHud.data(),[&](int y,const uint32_t* data){
            if(y>=22) return;
            for(int x=0;x<24;++x)
            {
                const auto ax=spellcross_filters::MakeAxis(x,8,300,100,mode);
                const auto ay=spellcross_filters::MakeAxis(y,8,300,100,mode);
                const auto expected=spellcross_filters::Protected(logical[y*24+x])?
                    spellcross_filters::PaletteRGB(pal,logical[y*24+x]):
                    mode==9?image.SampleScale2x(ax,ay):
                    mode==19?image.SampleSoftEPX(ax,ay):
                    mode==17?image.SampleEdgeAdaptive(ax,ay):
                    mode==18?image.SampleTriangle(ax,ay):
                    mode==20?image.SampleCRT(ax,ay,true):image.Sample(ax,ay);
                for(int c=0;c<3;++c)
                    if(std::abs(int((data[x]>>(c*8))&255)-int(expected[c]))>2) valid=false;
            }
        },true)) {std::cerr<<gpu.Error()<<'\n';return 1;}
        if(!valid) {std::cerr<<"CPU/GPU mismatch in filter "<<mode<<'\n';return 1;}
    }
    // Lighting intensity and shadow-only grading must agree with CPU output.
    art.fill(100);
    frame.mode=0;frame.dedither=0;frame.sharpen=0;frame.lighting=1;
    frame.width=frame.height=frame.sw=frame.sh=frame.nw=frame.nh=4;
    frame.zoom=frame.dpi=100;
    tags.fill(spellcross_enhanced::MakeTag(100,-12));
    for(int grade : {0,50,100})
    {
        frame.gradePercent=grade;
        const auto expected=spellcross_enhanced::ShadeRGB(pal[100],-12,false,grade);
        bool match=true;
        if(!gpu.Run(frame,art.data(),art.data(),pal,tags.data(),hud.data(),[&](int,const uint32_t* row){
            for(int x=0;x<4;++x) for(int c=0;c<3;++c)
                match &= int((row[x]>>(c*8))&255)==expected[c];
        },true)) {std::cerr<<gpu.Error()<<'\n';return 1;}
        assert(match);
    }
    frame.lighting=0;
    art.fill(245);tags.fill(spellcross_enhanced::MakeTag(245,0,spellcross_enhanced::Material::Water));
    frame.waterStrength=80;frame.waterPhase=37;
    bool waterMatch=true;
    if(!gpu.Run(frame,art.data(),art.data(),pal,tags.data(),hud.data(),[&](int y,const uint32_t* row){
        for(int x=0;x<4;++x) {
            const auto expected=spellcross_enhanced::ApplyWater({pal[245][0],pal[245][1],pal[245][2]},x*4,y*4,37,80,false);
            for(int c=0;c<3;++c) waterMatch &= int((row[x]>>(c*8))&255)==expected[c];
        }
    },true)) {std::cerr<<gpu.Error()<<'\n';return 1;}
    assert(waterMatch);
    frame.waterStrength=0;
    art.fill(100);tags.fill(spellcross_enhanced::MakeTag(100,0));tags[0]=0;
    frame.atmosphereIntensity=80;frame.atmospherePhase=37;
    for(int weather=1;weather<=4;++weather)
    {
        frame.atmosphere=weather;
        bool weatherMatch=true;
        if(!gpu.Run(frame,art.data(),art.data(),pal,tags.data(),hud.data(),[&](int y,const uint32_t* row){
            for(int x=0;x<4;++x)
            {
                const spellcross_enhanced::RGB raw{pal[100][0],pal[100][1],pal[100][2]};
                const auto expected=(x||y)?spellcross_enhanced::ApplyAtmosphere(raw,weather,80,x*4,y*4,37,false):raw;
                for(int c=0;c<3;++c) weatherMatch &= int((row[x]>>(c*8))&255)==expected[c];
            }
        },true)) {std::cerr<<gpu.Error()<<'\n';return 1;}
        assert(weatherMatch);
    }
    frame.atmosphere=0;frame.width=frame.height=8;frame.dpi=200;
    // A black fog/border index must remain black.
    art.fill(0);frame.mode=14;
    bool valid=true;
    if(!gpu.Run(frame,art.data(),art.data(),pal,tags.data(),hud.data(),[&](int,const uint32_t* data){
        for(int x=0;x<8;++x) valid &= (data[x]&0xffffffu)==0;
    },true)) {std::cerr<<gpu.Error()<<'\n';return 1;}
    assert(valid);
    std::cout<<"D3D11 compute shader dispatch verified\n";
}
