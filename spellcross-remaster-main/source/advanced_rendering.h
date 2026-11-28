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
    enum class Material : std::uint8_t { Earth, Sand, Grass, Stone, Road, Wood, Wet, Count };
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

    inline Tag MakeTag(std::uint8_t index, int shade, Material material=Material::Earth)
    {
        if (Reserved(index)) return 0;
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

    inline RGB ShadeRGB(const std::uint8_t* color, int shade, bool night)
    {
        const int luma = (77*color[0]+150*color[1]+29*color[2])/256;
        const int gain = 100 + shade * (night ? 1 : 2);
        RGB rgb{};
        for (int c=0;c<3;++c)
        {
            // Richer material colour, a mild S-like contrast stretch and
            // warm highlights/cool shadows. Keep the existing night mood.
            const int sat = night ? 108 : 122;
            const int chroma = luma + (static_cast<int>(color[c])-luma)*sat/100;
            const int contrast = chroma + (luma-112)*(night ? 5 : 12)/100;
            const int tint = night ? 0 : (c==0 ? shade/3 : c==2 ? -shade/3 : 0);
            rgb[c] = static_cast<std::uint8_t>(std::clamp(contrast*gain/100+tint,0,255));
            if (night && c != 1) rgb[c] = 0; // retain DOS green-only night vision
        }
        return rgb;
    }

    class RGBTable
    {
        std::array<std::array<RGB,256>, rgbMax-rgbMin+1> table{};
    public:
        void Prepare(const std::uint8_t (*palette)[3], bool night)
        {
            for(int shade=rgbMin;shade<=rgbMax;++shade)
                for(int index=0;index<256;++index)
                    table[shade-rgbMin][index]=ShadeRGB(palette[index],shade,night);
        }
        const std::uint8_t* Color(Tag tag) const
        {
            return table[TagShade(tag)-rgbMin][TagIndex(tag)].data();
        }
    };

    // Viewport-local metadata. No extra allocation proportional to map size.
    // Filtered tiles clear underlying metadata when they overwrite ground.
    class GroundSurface
    {
        int width=0, height=0, left=0, top=0;
        std::vector<Tag> tags;
    public:
        void Reset(int w,int h,int x,int y)
        {
            width=w; height=h; left=x; top=y;
            tags.assign(static_cast<std::size_t>(w)*h,0);
        }
        Tag At(int x,int y) const
        {
            x-=left; y-=top;
            if(x<0||y<0||x>=width||y>=height) return 0;
            return tags[static_cast<std::size_t>(y)*width+x];
        }
        void Write(int x,int y,Tag tag)
        {
            x-=left; y-=top;
            if(x>=0&&y>=0&&x<width&&y<height)
                tags[static_cast<std::size_t>(y)*width+x]=tag;
        }
        void Mark(const std::vector<std::uint8_t>& sprite,int rows,int yOffset,
                  int originX,int originY,const GroundLight* light,bool smooth,
                  Material material=Material::Earth)
        {
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
                    for(int x=0;x<count;++x)
                    {
                        const auto index=sprite[cursor+x];
                        if(!index) continue;
                        const int lx=offset+x,sx=originX+lx;
                        if(sx<left||sx-left>=width) continue;
                        Tag tag=0;
                        if(light&&!Reserved(index))
                        {
                            int shade=light->relief*3+
                                SurfaceDetail(light->worldX+lx,light->worldY+yOffset+y,smooth);
                            const int cy=rows/2;
                            int shadow=0;
                            if(light->object)
                                shadow=(std::min)(shadow,Contact(lx,y,47,cy+4,35,15,21));
                            if(light->company)
                                shadow=(std::min)(shadow,Contact(lx,y,46,cy+3,31,13,24));
                            if(light->air)
                                shadow=(std::min)(shadow,Contact(lx,y,54,cy+5,28,12,13));
                            tag=MakeTag(index,shade+shadow,material);
                        }
                        Write(sx,sy,tag);
                    }
                }
                cursor+=static_cast<std::size_t>(count);
            }
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

}
