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
}
