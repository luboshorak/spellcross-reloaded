#pragma once
#include "pixels.hpp"
#include <cstring>

namespace sc::native_pak {
struct SpriteRows {
    Bytes data;
    int x_min{},x_max{};
    bool transparent{};
};
// Build the existing renderer's decoded scanline representation, not an
// original FSU/PNM/DTA bitstream. Keep the shadow marker for compositing.
inline SpriteRows sprite_rows(const IndexedFrame& f,bool absolute_x,bool opaque_zero_black){
    if(!f.width||!f.height||f.width>2048||f.height>2048||
       f.indices.size()!=std::size_t(f.width)*f.height||f.opacity.size()!=f.indices.size())
        fail("Invalid native sprite dimensions");
    if(absolute_x&&(f.origin_x<-32768||f.origin_x>32767))fail("Sprite origin out of range");
    SpriteRows out;bool have_pixels=false;
    auto put_int=[&](int n){
        auto at=out.data.size();out.data.resize(at+sizeof(n));
        std::memcpy(out.data.data()+at,&n,sizeof(n));
    };
    for(unsigned y=0;y<f.height;++y){
        const auto start=std::size_t(y)*f.width;
        unsigned left=0,right=f.width;
        while(left<right&&!f.opacity[start+left])++left;
        while(right>left&&!f.opacity[start+right-1])--right;
        if(left==right){put_int(0);put_int(0);out.transparent=true;continue;}
        int offset=static_cast<int>(left)+(absolute_x?f.origin_x:0);
        put_int(offset);put_int(static_cast<int>(right-left));
        if(!have_pixels){out.x_min=offset;out.x_max=offset+int(right-left);have_pixels=true;}
        else{out.x_min=std::min(out.x_min,offset);out.x_max=std::max(out.x_max,offset+int(right-left));}
        if(left||right<f.width)out.transparent=true;
        for(unsigned x=left;x<right;++x){
            auto alpha=f.opacity[start+x],value=f.indices[start+x];
            if(alpha!=0&&alpha!=255)fail("Unsupported partial native sprite opacity");
            if(!alpha){out.data.push_back(0);out.transparent=true;}
            else if(!value){
                if(!opaque_zero_black&&!f.opaque_zero_display_index)fail("Opaque zero cannot be represented in engine sprite");
                out.data.push_back(static_cast<std::uint8_t>(f.opaque_zero_display_index.value_or(254)));
            }else out.data.push_back(value);
        }
    }
    return out;
}
struct FsuLayout {
    unsigned static_slopes{},static_azimuths{},animation_slopes{},animation_azimuths{},frames{};
    std::vector<std::vector<std::string>> stat,anim;
};
inline FsuLayout decode_fsu_layout(const J& atlas){
    const auto& j=atlas.at("fsu_layout");schema(j,"spellcross-fsu-layout");
    auto n=[&](const char* key,unsigned max){auto x=j.at(key).num();if(x>max)fail("FSU layout dimension");return static_cast<unsigned>(x);};
    FsuLayout l{n("static_slopes",13),n("static_azimuths",256),n("animation_slopes",13),n("animation_azimuths",256),n("frames_per_sequence",65535),{},{}};
    if(j.at("azimuth_zero_degrees").num()!=90||j.at("azimuth_direction").str()!="clockwise"||
       bool(l.static_slopes)!=bool(l.static_azimuths)||(l.animation_slopes&&l.animation_azimuths)||
       bool(l.animation_slopes||l.animation_azimuths)!=bool(l.frames))
        fail("Invalid FSU grouping convention");
    auto read=[&](const char* name,unsigned rows,unsigned cols){
        const auto& a=j.at(name).arr();if(a.size()!=rows)fail("FSU row count");
        std::vector<std::vector<std::string>> result;
        for(const auto& row:a){
            if(row.arr().size()!=cols)fail("FSU column count");
            std::vector<std::string> ids;for(const auto& id:row.arr())ids.push_back(id.str());
            result.push_back(std::move(ids));
        }
        return result;
    };
    l.stat=read("static_by_slope",l.static_slopes,l.static_azimuths);
    l.anim=read("animation_by_azimuth_or_slope",l.animation_slopes+l.animation_azimuths,l.frames);
    if(l.stat.empty()&&l.anim.empty())fail("Empty FSU layout");
    return l;
}
}
