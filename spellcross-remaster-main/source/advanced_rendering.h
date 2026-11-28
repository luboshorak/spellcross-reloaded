#pragma once
// Experimental indexed terrain lighting. Original sprite assets remain immutable.
// No wx dependency: the lighting and clipped sprite decoder can be unit tested.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>

namespace spellcross_enhanced
{
    constexpr int minShade = -12;
    constexpr int maxShade = 5;
    constexpr bool Reserved(int index) { return index == 0 || index >= 240; }

    inline int Relief(int west, int east, int north, int south, int center)
    {
        const int occlusion = (std::max)(0, west - center) +
                              (std::max)(0, north - center);
        return std::clamp((east - west) * 2 + (south - north) -
                          occlusion, -5, 5);
    }

    // Stable world-space texture; never depends on the camera or frame time.
    inline std::uint32_t Hash(std::uint32_t x, std::uint32_t y)
    {
        std::uint32_t h = x * 0x9e3779b9u ^ y * 0x85ebca6bu;
        h ^= h >> 16; h *= 0x7feb352du; h ^= h >> 15;
        return h;
    }

    inline int Grain(int x, int y)
    {
        const auto h = Hash(static_cast<std::uint32_t>(x) / 3u,
                            static_cast<std::uint32_t>(y) / 3u);
        return (h & 15u) == 0 ? -1 : (h & 15u) == 1 ? 1 : 0;
    }

    // Soft, asymmetric contact shade in native tile coordinates. The ellipse
    // is restricted to the authored nontransparent pixels of its own tile.
    inline int Contact(int x, int y, int cx, int cy, int rx, int ry, int strength)
    {
        const int dx = x - cx;
        const int dy = y - cy;
        if (rx <= 0 || ry <= 0 || dx <= -rx || dx >= rx || dy <= -ry || dy >= ry)
            return 0;
        const int q = dx * dx * 256 / (rx * rx) + dy * dy * 256 / (ry * ry);
        if (q >= 256) return 0;
        const int falloff = 256 - q;
        return -(strength * falloff * falloff + 32768) / 65536;
    }

    class Palette
    {
        std::array<std::uint8_t, 768> source{};
        std::array<std::array<std::uint8_t, 256>, maxShade - minShade + 1> lut{};
        bool ready = false;
    public:
        void Prepare(const std::uint8_t* rgb)
        {
            if (ready && std::memcmp(source.data(), rgb, source.size()) == 0)
                return;
            std::memcpy(source.data(), rgb, source.size());
            for (int shade = minShade; shade <= maxShade; ++shade)
            {
                auto& row = lut[shade - minShade];
                const int gain = 100 + shade * 4;
                for (int i = 0; i < 256; ++i)
                {
                    row[i] = static_cast<std::uint8_t>(i);
                    if (Reserved(i)) continue;
                    const int r = source[i*3], g = source[i*3+1], b = source[i*3+2];
                    const int luma = (77*r + 150*g + 29*b) / 256;
                    // Modest desaturation and contrast: preserve original terrain identity.
                    const auto channel = [&](int v) {
                        const int neutral = (v * 94 + luma * 6) / 100;
                        return std::clamp(((neutral - 112) * 104 / 100 + 112) * gain / 100, 0, 255);
                    };
                    const int target[3] = {channel(r), channel(g), channel(b)};
                    int best = 2147483647;
                    // Never map into transparency, animated water or reserved UI colours.
                    for (int j = 1; j < 240; ++j)
                    {
                        const int dr = target[0] - source[j*3];
                        const int dg = target[1] - source[j*3+1];
                        const int db = target[2] - source[j*3+2];
                        const int error = 2*dr*dr + 3*dg*dg + db*db;
                        if (error < best) { best = error; row[i] = static_cast<std::uint8_t>(j); }
                    }
                }
            }
            ready = true;
        }

        std::uint8_t Map(std::uint8_t index, int shade) const
        {
            return ready ? lut[std::clamp(shade, minShade, maxShade) - minShade][index] : index;
        }
    };

    struct GroundLight
    {
        int relief = 0;
        bool object = false;
        bool company = false;
        bool air = false;
        int worldX = 0;
        int worldY = 0;
    };

    // Decode the same native row format as Sprite::Render. The enhancement
    // lives inside the ground pass, before markers, objects, units and HUD.
    // Bounds are checked as integers before pointer arithmetic.
    inline void DrawGround(const std::vector<std::uint8_t>& sprite, int rows,
                           int yOffset, std::uint8_t* buffer, std::size_t length,
                           int stride, int originX, int originY,
                           const Palette& palette, const GroundLight& light)
    {
        if (!buffer || stride <= 0 || rows <= 0) return;
        std::size_t cursor = 0;
        for (int y = 0; y < rows; ++y)
        {
            if (sprite.size() - cursor < 2 * sizeof(int)) return;
            int offset, count;
            std::memcpy(&offset, sprite.data() + cursor, sizeof(int));
            std::memcpy(&count, sprite.data() + cursor + sizeof(int), sizeof(int));
            cursor += 2 * sizeof(int);
            if (count < 0 || static_cast<std::size_t>(count) > sprite.size() - cursor) return;
            const auto sy = static_cast<std::int64_t>(originY) + yOffset + y;
            const int cy = rows / 2;
            for (int x = 0; x < count; ++x)
            {
                const auto color = sprite[cursor + x];
                const auto localX = static_cast<std::int64_t>(offset) + x;
                const auto sx = static_cast<std::int64_t>(originX) + localX;
                if (color == 0 || sx < 0 || sx >= stride || sy < 0) continue;
                const auto dest = sy * stride + sx;
                if (dest < 0 || static_cast<std::uint64_t>(dest) >= length) continue;
                int shade = light.relief + Grain(light.worldX + static_cast<int>(localX), light.worldY + yOffset + y);
                int shadow = 0;
                if (light.object)
                    shadow = Contact(static_cast<int>(localX), y, 46, cy + 3, 32, 13, 8);
                if (light.company)
                    shadow = (std::min)(shadow, Contact(static_cast<int>(localX), y, 44, cy + 3, 27, 11, 9));
                if (light.air)
                    shadow = (std::min)(shadow, Contact(static_cast<int>(localX), y, 52, cy + 5, 25, 10, 5));
                buffer[static_cast<std::size_t>(dest)] = palette.Map(color, shade + shadow);
            }
            cursor += static_cast<std::size_t>(count);
        }
    }
    // Version 2: retain the original indexed surface for compositing, but
    // attach a separate light/material field to ground pixels. RGB conversion
    // happens only after the original sprites, overlays and HUD have rendered.
    enum class Material : std::uint8_t { Earth, Sand, Grass, Stone, Road, Wood, Wet, Water, Count };
    using Tag = std::uint32_t;
    using RGB = std::array<std::uint8_t, 3>;
    constexpr int rgbMin = -32, rgbMax = 31;

