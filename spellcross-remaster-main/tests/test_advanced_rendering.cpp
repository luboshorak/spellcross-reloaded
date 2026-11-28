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
    // Each material survives packing, including the darkest legal shade.
    for(unsigned material=0;material<static_cast<unsigned>(Material::Count);++material)
        for(int shade=rgbMin;shade<=rgbMax;++shade)
        {
            const auto tag=MakeTag(1,shade,static_cast<Material>(material));
            assert(tag && TagIndex(tag)==1 && TagShade(tag)==shade);
            assert(TagMaterial(tag)==static_cast<Material>(material));
        }
    assert(!SameMaterial(MakeTag(120,0,Material::Sand),MakeTag(120,0,Material::Grass)));
    assert(SameMaterial(MakeTag(120,0,Material::Sand),MakeTag(121,1,Material::Sand)));
    assert(!SameMaterial(0,0));
    // Four texels per native pixel; panning by one world pixel corresponds
    // exactly to moving four texels at native zoom.
    assert(TextureCoordinate(100,0,100,100)==400);
    assert(TextureCoordinate(100,1,100,100)==TextureCoordinate(101,0,100,100));
    assert(TextureCoordinate(100,1,200,100)==402);
    assert(TextureCoordinate(100,2,200,100)==404);
    assert(TextureStrength(50,100)==0 && TextureStrength(200,125)==100);
    assert(TileNoise(-1,37,16,1)==TileNoise(textureSize-1,37,16,1));
    assert(PebbleHeight(-1,37,32,1)==PebbleHeight(textureSize-1,37,32,1));
    TextureBank bank;
    const auto& sandTexture=bank.Get(Material::Sand);
    const auto& rockTexture=bank.Get(Material::Stone);
    const auto& grassTexture=bank.Get(Material::Grass);
    assert(sandTexture.size()==textureSize*textureSize);
    assert(&bank.Get(Material::Sand)==&sandTexture);
    bool different=false,subpixel=false,grassDifferent=false;
    for(int y=0;y<64;++y) for(int x=0;x<64;++x)
    {
        auto a=TextureBank::Sample(sandTexture,x,y);
        auto b=TextureBank::Sample(rockTexture,x,y);
        auto g=TextureBank::Sample(grassTexture,x,y);
        auto c=TextureBank::Sample(sandTexture,x+textureSize,y-textureSize);
        auto d=TextureBank::Sample(sandTexture,x+1,y);
        assert(a.light==c.light && a.tint==c.tint);
        different|=a.light!=b.light||a.tint!=b.tint;
        grassDifferent|=a.light!=g.light||a.tint!=g.tint;
        subpixel|=a.light!=d.light||a.tint!=d.tint;
    }
    assert(different && subpixel && grassDifferent);
    RGB soil={140,100,70};
    assert(ApplyMicroDetail(soil,{20,10},Material::Sand,0,false)==soil);
    assert(ApplyMicroDetail(soil,{20,10},Material::Sand,100,false)[0]>soil[0]);
    assert(ApplyMicroDetail(soil,{-20,0},Material::Sand,100,false)[0]<soil[0]);
    const RGB black={0,0,0};
    assert(ApplyMicroDetail(black,{30,20},Material::Earth,100,false)==black);
    const auto n=ApplyMicroDetail({0,140,0},{20,10},Material::Grass,100,true);
    assert(n[0]==0&&n[2]==0&&n[1]>140);
    // Edge reconstruction: preserve a straight boundary and reconstruct a
    // supported diagonal without introducing any random material noise.
    std::uint8_t rgbPal[256][3]{};
    for(int i=0;i<256;++i) for(int c=0;c<3;++c)
        rgbPal[i][c]=static_cast<std::uint8_t>(i);
    EdgePalette edge;
    edge.Prepare(rgbPal);
    assert(edge.Distance(254,120)==255 && edge.Distance(0,120)==255);
    assert(NativeSample(0,200,100,8).index==0);
    assert(NativeSample(1,200,100,8).fraction==192);
    assert(NativeSample(2,200,100,8).index==1);
    std::array<std::uint8_t,9> diagonalArt={80,160,160,80,80,160,80,80,80};
    const auto corner=Reconstruct(diagonalArt.data(),3,3,{1,250},{1,5},rgbPal,edge);
    assert(corner[0]>80 && corner[0]<160);
    std::array<std::uint8_t,9> straightArt={80,80,160,80,80,160,80,80,160};
    assert(Reconstruct(straightArt.data(),3,3,{1,250},{1,5},rgbPal,edge)[0]==80);
    diagonalArt[4]=254;
    assert(Reconstruct(diagonalArt.data(),3,3,{1,250},{1,5},rgbPal,edge)[0]==254);
    const RGB reconstructed={100,110,120}, lit={110,100,90}, raw={100,100,100};
    const RGB expected={110,110,110};
    assert(PreserveLighting(reconstructed,lit,raw)==expected);
    sprite.pop_back();
    DrawGround(sprite,1,0,buffer.data(),buffer.size(),4,0,0,palette,light);
    for(auto b:buffer) assert(b==254);
}
