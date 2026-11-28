#pragma once
// Dedicated legacy indexed->BGR path. No experimental metadata, worker
// scheduling or per-pixel dispatch. Bilinear rounding matches the V4 path.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace spellcross_legacy_rgb
{
    struct Axis { int first=0,second=0,weight=0; };
    inline Axis Sample(int target,int scale,int size)
    {
        if(size<=1) return {};
        const std::int64_t n=(2LL*target+1)*100-scale,d=2LL*scale;
        if(n<=0) return {};
        const auto fixed=(n*256+d/2)/d;
        if(fixed>=256LL*(size-1)) return {size-1,size-1,0};
        const int first=static_cast<int>(fixed/256);
        return {first,first+1,static_cast<int>(fixed%256)};
    }
    class Scaler
    {
        using Wide=std::array<std::uint16_t,3>;
        struct Row { int y=-1; std::vector<Wide> colors; };
        std::array<Row,2> rows;
        std::vector<Axis> horizontal,vertical;
        std::vector<int> nearestX,nearestY;
        int w=0,h=0,sw=0,sh=0,scale=0;
        const std::uint8_t* pixels=nullptr;
        const std::uint8_t (*palette)[3]=nullptr;
        bool smooth=false;
        const std::vector<Wide>& GetRow(int y)
        {
            auto& row=rows[static_cast<unsigned>(y)&1u];
            if(row.y==y) return row.colors;
            row.y=y;
            row.colors.resize(w);
            const auto* src=pixels+static_cast<std::size_t>(y)*sw;
            for(int x=0;x<w;++x)
            {
                const auto& a=horizontal[x];
                const auto* l=palette[src[a.first]];
                const auto* r=palette[src[a.second]];
                auto& out=row.colors[x];
                if(l==r||a.weight==0)
                {
                    out={static_cast<std::uint16_t>(l[2]*256),
                         static_cast<std::uint16_t>(l[1]*256),
                         static_cast<std::uint16_t>(l[0]*256)};
                }
                else for(int c=0;c<3;++c)
                    out[c]=static_cast<std::uint16_t>(l[2-c]*(256-a.weight)+r[2-c]*a.weight);
            }
            return row.colors;
        }
    public:
        void Prepare(const std::uint8_t* src,const std::uint8_t (*pal)[3],
                     int sourceWidth,int sourceHeight,int width,int height,int dpi,bool interpolate)
        {
            pixels=src; palette=pal;
            if(w!=width||h!=height||sw!=sourceWidth||sh!=sourceHeight||
                scale!=dpi||smooth!=interpolate)
            {
                w=width; h=height; sw=sourceWidth; sh=sourceHeight; scale=dpi; smooth=interpolate;
                if(smooth)
                {
                    horizontal.resize(w); vertical.resize(h);
                    for(int x=0;x<w;++x) horizontal[x]=Sample(x,scale,sw);
                    for(int y=0;y<h;++y) vertical[y]=Sample(y,scale,sh);
                }
                else
                {
                    nearestX.resize(w); nearestY.resize(h);
                    for(int x=0;x<w;++x) nearestX[x]=std::clamp(x*100/scale,0,sw-1);
                    for(int y=0;y<h;++y) nearestY[y]=std::clamp(y*100/scale,0,sh-1);
                }
            }
            // Contents and palette can change every frame; only coordinates persist.
            for(auto& row:rows) row.y=-1;
        }
        void Write(int y,std::uint8_t* out)
        {
            if(!smooth)
            {
                const auto* src=pixels+static_cast<std::size_t>(nearestY[y])*sw;
                if(scale==100 && w==sw)
                    for(int x=0;x<w;++x)
                    {
                        const auto* c=palette[src[x]];
                        *out++=c[2]; *out++=c[1]; *out++=c[0];
                    }
                else
                    for(int x=0;x<w;++x)
                    {
                        const auto* c=palette[src[nearestX[x]]];
                        *out++=c[2]; *out++=c[1]; *out++=c[0];
                    }
                return;
            }
            const auto& a=vertical[y];
            const auto& top=GetRow(a.first);
            if(a.weight==0||a.first==a.second)
            {
                for(const auto& pixel:top)
                    for(int c=0;c<3;++c) *out++=static_cast<std::uint8_t>((pixel[c]+128)>>8);
                return;
            }
            // Consecutive source rows always occupy distinct cache slots.
            const auto& bottom=GetRow(a.second);
            const unsigned v=a.weight,u=256-v;
            for(int x=0;x<w;++x)
                for(int c=0;c<3;++c)
                    *out++=static_cast<std::uint8_t>((top[x][c]*u+bottom[x][c]*v+32768)>>16);
        }
    };
}