    inline int Noise(int x, int y, int cell)
    {
        // floor division also works for negative projected elevations
        const int ix = x / cell - (x % cell < 0);
        const int iy = y / cell - (y % cell < 0);
        const int fx = x - ix * cell, fy = y - iy * cell;
        const auto v = [](int a, int b) {
            return static_cast<int>(Hash(static_cast<std::uint32_t>(a),
                                         static_cast<std::uint32_t>(b)) & 31u) - 16;
        };
        const int a = v(ix,iy) * (cell-fx) + v(ix+1,iy) * fx;
        const int b = v(ix,iy+1) * (cell-fx) + v(ix+1,iy+1) * fx;
        return (a * (cell-fy) + b * fy) / (cell*cell);
    }

    inline int SurfaceDetail(int x, int y, bool smooth)
    {
        // Coherent patches plus small mineral/soil grains, not frame noise.
        const int broad = Noise(x,y,24);
        const int medium = Noise(x,y,7);
        const int fine = static_cast<int>(Hash(static_cast<std::uint32_t>(x),
                                              static_cast<std::uint32_t>(y)) & 15u) - 8;
        if (smooth) return broad / 4 + medium / 8;
        return broad / 2 + medium / 2 + fine / 2;
    }

    // Bounded exact world-space noise cache, independent of map and camera.
    class SurfaceCache
    {
        static constexpr int side=32, slots=2048;
        struct Block
        {
            int x=0,y=0;
            bool valid=false;
            std::array<std::int8_t,side*side> rough{},smooth{};
        };
        std::vector<Block> blocks;
    public:
        static constexpr int Side=side;
        const std::int8_t* Row(int x,int y,bool smooth)
        {
            if(blocks.empty()) blocks.resize(slots);
            const int bx=x/side-(x%side<0),by=y/side-(y%side<0);
            auto& block=blocks[Hash(static_cast<std::uint32_t>(bx),
                static_cast<std::uint32_t>(by))&(slots-1)];
            if(!block.valid||block.x!=bx||block.y!=by)
            {
                block.x=bx; block.y=by; block.valid=true;
                block.rough.fill(127); block.smooth.fill(127);
            }
            auto* values=(smooth?block.smooth.data():block.rough.data())+(y-by*side)*side;
            if(values[0]==127)
                for(int i=0;i<side;++i)
                    values[i]=static_cast<std::int8_t>(SurfaceDetail(bx*side+i,y,smooth));
            return values;
        }
        int At(int x,int y,bool smooth)
        {
            return Row(x,y,smooth)[static_cast<unsigned>(x)&(side-1)];
        }
    };
    constexpr bool WaterIndex(int index) { return index>=240&&index<=249; }
    inline Tag MakeTag(std::uint8_t index, int shade, Material material=Material::Earth)
    {
        if (Reserved(index) && !(material==Material::Water&&WaterIndex(index))) return 0;
        return (static_cast<Tag>(material)<<14) |
            static_cast<Tag>(((std::clamp(shade,rgbMin,rgbMax)-rgbMin)<<8) | index);
    }
    inline std::uint8_t TagIndex(Tag tag) { return static_cast<std::uint8_t>(tag & 255); }
    inline int TagShade(Tag tag) { return static_cast<int>((tag >> 8)&63u) + rgbMin; }
    inline Material TagMaterial(Tag tag) { return static_cast<Material>((tag>>14)&7u); }
    inline bool SameMaterial(Tag a, Tag b)
    {
        return a && b && TagMaterial(a)==TagMaterial(b);
    }

    inline int ScaleShade(int shade,int percent)
    {
        return shade*std::clamp(percent,0,100)/100;
    }
    inline RGB ShadeRGB(const std::uint8_t* color, int shade, bool night,int grade=100)
    {
        grade=std::clamp(grade,0,100);
        const int luma = (77*color[0]+150*color[1]+29*color[2])/256;
        const int gain = 100 + shade * (night ? 1 : 2);
        RGB rgb{};
        for (int c=0;c<3;++c)
        {
            // Richer material colour, a mild S-like contrast stretch and
            // warm highlights/cool shadows. Keep the existing night mood.
            const int sat = 100+(night ? 8 : 22)*grade/100;
            const int chroma = luma + (static_cast<int>(color[c])-luma)*sat/100;
            const int contrast = chroma + (luma-112)*(night ? 5 : 12)*grade/10000;
            const int tint = night ? 0 : (c==0 ? shade/3 : c==2 ? -shade/3 : 0)*grade/100;
            rgb[c] = static_cast<std::uint8_t>(std::clamp(contrast*gain/100+tint,0,255));
            if (night && c != 1) rgb[c] = 0; // retain DOS green-only night vision
        }
        return rgb;
    }

