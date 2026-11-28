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
    sprite.pop_back();
    DrawGround(sprite,1,0,buffer.data(),buffer.size(),4,0,0,palette,light);
    for(auto b:buffer) assert(b==254);
}
