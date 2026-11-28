#include "../source/advanced_rendering.h"
#include <cassert>
#include <array>
#include <vector>
#include <cstring>

int main()
{
    using namespace spellcross_enhanced;
    assert(Relief(2,2,2,2,2) == 0);
    assert(Relief(1,3,2,2,2) > 0);
    assert(Relief(3,1,2,2,2) < 0);
    assert(Contact(40,20,40,20,20,10,8) == -8);
    assert(Contact(60,20,40,20,20,10,8) == 0);
    assert(Contact(45,20,40,20,20,10,8) < 0);
    std::array<std::uint8_t,768> colors{};
    for(int i=0;i<256;++i)
        colors[i*3]=colors[i*3+1]=colors[i*3+2]=static_cast<std::uint8_t>(i);
    Palette palette;
    assert(palette.Map(120,-8)==120);
    palette.Prepare(colors.data());
    assert(palette.Map(120,-8)<palette.Map(120,0));
    assert(palette.Map(120,4)>palette.Map(120,0));
    for(int shade=-20;shade<=20;++shade)
    {
        assert(palette.Map(0,shade)==0);
        for(int i=240;i<256;++i) assert(palette.Map(static_cast<std::uint8_t>(i),shade)==i);
        for(int i=1;i<240;++i) { const auto v=palette.Map(static_cast<std::uint8_t>(i),shade); assert(v>0&&v<240); }
    }
    // A native scanline with transparent and animated water pixels.
    std::vector<std::uint8_t> sprite(2*sizeof(int)+4);
    int offset=0, count=4;
    std::memcpy(sprite.data(),&offset,sizeof(int));
    std::memcpy(sprite.data()+sizeof(int),&count,sizeof(int));
    sprite[2*sizeof(int)]=0; sprite[2*sizeof(int)+1]=120;
    sprite[2*sizeof(int)+2]=245; sprite[2*sizeof(int)+3]=120;
    std::array<std::uint8_t,8> buffer{}; buffer.fill(254);
    GroundLight light; light.relief=-5;
    DrawGround(sprite,1,0,buffer.data(),buffer.size(),4,0,0,palette,light);
    assert(buffer[0]==254 && buffer[1]<120 && buffer[2]==245 && buffer[3]<120);
    assert(buffer[4]==254);
    auto second=buffer;
    DrawGround(sprite,1,0,second.data(),second.size(),4,0,0,palette,light);
    assert(second==buffer);
    buffer.fill(254);
    DrawGround(sprite,1,0,buffer.data(),buffer.size(),4,-2,0,palette,light);
    assert(buffer[0]==245 && buffer[1]<120 && buffer[2]==254 && buffer[3]==254);
    buffer.fill(254);
    DrawGround(sprite,1,0,buffer.data(),buffer.size(),4,0,-1,palette,light);
    for(auto b:buffer) assert(b==254);
    // RGB lighting creates new colours instead of rounding back to a
    // 256-entry palette; strong shadows and material variation remain visible.
    const std::uint8_t sand[3]={170,125,75};
    const auto neutral=ShadeRGB(sand,0,false);
    const auto shadow=ShadeRGB(sand,-20,false);
    const auto sun=ShadeRGB(sand,15,false);
    assert(shadow[0]<neutral[0] && sun[0]>neutral[0]);
    assert(neutral[0]-neutral[2]>sand[0]-sand[2]);
    const std::uint8_t nightGreen[3]={0,180,0};
    const auto night=ShadeRGB(nightGreen,20,true);
    assert(night[0]==0 && night[2]==0 && night[1]>180);
    assert(MakeTag(245,10)==0 && MakeTag(0,0)==0);
    for(int shade=-32;shade<=31;++shade)
    {
        const auto tag=MakeTag(120,shade);
        assert(TagIndex(tag)==120 && TagShade(tag)==shade);
    }
    assert(SurfaceDetail(42,91,false)==SurfaceDetail(42,91,false));
    bool varied=false;
    for(int i=1;i<32;++i) varied|=SurfaceDetail(i,91,false)!=SurfaceDetail(0,91,false);
    assert(varied);
    GroundSurface field;
    field.Reset(5,5,0,0);
    std::array<std::uint8_t,25> frame{};
    frame.fill(120);
    for(int y=0;y<5;++y) for(int x=0;x<5;++x) field.Write(x,y,MakeTag(120,3));
    assert(field.Match(frame.data(),frame.size(),5,2,2));
    frame[2*5+3]=42; // later sprite/overlay replaces a ground pixel
    assert(!field.Match(frame.data(),frame.size(),5,2,2));
    frame[2*5+3]=120;
    field.Write(3,2,0); // filtered tile overwrites metadata
    assert(!field.Match(frame.data(),frame.size(),5,2,2));
    field.Reset(4,4,0,0);
    field.Mark(sprite,1,0,0,1,&light,false);
    assert(field.At(0,1)==0 && field.At(1,1)!=0 && field.At(2,1)==0);
    field.Mark(sprite,1,0,0,1,nullptr,false);
    assert(field.At(1,1)==0);
    sprite.pop_back();
    DrawGround(sprite,1,0,buffer.data(),buffer.size(),4,0,0,palette,light);
    for(auto b:buffer) assert(b==254);
}
