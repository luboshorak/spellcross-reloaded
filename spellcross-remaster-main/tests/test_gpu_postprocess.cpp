#include "../source/gpu_postprocess.h"
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
    for(int mode : {0,1,2,10,11,13,14})
    {
        frame.mode=mode;
        bool valid=true;int rows=0;
        if(!gpu.Run(frame,art.data(),art.data(),pal,tags.data(),hud.data(),[&](int,const uint32_t* data){
            ++rows;for(int x=0;x<8;++x) valid &= (data[x]&0xffffffu)==(100u|(50u<<8)|(33u<<16));
        },true)) {std::cerr<<gpu.Error()<<'\n';return 1;}
        assert(valid&&rows==8);
    }
    // A black fog/border index must remain black.
    art.fill(0);frame.mode=14;
    bool valid=true;
    if(!gpu.Run(frame,art.data(),art.data(),pal,tags.data(),hud.data(),[&](int,const uint32_t* data){
        for(int x=0;x<8;++x) valid &= (data[x]&0xffffffu)==0;
    },true)) {std::cerr<<gpu.Error()<<'\n';return 1;}
    assert(valid);
    std::cout<<"D3D11 compute shader dispatch verified\n";
}
