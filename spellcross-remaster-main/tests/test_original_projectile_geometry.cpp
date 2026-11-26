#include "../source/original_projectile_geometry.h"
#include <cassert>
#include <cstdint>
#include <cstring>

int main(int argc,char** argv) {
    using namespace original_projectile;
    const bool legacy=argc>1 && std::strcmp(argv[1],"fsa")==0;
    assert(enabled()!=legacy);
    assert(air_height()==(legacy?150:120));
    // FNV-1a of 1,664 raw bytes at EN EXE offset 0x1894b8.
    std::uint32_t hash=2166136261u;
    for(const auto& row:offsets) for(const auto& p:row) {
        for(int v:{p.x,p.y}) for(int k=0;k<4;++k)
            hash=(hash^((static_cast<std::uint32_t>(v)>>(8*k))&255u))*16777619u;
    }
    assert(hash==0xbe15a065u);
    for(int s=0;s<13;++s) for(int d=0;d<16;++d) {
        const auto p=offset(s,false,d),a=offset(s,true,d);
        assert(a.x==offsets[0][d].x && a.y==offsets[0][d].y);
        auto local=local_origin(s,false,d);
        assert(local.x==40+p.x && local.y==center_y(s)+p.y);
    }
    assert(center_y(0)==24 && center_y(12)==33);
    assert(offset(-1,false,0).y==-31 && offset(13,false,0).y==-31);
    assert(tile_x(1,0)==120 && tile_x(1,1)==80);
    auto ground=tile_center(1,2,1,1,-7,false);
    auto air=tile_center(1,2,1,1,-7,true);
    assert(ground.x==160 && ground.y==56 && air.y==ground.y-120);
    const Point vectors[16]={{0,-100},{69,-100},{167,-100},{100,-25},{100,0},{100,25},{167,100},{69,100},{0,100},{-69,100},{-167,100},{-100,25},{-100,0},{-100,-25},{-167,-100},{-69,-100}};
    for(int d=0;d<16;++d) {
        assert(direction(vectors[d])==d);
        assert(from_angle(angle(d))==d);
        assert(graphic_direction(d,true)==(d+4)%16);
        assert(graphic_direction(d,false)==d);
        for(int current=0;current<16;++current) {
            const int turn=turn_delta(d,current,16);
            assert(std::abs(turn)<=8 && wrap(current+turn)==d);
            if(std::abs(d-current)==8) assert(turn==d-current);
        }
    }
    assert(direction({0,0},5)==5);
    assert(direction({10000,-1193})==4);
    assert(direction({10000,-1194})==3);
    auto t=trajectory({36,-11},{100,0},{100,0});
    assert(flight_ticks(t)==6);
    auto p=position(t,1);assert(p.x==46 && p.y==-11);
    p=position(t,999);assert(p.x==86 && p.y==-11);
    auto u=trajectory({5,40},{30,100},{30,100});
    assert(flight_ticks(u)==5);
    p=position(u,2);assert(p.x==11 && p.y==60);
    auto reverse=trajectory({-36,0},{-100,-20},{-100,-20});
    p=position(reverse,2);assert(p.x==-56 && p.y==-4);
    auto near=trajectory({36,0},{20,0},{20,0});
    assert(flight_ticks(near)==1 && position(near,10).x==36);
    auto zero=trajectory({0,0},{0,0},{0,0});
    assert(flight_ticks(zero)==1 && position(zero,1).x==0);
}
