#pragma once
// CPU equivalents of classic shader-style image filters. No OpenGL context,
// third-party shader code, replacement sprites or AI assets are required.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace spellcross_filters
{
    using RGB=std::array<std::uint8_t,3>;
    enum Mode : int { Legacy=0, Nearest=1, SharpBilinear=2, CatmullRom=3, Lanczos2=4 };
    constexpr int weightScale=4096;
    inline bool Protected(std::uint8_t i)
    {
        // Keep black map borders, status/selection colours and animated
        // water indices on their original rendering path.
        return i==0 || i>=228;
    }
    inline RGB PaletteRGB(const std::uint8_t (*palette)[3],std::uint8_t i)
    {
        return {palette[i][0],palette[i][1],palette[i][2]};
    }
    inline int Luma(const RGB& c)
    {
        return (77*c[0]+150*c[1]+29*c[2]+128)/256;
    }
    inline int Distance(const RGB& a,const RGB& b)
    {
        return (std::abs)(int(a[0])-b[0])+(std::abs)(int(a[1])-b[1])+
            (std::abs)(int(a[2])-b[2]);
    }
    inline RGB Mean(RGB a,const RGB& b)
    {
        for(int c=0;c<3;++c) a[c]=static_cast<std::uint8_t>((int(a[c])+b[c]+1)/2);
        return a;
    }
    inline double Cubic(double x)
    {
        x=std::abs(x);
        if(x<1.0) return ((1.5*x-2.5)*x)*x+1.0;
        if(x<2.0) return ((-0.5*x+2.5)*x-4.0)*x+2.0;
        return 0.0;
    }
    inline double Sinc(double x)
    {
        if(std::abs(x)<1e-8) return 1.0;
        constexpr double pi=3.14159265358979323846;
        return std::sin(pi*x)/(pi*x);
    }
    inline double Lanczos(double x)
    {
        return std::abs(x)<2.0 ? Sinc(x)*Sinc(x/2.0) : 0.0;
    }
    struct Axis
    {
        std::array<int,4> index{};
        std::array<int,4> weight{};
        int count=1, nearest=0;
    };
    inline Axis MakeAxis(int target,int extent,int zoom,int display,int mode)
    {
        Axis out;
        if(extent<=1||zoom<=0||display<=0)
        {
            out.weight[0]=weightScale;
            return out;
        }
        const double scale=double(zoom)*display/10000.0;
        const double pos=std::clamp((target+0.5)/scale-0.5,0.0,double(extent-1));
        const int base=static_cast<int>(std::floor(pos));
        const double fraction=pos-base;
        out.nearest=std::clamp(static_cast<int>(std::floor(pos+0.5)),0,extent-1);
        if(mode==Nearest||mode==Legacy)
        {
            out.index[0]=out.nearest;
            out.weight[0]=weightScale;
            return out;
        }
        if(mode==SharpBilinear)
        {
            out.count=2;
            out.index[0]=base; out.index[1]=(std::min)(base+1,extent-1);
            // Preserve a sharp pixel interior and blend only around its edge.
            const double f=std::clamp((fraction-0.5)*(std::max)(1.0,scale)+0.5,0.0,1.0);
            out.weight[1]=static_cast<int>(std::lround(f*weightScale));
            out.weight[0]=weightScale-out.weight[1];
            return out;
        }
        out.count=4;
        std::array<double,4> weights{};
        double sum=0;
        for(int i=0;i<4;++i)
        {
            const int raw=base+i-1;
            out.index[i]=std::clamp(raw,0,extent-1);
            weights[i]=mode==Lanczos2 ? Lanczos(pos-raw) : Cubic(pos-raw);
            sum+=weights[i];
        }
        int total=0;
        for(int i=0;i<4;++i)
        {
            out.weight[i]=static_cast<int>(std::lround(weights[i]*weightScale/sum));
            total+=out.weight[i];
        }
        out.weight[1]+=weightScale-total; // exact DC gain for flat colours
        return out;
    }

    class Image
    {
        int width=0,height=0;
        const std::uint8_t* indices=nullptr;
        std::vector<RGB> colors;
        std::size_t Index(int x,int y) const { return static_cast<std::size_t>(y)*width+x; }
        bool Safe(int x,int y) const { return !Protected(indices[Index(x,y)]); }
    public:
        void Prepare(const std::uint8_t* pixels,int w,int h,
                     const std::uint8_t (*palette)[3],bool dedither,int sharpen)
        {
            width=w; height=h; indices=pixels;
            colors.resize(static_cast<std::size_t>(w)*h);
            for(std::size_t i=0;i<colors.size();++i) colors[i]=PaletteRGB(palette,pixels[i]);
            if(dedither && w>2 && h>2)
            {
                auto source=colors;
                for(int y=1;y<h-1;++y) for(int x=1;x<w-1;++x)
                {
                    const auto i=Index(x,y);
                    const auto a=pixels[i],b=pixels[Index(x-1,y)];
                    if(Protected(a)||Protected(b)||a==b) continue;
                    // Deliberately conservative AB/BA pattern recognition:
                    // exact alternating palette indices, both axes, at least
                    // three diagonal confirmations, never a protected index.
                    if(pixels[Index(x+1,y)]!=b || pixels[Index(x,y-1)]!=b ||
                       pixels[Index(x,y+1)]!=b) continue;
                    int diagonal=0;
                    for(int dy : {-1,1}) for(int dx : {-1,1})
                        diagonal+=pixels[Index(x+dx,y+dy)]==a;
                    const auto& ca=source[i]; const auto& cb=source[Index(x-1,y)];
                    const int distance=Distance(ca,cb);
                    if(diagonal>=3 && distance>=12 && distance<=144 &&
                       (std::abs)(Luma(ca)-Luma(cb))<=48)
                        colors[i]=Mean(ca,cb);
                }
            }
            sharpen=std::clamp(sharpen,0,100);
            if(sharpen && w>2 && h>2)
            {
                auto source=colors;
                for(int y=1;y<h-1;++y) for(int x=1;x<w-1;++x)
                {
                    if(!Safe(x,y)||!Safe(x-1,y)||!Safe(x+1,y)||!Safe(x,y-1)||!Safe(x,y+1)) continue;
                    const auto i=Index(x,y);
                    const auto &c=source[i],&l=source[Index(x-1,y)],&r=source[Index(x+1,y)],
                        &u=source[Index(x,y-1)],&d=source[Index(x,y+1)];
                    const int lc=Luma(c),ll=Luma(l),lr=Luma(r),lu=Luma(u),ld=Luma(d);
                    const int lo=(std::min)({lc,ll,lr,lu,ld}),hi=(std::max)({lc,ll,lr,lu,ld});
                    // Limit strong-edge enhancement and prevent ringing.
                    const int contrast=hi-lo;
                    if(contrast<3 || contrast>96) continue;
                    const int adapt=(std::min)(64,96-contrast);
                    for(int k=0;k<3;++k)
                    {
                        const int delta=(4*int(c[k])-l[k]-r[k]-u[k]-d[k])*sharpen*adapt/(400*64);
                        const int low=(std::min)({c[k],l[k],r[k],u[k],d[k]});
                        const int high=(std::max)({c[k],l[k],r[k],u[k],d[k]});
                        // Do not create a colour channel absent in the source;
                        // this also preserves the original green night palette.
                        const int allowance=high==low?0:4;
                        colors[i][k]=static_cast<std::uint8_t>(std::clamp(
                            int(c[k])+delta,(std::max)(0,low-allowance),(std::min)(255,high+allowance)));
                    }
                }
            }
        }
        const RGB& At(int x,int y) const { return colors[Index(x,y)]; }
        bool Filterable(int x,int y) const { return Safe(x,y); }
        RGB Sample(const Axis& ax,const Axis& ay) const
        {
            const auto center=At(ax.nearest,ay.nearest);
            if(!Safe(ax.nearest,ay.nearest)) return center;
            if(ax.count==1 && ay.count==1) return center;
            std::int64_t acc[3]={};
            int low[3]={255,255,255},high[3]={0,0,0};
            for(int j=0;j<ay.count;++j) for(int i=0;i<ax.count;++i)
            {
                const int x=ax.index[i],y=ay.index[j];
                const auto& c=Safe(x,y)?At(x,y):center;
                const auto w=std::int64_t(ax.weight[i])*ay.weight[j];
                for(int k=0;k<3;++k)
                {
                    acc[k]+=w*c[k];
                    // Restrict overshoot to the local 2x2 neighbourhood.
                    if((ax.count<=2 || (i==1||i==2)) &&
                       (ay.count<=2 || (j==1||j==2)))
                    {
                        low[k]=(std::min)(low[k],int(c[k]));
                        high[k]=(std::max)(high[k],int(c[k]));
                    }
                }
            }
            RGB result{};
            constexpr std::int64_t denom=std::int64_t(weightScale)*weightScale;
            for(int k=0;k<3;++k)
                result[k]=static_cast<std::uint8_t>(std::clamp(
                    static_cast<int>((acc[k]+denom/2)/denom),low[k],high[k]));
            return result;
        }
    };
}