    class RGBTable
    {
        std::array<std::array<RGB,256>, rgbMax-rgbMin+1> table{};
        std::array<std::uint8_t,768> lastPalette{};
        bool prepared=false,lastNight=false;
        int lastGrade=-1;
    public:
        void Prepare(const std::uint8_t (*palette)[3], bool night,int grade=100)
        {
            grade=std::clamp(grade,0,100);
            if(prepared && lastNight==night && lastGrade==grade &&
                std::memcmp(lastPalette.data(),palette,lastPalette.size())==0) return;
            std::memcpy(lastPalette.data(),palette,lastPalette.size());
            prepared=true; lastNight=night; lastGrade=grade;
            for(int shade=rgbMin;shade<=rgbMax;++shade)
                for(int index=0;index<256;++index)
                    table[shade-rgbMin][index]=ShadeRGB(palette[index],shade,night,grade);
        }
        const std::uint8_t* Color(Tag tag) const
        {
            return table[TagShade(tag)-rgbMin][TagIndex(tag)].data();
        }
    };

    // Viewport-local metadata. No extra allocation proportional to map size.
    // Filtered tiles clear underlying metadata when they overwrite ground.
    // Cache tile-local contact shadows for all caster combinations.
    class ContactRows
    {
        static constexpr int width=96, halfHeight=24;
        using Row=std::array<std::int8_t,width>;
        std::array<std::array<Row,halfHeight*2+1>,8> table{};
    public:
        ContactRows()
        {
            for(unsigned mask=1;mask<8;++mask)
                for(int y=-halfHeight;y<=halfHeight;++y)
                    for(int x=0;x<width;++x)
                    {
                        int shadow=0;
                        if(mask&1) shadow=(std::min)(shadow,Contact(x,y,47,4,35,15,21));
                        if(mask&2) shadow=(std::min)(shadow,Contact(x,y,46,3,31,13,24));
                        if(mask&4) shadow=(std::min)(shadow,Contact(x,y,54,5,28,12,13));
                        table[mask][y+halfHeight][x]=static_cast<std::int8_t>(shadow);
                    }
        }
        const Row* Get(unsigned mask,int y) const
        {
            return mask&&mask<8&&y>=-halfHeight&&y<=halfHeight?&table[mask][y+halfHeight]:nullptr;
        }
        static int At(const Row* row,int x)
        {
            return row&&x>=0&&x<width?(*row)[x]:0;
        }
    };
    // Ownership describes the topmost indexed write, independently of colour.
    enum class PixelLayer : std::uint8_t { Empty, Ground, Scenery, Unit, Effect, Overlay, Shadow };
    class GroundSurface
    {
        int width=0, height=0, left=0, top=0;
        std::vector<Tag> tags,matched;
        std::vector<std::uint8_t> valid;
        std::vector<PixelLayer> layers;
        std::vector<std::uint8_t> projectedShadow;
    public:
        void Reset(int w,int h,int x,int y)
        {
            width=w; height=h; left=x; top=y;
            tags.assign(static_cast<std::size_t>(w)*h,0);
            layers.assign(tags.size(),PixelLayer::Empty);
            projectedShadow.clear();
        }
        Tag At(int x,int y) const
        {
            x-=left; y-=top;
            if(x<0||y<0||x>=width||y>=height) return 0;
            return tags[static_cast<std::size_t>(y)*width+x];
        }
        PixelLayer LayerAt(int x,int y) const
        {
            x-=left; y-=top;
            if(x<0||y<0||x>=width||y>=height) return PixelLayer::Empty;
            return layers[static_cast<std::size_t>(y)*width+x];
        }
        void Write(int x,int y,Tag tag)
        {
            x-=left; y-=top;
            if(x>=0&&y>=0&&x<width&&y<height)
            {
                const auto i=static_cast<std::size_t>(y)*width+x;
                tags[i]=tag;
                layers[i]=tag?PixelLayer::Ground:PixelLayer::Empty;
            }
        }
        // Invalidate actual opaque writes even when their palette index
        // happens to equal the ground underneath. Null source means solid.
        void OccludeSpan(std::size_t offset,int stride,int count,
                         const std::uint8_t* source=nullptr,int repeat=1,
                         PixelLayer layer=PixelLayer::Effect,int shadowIndex=-1,int skipIndex=-1)
        {
            if(stride<=0||count<=0||repeat<=0||width<=0||height<=0) return;
            // Divide once per span, not once per covered pixel. Most sprite
            // scanlines fit a single destination row; still handle wrapping.
            auto y=offset/static_cast<std::size_t>(stride);
            int x=static_cast<int>(offset%static_cast<std::size_t>(stride));
            const auto bottom=static_cast<std::size_t>((std::max)(0,top+height));
            for(int i=0;i<count;++i)
            {
                const bool opaque=!source||(source[i]!=0&&source[i]!=skipIndex);
                for(int z=0;z<repeat;++z)
                {
                    if(y>=bottom) return;
                    if(opaque&&static_cast<int>(y)>=top&&x>=left&&x-left<width)
                    {
                        const auto dst=static_cast<std::size_t>(static_cast<int>(y)-top)*width+x-left;
                        tags[dst]=0;
                        layers[dst]=source&&source[i]==shadowIndex?PixelLayer::Shadow:layer;
                    }
                    if(++x==stride) { x=0; ++y; }
                }
            }
        }
        // Project opaque sprite scanline runs onto a ground plane. The
        // anchor is the caster's ground contact, not its bounding-box centre.
        // Screen-space height is approximate (the assets have no depth map).
        // Each run preserves source width, transparent gaps and animation.
        void ProjectShadowSpan(std::size_t offset,int stride,int count,
                               const std::uint8_t* source,int repeat,
                               int groundY,int strength,int skipIndex=-1,
                               int direction=0,int lengthPercent=100,int softness=33)
        {
            strength=std::clamp(strength,0,100);
            if(!strength||!source||stride<=0||count<=0||repeat<=0||width<=0||height<=0) return;
            const auto firstY=offset/static_cast<std::size_t>(stride);
            // Bound off-screen writes before converting to signed coordinates.
            if(firstY>static_cast<std::size_t>((std::max)(0,top+height+256))) return;
            if(projectedShadow.empty()) projectedShadow.assign(tags.size(),0);
            const int opacity=24*strength/100;
            direction=std::clamp(direction,0,3);
            lengthPercent=std::clamp(lengthPercent,25,200);
            softness=std::clamp(softness,0,100);
            const int signX=(direction==0||direction==3)?1:-1;
            const int signY=direction<2?1:-1;
            const int radius=softness==0?0:1+(softness>33)+(softness>66);
            const auto run=[&](int x,int y,int length) {
                const int elevation=std::clamp(groundY-y,0,256);
                const int reach=elevation*lengthPercent/100;
                const int px=x+signX*reach,py=y+elevation+signY*reach/2;
                const auto stamp=[&](int row,int begin,int finish,int amount) {
                    if(!amount||row<top||row>=top+height) return;
                    begin=(std::max)(begin,left);finish=(std::min)(finish,left+width);
                    auto* dest=projectedShadow.data()+static_cast<std::size_t>(row-top)*width;
                    for(int sx=begin;sx<finish;++sx)
                        dest[sx-left]=(std::max)(dest[sx-left],static_cast<std::uint8_t>(amount));
                };
                stamp(py,px,px+length,opacity);
                for(int r=1;r<=radius;++r)
                {
                    const int fringe=opacity*softness*(radius+1-r)/(99*radius);
                    stamp(py-r,px-r,px+length+r,fringe);
                    stamp(py+r,px-r,px+length+r,fringe);
                    stamp(py,px-r,px,fringe);
                    stamp(py,px+length,px+length+r,fringe);
                }
            };
            // Source runs may cross a scratch-buffer scanline (legacy raster
            // semantics); split them at its boundary before projecting.
            int y=static_cast<int>(firstY),x=static_cast<int>(offset%stride);
            int runStart=-1;
            for(int i=0;i<count;++i)
            {
                const bool opaque=source[i]!=0&&source[i]!=skipIndex;
                for(int z=0;z<repeat;++z)
                {
                    if(opaque&&runStart<0) runStart=x;
                    if(!opaque&&runStart>=0) { run(runStart,y,x-runStart);runStart=-1; }
                    if(++x==stride)
                    {
                        if(runStart>=0) run(runStart,y,x-runStart);
                        runStart=-1;x=0;++y;
                    }
                }
            }
            if(runStart>=0) run(runStart,y,x-runStart);
        }
        void ApplyShadows()
        {
            if(projectedShadow.empty()) return;
            for(std::size_t i=0;i<tags.size();++i)
            {
                const auto tag=tags[i];
                if(tag&&layers[i]==PixelLayer::Ground&&projectedShadow[i])
                    tags[i]=MakeTag(TagIndex(tag),TagShade(tag)-projectedShadow[i],TagMaterial(tag));
            }
            projectedShadow.clear();
        }
        void Mark(const std::vector<std::uint8_t>& sprite,int rows,int yOffset,
                  int originX,int originY,const GroundLight* light,bool smooth,
                  Material material=Material::Earth,bool calculateShade=true,
                  SurfaceCache* cache=nullptr,int lightingPercent=100,bool animateWater=false)
        {
            static const ContactRows contactRows;
            const unsigned casterMask=light&&calculateShade?
                (unsigned(light->object)|unsigned(light->company)*2|unsigned(light->air)*4):0;
            const int relief=light?light->relief*3:0;
            std::size_t cursor=0;
            for(int y=0;y<rows;++y)
            {
                if(sprite.size()-cursor<2*sizeof(int)) return;
                int offset,count;
                std::memcpy(&offset,sprite.data()+cursor,sizeof(int));
                std::memcpy(&count,sprite.data()+cursor+sizeof(int),sizeof(int));
                cursor+=2*sizeof(int);
                if(count<0||static_cast<std::size_t>(count)>sprite.size()-cursor) return;
                const int sy=originY+yOffset+y;
                if(sy>=top&&sy-top<height)
                {
                    const int start=(std::max)(0,left-originX-offset);
                    const int end=(std::min)(count,left+width-originX-offset);
                    auto* dest=tags.data()+static_cast<std::size_t>(sy-top)*width;
                    const int worldY=light?light->worldY+yOffset+y:0;
                    const auto* shadowRow=contactRows.Get(casterMask,y-rows/2);
                    const std::int8_t* noiseRow=nullptr;
                    int noiseBlock=0;
                    for(int x=start;x<end;++x)
                    {
                        const auto index=sprite[cursor+x];
                        if(!index) continue;
                        const int lx=offset+x,sx=originX+lx;
                        Tag tag=0;
                        if(light&&animateWater&&WaterIndex(index))
                            tag=MakeTag(index,0,Material::Water);
                        else if(light&&!Reserved(index) && !calculateShade)
                            tag=MakeTag(index,0,material);
                        else if(light&&!Reserved(index))
                        {
                            const int worldX=light->worldX+lx;
                            int detail;
                            if(cache)
                            {
                                const int block=worldX-static_cast<int>(static_cast<unsigned>(worldX)&(SurfaceCache::Side-1));
                                if(!noiseRow||noiseBlock!=block)
                                {
                                    noiseRow=cache->Row(worldX,worldY,smooth);
                                    noiseBlock=block;
                                }
                                detail=noiseRow[worldX-block];
                            }
                            else detail=SurfaceDetail(worldX,worldY,smooth);
                            tag=MakeTag(index,ScaleShade(relief+detail+ContactRows::At(shadowRow,lx),lightingPercent),material);
                        }
                        dest[sx-left]=tag;
                        layers[static_cast<std::size_t>(sy-top)*width+sx-left]=PixelLayer::Ground;
                    }
                }
                cursor+=static_cast<std::size_t>(count);
            }
        }
        // Five-point mask computed once on native pixels, then enlarged.
        void Validate(const std::uint8_t* frame,std::size_t length,int stride)
        {
            matched.assign(tags.size(),0);
            valid.assign(tags.size(),0);
            if(!frame||stride<=0||width<3||height<3) return;
            for(int y=0;y<height;++y)
            {
                const int py=top+y;
                if(py<0) continue;
                const auto row=static_cast<std::size_t>(py)*stride;
                const int start=(std::max)(0,-left),end=(std::min)(width,stride-left);
                for(int x=start;x<end;++x)
                {
                    const auto i=static_cast<std::size_t>(y)*width+x;
                    const auto pos=row+left+x;
                    const auto tag=tags[i];
                    valid[i]=pos<length && tag && TagIndex(tag)==frame[pos];
                }
            }
            for(int y=1;y<height-1;++y)
                for(int x=1;x<width-1;++x)
                {
                    const auto i=static_cast<std::size_t>(y)*width+x;
                    if(valid[i]&&valid[i-1]&&valid[i+1]&&valid[i-width]&&valid[i+width])
                        matched[i]=tags[i];
                }
        }
        const Tag* MatchedRow(int y) const
        {
            return y>=0&&y<height&&!matched.empty()?
                matched.data()+static_cast<std::size_t>(y)*width:nullptr;
        }
        // Reject composited pixels AND a one-pixel rim around changes. This
        // is conservative: later art wins over the enhanced ground.
        Tag Match(const std::uint8_t* frame,std::size_t length,int stride,int x,int y) const
        {
            constexpr int dx[]={0,-1,1,0,0},dy[]={0,0,0,-1,1};
            if(!frame||stride<=0) return 0;
            for(int i=0;i<5;++i)
            {
                const int px=x+dx[i],py=y+dy[i];
                if(px<0||px>=stride||py<0) return 0;
                const auto pos=static_cast<std::size_t>(py)*stride+px;
                const Tag tag=At(px,py);
                if(pos>=length||!tag||TagIndex(tag)!=frame[pos]) return 0;
            }
            return At(x,y);
        }
    };

