#pragma once
// Experimental indexed terrain lighting. Original sprite assets remain immutable.
// No wx dependency: the lighting and clipped sprite decoder can be unit tested.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
    using Tag = std::uint16_t;
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

    inline Tag MakeTag(std::uint8_t index, int shade)
    {
        if (Reserved(index)) return 0;
        return static_cast<Tag>(((std::clamp(shade,rgbMin,rgbMax)-rgbMin+1)<<8) | index);
    }
    inline std::uint8_t TagIndex(Tag tag) { return static_cast<std::uint8_t>(tag & 255); }
    inline int TagShade(Tag tag) { return static_cast<int>(tag >> 8) + rgbMin - 1; }

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
                  int originX,int originY,const GroundLight* light,bool smooth)
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
                            tag=MakeTag(index,shade+shadow);
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

}
