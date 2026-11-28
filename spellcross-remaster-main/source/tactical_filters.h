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
    // Never renumber persisted filter IDs. Retired IDs deliberately remain holes.
    enum Mode : int { Legacy=0, Nearest=1, SharpBilinear=2, Scale2x=9,
        Cosine=10, Smootherstep=11, RobidouxSharp=13, CubicSharp=14,
        EdgeAdaptive=17, Triangular=18, SoftEPX=19, CRTScanline=20, LastMode=CRTScanline };
    inline constexpr const char* ModeNames[]={
        "Legacy", "Nearest", "Sharp Bilinear", nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, "Scale2x / EPX", "Cosine", "Smootherstep", nullptr, "Robidoux Sharp",
        "Sharp Bicubic", nullptr, nullptr, "Edge-preserving bilinear", "Directional triangle", "Soft EPX", "CRT soft scanlines"};
    static_assert(sizeof(ModeNames)/sizeof(*ModeNames)==LastMode+1);
    inline bool Available(int mode)
    {
        return mode>=0 && mode<=LastMode && ModeNames[mode]!=nullptr;
    }
    inline bool FourTap(int mode)
    {
        return mode==RobidouxSharp || mode==CubicSharp;
    }
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
    // Cubic reconstruction for the two retained sharp kernels.
    inline double CubicBC(double x,double b,double c)
    {
        x=std::abs(x);
        if(x<1.0) return (((12-9*b-6*c)*x+(-18+12*b+6*c))*x*x+6-2*b)/6;
        if(x<2.0) return (((-b-6*c)*x+(6*b+30*c))*x*x+
                         (-12*b-48*c)*x+8*b+24*c)/6;
        return 0.0;
    }
    struct Axis
    {
        std::array<int,4> index{};
        std::array<int,4> weight{};
        int count=1, nearest=0, side=0, phase=128;
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
        out.side=pos<out.nearest?-1:1;
        out.phase=std::clamp(static_cast<int>((pos-out.nearest+0.5)*256),0,255);
        if(!Available(mode)||mode==Nearest||mode==Legacy||mode==Scale2x||mode==SoftEPX)
        {
            out.index[0]=out.nearest;
            out.weight[0]=weightScale;
            return out;
        }
        if(mode==SharpBilinear||mode==EdgeAdaptive||mode==Cosine||mode==Smootherstep||mode==Triangular||mode==CRTScanline)
        {
            out.count=2;
            out.index[0]=base; out.index[1]=(std::min)(base+1,extent-1);
            // Preserve a sharp pixel interior and blend only around its edge.
            const double f=mode==SharpBilinear ?
                std::clamp((fraction-0.5)*(std::max)(1.0,scale)+0.5,0.0,1.0) :
                mode==Cosine ? (1-std::cos(3.14159265358979323846*fraction))*0.5 :
                mode==Smootherstep ? fraction*fraction*fraction*(fraction*(6*fraction-15)+10) : fraction;
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
            weights[i]=mode==RobidouxSharp ?
                CubicBC(pos-raw,0.262014512,0.368992744) : CubicBC(pos-raw,0.0,0.75);
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
        std::vector<RGB> colors,scratch;
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
                // Dedithering reads only original palette colours, not its own output.
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
                    const auto ca=PaletteRGB(palette,a),cb=PaletteRGB(palette,b);
                    const int distance=Distance(ca,cb);
                    if(diagonal>=3 && distance>=12 && distance<=144 &&
                       (std::abs)(Luma(ca)-Luma(cb))<=48)
                        colors[i]=Mean(ca,cb);
                }
            }
            sharpen=std::clamp(sharpen,0,100);
            if(sharpen && w>2 && h>2)
            {
                scratch=colors;
                const auto& source=scratch;
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
        RGB SampleScale2x(const Axis& ax,const Axis& ay) const
        {
            const int x=ax.nearest,y=ay.nearest;
            const auto center=At(x,y);
            if(!Safe(x,y)||x<=0||y<=0||x>=width-1||y>=height-1||!ax.side||!ay.side)
                return center;
            const int dx=ax.side,dy=ay.side;
            const auto a=indices[Index(x+dx,y)],b=indices[Index(x,y+dy)];
            const auto oppositeA=indices[Index(x-dx,y)],oppositeB=indices[Index(x,y-dy)];
            // Standard Scale2x quadrant rule, sampled at the requested zoom.
            if(a==b && a!=oppositeA && b!=oppositeB && !Protected(a))
                return At(x+dx,y);
            return center;
        }
        RGB SampleSoftEPX(const Axis& ax,const Axis& ay) const
        {
            const int x=ax.nearest,y=ay.nearest,dx=ax.side,dy=ay.side;
            const auto center=At(x,y);
            if(!Safe(x,y)||x<=0||y<=0||x>=width-1||y>=height-1||!dx||!dy) return center;
            if(!Safe(x+dx,y)||!Safe(x,y+dy)||!Safe(x-dx,y)||!Safe(x,y-dy)) return center;
            const auto& a=At(x+dx,y);
            const auto& b=At(x,y+dy);
            if(Distance(a,b)>32 || Distance(a,center)<48 || Distance(b,center)<48 ||
               Distance(a,At(x-dx,y))<40 || Distance(b,At(x,y-dy))<40) return center;
            const int coverage=std::clamp(((std::abs)(ax.phase-128)+(std::abs)(ay.phase-128)-100)*3,0,192);
            const auto mean=Mean(a,b);
            RGB result{};
            for(int k=0;k<3;++k)
                result[k]=static_cast<std::uint8_t>((int(center[k])*(256-coverage)+int(mean[k])*coverage+128)/256);
            return result;
        }
        RGB SampleTriangle(const Axis& ax,const Axis& ay) const
        {
            const auto center=At(ax.nearest,ay.nearest);
            if(!Safe(ax.nearest,ay.nearest)||ax.count!=2||ay.count!=2) return center;
            const auto get=[&](int x,int y)->RGB { return Safe(x,y)?At(x,y):center; };
            const RGB a=get(ax.index[0],ay.index[0]),b=get(ax.index[1],ay.index[0]),
                      c=get(ax.index[0],ay.index[1]),d=get(ax.index[1],ay.index[1]);
            const int u=ax.weight[1],v=ay.weight[1],s=weightScale;
            const RGB *p=&a,*q=&b,*r=&d;
            int wp=s-u,wq=u-v,wr=v;
            if(Distance(a,d)<=Distance(b,c))
            {
                if(v>u) { q=&c; wp=s-v; wq=v-u; wr=u; }
            }
            else if(u+v<=s) { q=&b; r=&c; wp=s-u-v; wq=u; wr=v; }
            else { p=&d; q=&b; r=&c; wp=u+v-s; wq=s-v; wr=s-u; }
            RGB result{};
            for(int k=0;k<3;++k)
                result[k]=static_cast<std::uint8_t>(((*p)[k]*wp+(*q)[k]*wq+(*r)[k]*wr+s/2)/s);
            return result;
        }
        RGB SampleCRT(const Axis& ax,const Axis& ay,bool enlarged) const
        {
            auto result=Sample(ax,ay);
            if(!enlarged||!Safe(ax.nearest,ay.nearest)) return result;
            const int edge=(std::abs)(ay.phase-128),gain=256-edge*edge/256;
            for(auto& c:result) c=static_cast<std::uint8_t>((int(c)*gain+128)/256);
            return result;
        }
        RGB SampleEdgeAdaptive(const Axis& ax,const Axis& ay) const
        {
            const auto center=At(ax.nearest,ay.nearest);
            if(!Safe(ax.nearest,ay.nearest)) return center;
            int sum[3]={},total=0;
            for(int j=0;j<ay.count;++j) for(int i=0;i<ax.count;++i)
            {
                const int x=ax.index[i],y=ay.index[j];
                if(!Safe(x,y)) continue;
                const auto& c=At(x,y);
                // Suppress cross-silhouette mixing but interpolate compatible
                // colours in gradients; integer arithmetic on hot path.
                const int similarity=(std::max)(0,96-Distance(center,c));
                const int w=((ax.weight[i]*ay.weight[j])>>12)*similarity;
                total+=w;
                for(int k=0;k<3;++k) sum[k]+=w*c[k];
            }
            if(!total) return center;
            return {static_cast<std::uint8_t>((sum[0]+total/2)/total),
                    static_cast<std::uint8_t>((sum[1]+total/2)/total),
                    static_cast<std::uint8_t>((sum[2]+total/2)/total)};
        }
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

    // Reuse horizontal convolution across consecutive destination rows.
    // The protected-weight term reconstructs the exact original per-pixel
    // fallback (replace protected samples by the current center colour).
    // Only four native rows are cached, regardless of framebuffer height.
    class Scanline
    {
        struct Tap
        {
            std::array<int,3> sum{};
            std::array<int,3> lo{255,255,255},hi{};
            int protectedWeight=0;
            bool protectedLocal=false;
        };
        struct Row { int y=-1; std::vector<Tap> taps; };
        std::array<Row,4> rows;
        std::array<const Row*,4> active{};
        const Image* image=nullptr;
        const std::vector<Axis>* axes=nullptr;
    public:
        void Reset(const Image& src,const std::vector<Axis>& horizontal)
        {
            image=&src; axes=&horizontal;
            for(auto& row:rows) row.y=-1;
        }
        void Begin(const Axis& ay)
        {
            for(int j=0;j<ay.count;++j)
            {
                const int y=ay.index[j];
                auto& row=rows[static_cast<unsigned>(y)%rows.size()];
                if(row.y!=y)
                {
                    row.y=y;
                    row.taps.resize(axes->size());
                    for(std::size_t x=0;x<axes->size();++x)
                    {
                        const auto& ax=(*axes)[x];
                        Tap tap;
                        for(int i=0;i<ax.count;++i)
                        {
                            const bool local=ax.count<=2||i==1||i==2;
                            const int sx=ax.index[i];
                            if(!image->Filterable(sx,y))
                            {
                                tap.protectedWeight+=ax.weight[i];
                                if(local) tap.protectedLocal=true;
                                continue;
                            }
                            const auto& color=image->At(sx,y);
                            for(int k=0;k<3;++k)
                            {
                                tap.sum[k]+=ax.weight[i]*color[k];
                                if(local)
                                {
                                    tap.lo[k]=(std::min)(tap.lo[k],int(color[k]));
                                    tap.hi[k]=(std::max)(tap.hi[k],int(color[k]));
                                }
                            }
                        }
                        row.taps[x]=tap;
                    }
                }
                active[j]=&row;
            }
        }
        RGB Sample(std::size_t x,const Axis& ay) const
        {
            const auto& ax=(*axes)[x];
            const auto center=image->At(ax.nearest,ay.nearest);
            if(!image->Filterable(ax.nearest,ay.nearest)) return center;
            std::int64_t acc[3]={};
            int lo[3]={255,255,255},hi[3]={};
            for(int j=0;j<ay.count;++j)
            {
                const auto& tap=active[j]->taps[x];
                const bool local=ay.count<=2||j==1||j==2;
                for(int k=0;k<3;++k)
                {
                    acc[k]+=std::int64_t(ay.weight[j])*(tap.sum[k]+tap.protectedWeight*center[k]);
                    if(local)
                    {
                        lo[k]=(std::min)(lo[k],tap.lo[k]);
                        hi[k]=(std::max)(hi[k],tap.hi[k]);
                        if(tap.protectedLocal)
                        {
                            lo[k]=(std::min)(lo[k],int(center[k]));
                            hi[k]=(std::max)(hi[k],int(center[k]));
                        }
                    }
                }
            }
            RGB result{};
            constexpr std::int64_t denom=std::int64_t(weightScale)*weightScale;
            for(int k=0;k<3;++k)
                result[k]=static_cast<std::uint8_t>(std::clamp(
                    static_cast<int>((acc[k]+denom/2)/denom),lo[k],hi[k]));
            return result;
        }
    };
    class AxisCache
    {
        int width=0,height=0,nativeWidth=0,nativeHeight=0,zoom=0,display=0,mode=-1;
    public:
        std::vector<Axis> x,y;
        void Prepare(int w,int h,int nw,int nh,int z,int d,int m)
        {
            if(w==width&&h==height&&nw==nativeWidth&&nh==nativeHeight&&
               z==zoom&&d==display&&m==mode) return;
            width=w; height=h; nativeWidth=nw; nativeHeight=nh; zoom=z; display=d; mode=m;
            x.resize(w); y.resize(h);
            for(int i=0;i<w;++i) x[i]=MakeAxis(i,nw,z,d,m);
            for(int i=0;i<h;++i) y[i]=MakeAxis(i,nh,z,d,m);
        }
    };
}