    // Scoped write observer for the original indexed rasterizers. No second
    // drawing pass, no change to palette bytes or game logic. Only the exact
    // battlefield buffer is observed; other previews and HUDs are unaffected.
    struct GroundOcclusion
    {
        GroundSurface* surface=nullptr;
        const std::uint8_t* buffer=nullptr;
        int stride=0;
        PixelLayer layer=PixelLayer::Effect;
        bool originalShadows=true;
        int shadowStrength=0;
        bool castingShadow=false;
        int groundY=0;
        int shadowDirection=0,shadowLength=100,shadowSoftness=33;
        void Span(const std::uint8_t* destination,int count,
                  const std::uint8_t* source=nullptr,int repeat=1,
                  PixelLayer kind=PixelLayer::Empty,int shadowIndex=-1,int skipIndex=-1) const
        {
            const auto start=reinterpret_cast<std::uintptr_t>(buffer);
            const auto write=reinterpret_cast<std::uintptr_t>(destination);
            if(write<start) return;
            const auto offset=static_cast<std::size_t>(write-start);
            const auto effectiveKind=kind==PixelLayer::Empty?layer:kind;
            if(castingShadow&&shadowStrength>0&&
                (effectiveKind==PixelLayer::Scenery||effectiveKind==PixelLayer::Unit))
                surface->ProjectShadowSpan(offset,stride,count,source,repeat,groundY,
                    shadowStrength,shadowIndex,shadowDirection,shadowLength,shadowSoftness);
            surface->OccludeSpan(offset,stride,count,source,repeat,effectiveKind,shadowIndex,skipIndex);
        }
    };
    inline thread_local GroundOcclusion activeGroundOcclusion;
    class GroundOcclusionScope
    {
        GroundOcclusion previous;
    public:
        GroundOcclusionScope(GroundSurface* surface,const std::uint8_t* buffer,int stride,bool originalShadows=true,int shadowStrength=0,
                             int direction=0,int length=100,int softness=33)
            : previous(activeGroundOcclusion)
        {
            activeGroundOcclusion={surface,buffer,stride,PixelLayer::Effect,originalShadows,shadowStrength,false,0,
                direction,length,softness};
        }
        ~GroundOcclusionScope() { activeGroundOcclusion=previous; }
        GroundOcclusionScope(const GroundOcclusionScope&)=delete;
        GroundOcclusionScope& operator=(const GroundOcclusionScope&)=delete;
    };
    class ForegroundLayerScope
    {
        PixelLayer previous;
    public:
        explicit ForegroundLayerScope(PixelLayer kind):previous(activeGroundOcclusion.layer)
        {
            activeGroundOcclusion.layer=kind;
        }
        ~ForegroundLayerScope() { activeGroundOcclusion.layer=previous; }
        ForegroundLayerScope(const ForegroundLayerScope&)=delete;
        ForegroundLayerScope& operator=(const ForegroundLayerScope&)=delete;
    };
    class ShadowCasterScope
    {
        bool previous;
        int previousGround;
    public:
        ShadowCasterScope(int groundY,bool enabled)
            : previous(activeGroundOcclusion.castingShadow),
              previousGround(activeGroundOcclusion.groundY)
        {
            activeGroundOcclusion.castingShadow=enabled&&activeGroundOcclusion.shadowStrength>0;
            activeGroundOcclusion.groundY=groundY;
        }
        ~ShadowCasterScope()
        {
            activeGroundOcclusion.castingShadow=previous;
            activeGroundOcclusion.groundY=previousGround;
        }
        ShadowCasterScope(const ShadowCasterScope&)=delete;
        ShadowCasterScope& operator=(const ShadowCasterScope&)=delete;
    };
    // FSU palette index FD is a shadow operation only when the caller
    // supplies a shadow filter. Never reinterpret that index in other assets.
    inline bool SuppressOriginalShadow(const std::uint8_t* buffer,int stride,bool hasShadowFilter)
    {
        return hasShadowFilter && !activeGroundOcclusion.originalShadows &&
            activeGroundOcclusion.buffer==buffer && activeGroundOcclusion.stride==stride;
    }
    inline const GroundOcclusion* ObserveGround(const std::uint8_t* buffer,int stride)
    {
        return activeGroundOcclusion.surface&&activeGroundOcclusion.buffer==buffer&&
            activeGroundOcclusion.stride==stride?&activeGroundOcclusion:nullptr;
    }

    // V3: procedural materials sampled in the PHYSICAL framebuffer, at four
    // texels per native world pixel. Textures are generated once, not shipped
    // as replacement assets. Periodic lattice noise keeps atlas edges seamless.
    constexpr int textureSize=512, textureMask=textureSize-1, texelsPerPixel=4;
    struct MicroDetail { std::int8_t light=0, tint=0; };

    inline int TileNoise(int x,int y,int cell,std::uint32_t seed)
    {
        x=static_cast<int>(static_cast<unsigned>(x)&textureMask);
        y=static_cast<int>(static_cast<unsigned>(y)&textureMask);
        const int ix=x/cell, iy=y/cell, fx=x%cell, fy=y%cell;
        const int mask=textureSize/cell-1;
        const auto v=[&](int a,int b) {
            return static_cast<int>(Hash(static_cast<unsigned>(a&mask)^seed,
                static_cast<unsigned>(b&mask)^(seed*0x85ebca6bu))&63u)-32;
        };
        // Smoothstep in fixed point: no discontinuous gradients at cell edges.
        const int u=fx*256/cell, w=fy*256/cell;
        const int sx=u*u*(768-2*u)/65536, sy=w*w*(768-2*w)/65536;
        const int a=v(ix,iy)*(256-sx)+v(ix+1,iy)*sx;
        const int b=v(ix,iy+1)*(256-sx)+v(ix+1,iy+1)*sx;
        return (a*(256-sy)+b*sy)/65536;
    }

    // Sparse elliptical raised grains, distributed by a deterministic jittered
    // lattice. Neighbour cells are checked so pebble silhouettes never get cut
    // off by a cell boundary. The vertical radius is foreshortened.
    inline int PebbleHeight(int x,int y,int cell,std::uint32_t seed)
    {
        x=static_cast<int>(static_cast<unsigned>(x)&textureMask);
        y=static_cast<int>(static_cast<unsigned>(y)&textureMask);
        const int ix=x/cell,iy=y/cell,mask=textureSize/cell-1;
        int height=0;
        for(int dy=-1;dy<=1;++dy)
            for(int dx=-1;dx<=1;++dx)
            {
                const auto h=Hash(static_cast<unsigned>((ix+dx)&mask)^seed,
                                  static_cast<unsigned>((iy+dy)&mask));
                if((h&7u)>2u) continue;
                const int cx=(ix+dx)*cell+static_cast<int>((h>>4)&(cell-1));
                const int cy=(iy+dy)*cell+static_cast<int>((h>>12)&(cell-1));
                const int rx=cell/7+static_cast<int>((h>>20)&3u);
                const int ry=(std::max)(2,rx*2/3);
                const int px=x-cx,py=y-cy;
                if(px<=-rx||px>=rx||py<=-ry||py>=ry) continue;
                const int q=px*px*256/(rx*rx)+py*py*256/(ry*ry);
                if(q<256) height=(std::max)(height,(256-q)*24/256);
            }
        return height;
    }

    class TextureBank
    {
        std::array<std::vector<MicroDetail>,static_cast<std::size_t>(Material::Count)> textures;
        void Generate(Material material)
        {
            auto& out=textures[static_cast<std::size_t>(material)];
            if(!out.empty()) return;
            out.resize(textureSize*textureSize);
            std::vector<std::int16_t> height(out.size());
            const std::uint32_t seed=0x9e3779b9u*(1u+static_cast<unsigned>(material));
            const auto idx=[](int x,int y) {
                return static_cast<std::size_t>(static_cast<unsigned>(y)&textureMask)*textureSize+
                    (static_cast<unsigned>(x)&textureMask);
            };
            for(int y=0;y<textureSize;++y)
                for(int x=0;x<textureSize;++x)
                {
                    const int broad=TileNoise(x,y,64,seed);
                    const int medium=TileNoise(x,y,16,seed+1);
                    const int fine=TileNoise(x,y,4,seed+2);
                    const int grain=static_cast<int>(Hash(static_cast<unsigned>(x)^seed,
                        static_cast<unsigned>(y))&15u)-8;
                    int z=0,tint=0;
                    switch(material)
                    {
                        case Material::Sand:
                        {
                            // Wind-rippled sand and scattered mineral grains.
                            const int phase=(y+x/4+broad/2)&31;
                            const int ripple=16-(std::abs)(phase-16);
                            z=broad/2+medium/3+fine/4+ripple/2+grain/3;
                            tint=broad/3+medium/5;
                            break;
                        }
                        case Material::Grass:
                            // Dense clusters with finer, vertically stretched blades.
                            z=broad/2+medium+TileNoise(x*2,y,4,seed+3)/2+grain/3;
                            tint=broad/2+medium/4;
                            break;
                        case Material::Stone:
                            // Angular relief, mineral strata and scattered rock chips.
                            z=broad/2+(std::abs)(medium)+fine/2+
                                PebbleHeight(x,y,32,seed);
                            tint=medium/3;
                            break;
                        case Material::Road:
                            // Compact asphalt/gravel, not deep random hills.
                            z=broad/5+medium/5+fine/3+grain/3;
                            tint=medium/8;
                            break;
                        case Material::Wood:
                            // Long grain along one axis; no new board seams.
                            z=TileNoise(x,y*4,16,seed)/2+fine/4;
                            tint=broad/5;
                            break;
                        case Material::Wet:
                            // Mud and wet surfaces have smoother low-frequency relief.
                            z=broad/4+medium/4+fine/8;
                            tint=medium/8;
                            break;
                        default:
                            z=broad/2+medium/2+fine/3+grain/3+
                                PebbleHeight(x,y,32,seed);
                            tint=broad/3+medium/5;
                            break;
                    }
                    height[idx(x,y)]=static_cast<std::int16_t>(z);
                    out[idx(x,y)].tint=static_cast<std::int8_t>(std::clamp(tint,-20,20));
                }
            // Approximate normal-map lighting from the height field. Light
            // from upper left; negative slopes supply micro self-shadow.
            for(int y=0;y<textureSize;++y)
                for(int x=0;x<textureSize;++x)
                {
                    const int dx=height[idx(x-1,y)]-height[idx(x+1,y)];
                    const int dy=height[idx(x,y-1)]-height[idx(x,y+1)];
                    const int light=dx*2+dy*3+height[idx(x,y)]/4;
                    out[idx(x,y)].light=static_cast<std::int8_t>(std::clamp(light,-30,30));
                }
        }
    public:
        const std::vector<MicroDetail>& Get(Material material)
        {
            Generate(material);
            return textures[static_cast<std::size_t>(material)];
        }
        static MicroDetail Sample(const std::vector<MicroDetail>& texture,int x,int y)
        {
            return texture[static_cast<std::size_t>(static_cast<unsigned>(y)&textureMask)*textureSize+
                (static_cast<unsigned>(x)&textureMask)];
        }
    };

    // World position in quarter-native-pixel units. Use the actual camera
    // origin and combined DPI/zoom; scrolling never shifts material phase.
    inline int TextureCoordinate(int worldOrigin,int physicalPixel,int zoom,int display)
    {
        const auto divisor=static_cast<std::int64_t>(zoom)*display;
        if(divisor<=0) return 0;
        return static_cast<int>(static_cast<std::int64_t>(worldOrigin)*texelsPerPixel+
            static_cast<std::int64_t>(physicalPixel)*10000*texelsPerPixel/divisor);
    }

    inline int TextureStrength(int zoom,int display)
    {
        // Fade microstructure when zoomed out instead of producing moire.
        return std::clamp((zoom*display-5000)/75,0,100);
    }

    // Multi-scale sampling reduces the obvious 512-texel repeat without
    // changing the original single-scale look when V2 is disabled.
    inline MicroDetail BlendMaterialDetail(MicroDetail fine,MicroDetail broad,Material material)
    {
        const int weight=material==Material::Road||material==Material::Wet?2:3;
        return {static_cast<std::int8_t>((fine.light*(5-weight)+broad.light*weight)/5),
                static_cast<std::int8_t>((fine.tint*(5-weight)+broad.tint*weight)/5)};
    }
    inline int WaveTriangle(int phase)
    {
        const int p=static_cast<int>(static_cast<unsigned>(phase)&127u);
        return 64-(std::abs)(p-64);
    }
    inline RGB ApplyWater(RGB color,int x,int y,int phase,int strength,bool night)
    {
        strength=std::clamp(strength,0,100);
        if(!strength) return color;
        // Two travelling ripples in world coordinates. Multiplicative
        // lighting keeps black black and preserves palette cycling underneath.
        const int a=WaveTriangle(x/3+y*2+phase);
        const int b=WaveTriangle(x+y/2-phase*2);
        const int crest=(std::max)(0,a+b-100);
        const int gain=100+((a-32)+(b-32)/2+crest)*strength/240;
        for(int c=0;c<3;++c)
        {
            if(night&&c!=1) continue;
            const int base=color[c];
            const int glint=night?0:crest*strength*(c+1)*(std::min)(base,96)/28800;
            color[c]=static_cast<std::uint8_t>(std::clamp(base*gain/100+glint,0,255));
        }
        return color;
    }
    inline RGB ApplyMicroDetail(RGB color,MicroDetail detail,Material material,int strength,bool night)
    {
        const int gain=100+detail.light*strength/(night?200:100);
        for(int c=0;c<3;++c)
        {
            if(night&&c!=1) { color[c]=0; continue; }
            int tint=0;
            if(!night)
            {
                const int v=detail.tint*strength/100;
                if(material==Material::Grass) tint=c==1?v:c==0?v/3:-v/3;
                else if(material==Material::Sand||material==Material::Earth||material==Material::Wood)
                    tint=c==0?v:c==1?v/2:-v/2;
                else if(material==Material::Stone) tint=c==2?v:c==0?-v/2:0;
            }
            // Scale the added chroma with the original brightness: black
            // stays black and deep shadows are not artificially lifted.
            const int base=color[c];
            color[c]=static_cast<std::uint8_t>(std::clamp(base*gain/100+
                tint*(std::min)(base,96)/96,0,255));
        }
        return color;
    }

    // Procedural edge reconstruction from original indexed art, not a
    // stochastic texture. Samples retain their authored colours. Only a
    // supported diagonal or a low-contrast continuous surface is interpolated.
    struct Subpixel { int index=0, fraction=128; };
    inline Subpixel NativeSample(int physical,int zoom,int display,int extent)
    {
        if(extent<=1) return {};
        const auto scale=static_cast<std::int64_t>(zoom)*display;
        if(scale<=0) return {};
        const auto fixed=(2LL*physical+1)*10000*128/scale;
        if(fixed<=0) return {0,0};
        if(fixed>=static_cast<std::int64_t>(extent)*256) return {extent-1,255};
        return {static_cast<int>(fixed/256),static_cast<int>(fixed%256)};
    }
    inline RGB Blend(RGB a,RGB b,int weight)
    {
        weight=std::clamp(weight,0,256);
        for(int c=0;c<3;++c)
            a[c]=static_cast<std::uint8_t>((a[c]*(256-weight)+b[c]*weight+128)/256);
        return a;
    }
    inline int ColourDistance(const std::uint8_t* a,const std::uint8_t* b)
    {
        const int dr=static_cast<int>(a[0])-b[0];
        const int dg=static_cast<int>(a[1])-b[1];
        const int db=static_cast<int>(a[2])-b[2];
        return (std::abs)(dr)+(std::abs)(dg)+(std::abs)(db);
    }
    // A cached pairwise classification avoids computing colour differences
    // for every output pixel. Reserved/very dark colours are hard barriers.
    class EdgePalette
    {
        std::array<std::array<std::uint8_t,256>,256> distances{};
        std::array<std::uint8_t,240*3> lastPalette{};
        bool prepared=false;
    public:
        void Prepare(const std::uint8_t (*palette)[3])
        {
            // Animated water is reserved, so changes to indices 240-255
            // cannot change the distance table. No rebuild for water cycling.
            if(prepared && std::memcmp(lastPalette.data(),palette,lastPalette.size())==0) return;
            std::memcpy(lastPalette.data(),palette,lastPalette.size());
            prepared=true;
            for(int i=0;i<256;++i) for(int j=0;j<256;++j)
            {
                const auto* a=palette[i]; const auto* b=palette[j];
                const bool darkA=(std::max)({a[0],a[1],a[2]})<20;
                const bool darkB=(std::max)({b[0],b[1],b[2]})<20;
                distances[i][j]=(Reserved(i)||Reserved(j)||darkA||darkB) ? 255 :
                    static_cast<std::uint8_t>((std::min)(254,ColourDistance(a,b)));
            }
        }
        int Distance(std::uint8_t a,std::uint8_t b) const { return distances[a][b]; }
    };
    inline RGB Reconstruct(const std::uint8_t* pixels,int width,int height,
                           Subpixel sx,Subpixel sy,const std::uint8_t (*pal)[3],
                           const EdgePalette& edges)
    {
        const int x=std::clamp(sx.index,0,width-1),y=std::clamp(sy.index,0,height-1);
        const auto* row=pixels+static_cast<std::size_t>(y)*width;
        const auto center=row[x];
        const auto rgb=[&](std::uint8_t i)->RGB { return {pal[i][0],pal[i][1],pal[i][2]}; };
        RGB result=rgb(center);
        if(Reserved(center)||x==0||y==0||x==width-1||y==height-1) return result;
        const int dx=sx.fraction<128?-1:1,dy=sy.fraction<128?-1:1;
        const int fx=(std::abs)(sx.fraction-128),fy=(std::abs)(sy.fraction-128);
        const auto a=row[x+dx],b=row[dy*width+x];
        if(a==center && b==center) return result;
        const auto oppositeA=row[x-dx],oppositeB=row[-dy*width+x];
        const auto diagonal=row[dy*width+x+dx];
        const int da=edges.Distance(center,a),db=edges.Distance(center,b);
        const int ab=edges.Distance(a,b);
        // Two agreeing neighbours trace a diagonal through the pixel corner.
        // Require contrasting opposite sides, so isolated one-pixel details
        // and straight/thin lines do not get rounded away.
        if(ab<=26 && da>=40 && da<255 && db>=40 && db<255 &&
            edges.Distance(a,oppositeA)>35 && edges.Distance(b,oppositeB)>35 &&
            edges.Distance(a,diagonal)<=48)
        {
            const int coverage=std::clamp((fx+fy-115)*3,0,224);
            return Blend(result,Blend(rgb(a),rgb(b),128),coverage);
        }
        // Modest directional interpolation only across compatible colours.
        // Do not blur a silhouette, cross a fog edge, or invent new geometry.
        if(da<=32) result=Blend(result,rgb(a),fx/2);
        if(db<=32) result=Blend(result,rgb(b),fy/2);
        return result;
    }
    inline RGB PreserveLighting(RGB reconstructed,RGB base,RGB original)
    {
        for(int c=0;c<3;++c)
            reconstructed[c]=static_cast<std::uint8_t>(std::clamp(
                static_cast<int>(reconstructed[c])+base[c]-original[c],0,255));
        return reconstructed;
    }

}
