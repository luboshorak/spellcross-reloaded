#pragma once

// Experimental CAN picture decoder: independent of the game and wxWidgets.
// Palette transform row = 4 * local intensity is still a hypothesis.
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>
#include <algorithm>

namespace spellcross_can {
struct Decoded {
    std::array<uint8_t, 768> palette{};
    std::vector<std::vector<uint8_t>> frames;
    std::vector<int16_t> audio;
};

inline uint32_t ReadLE32(const uint8_t* p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
        (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

inline bool ExpandFrame(const uint8_t* data, size_t size, std::array<uint8_t, 68000>& raw)
{
    std::array<uint8_t, 4096> ring{};
    size_t r = 0xFEE, p = 0, out = 0;
    auto emit = [&](uint8_t c) {
        if (out == raw.size()) return false;
        raw[out++] = c;
        ring[r] = c;
        r = (r + 1) & 4095;
        return true;
    };
    while (p < size) {
        const unsigned flags = data[p++];
        for (unsigned bit = 0; bit < 8 && p < size; ++bit) {
            if (flags & (1u << bit)) {
                if (!emit(data[p++])) return false;
            } else {
                if (size - p < 2) return false;
                const unsigned a = data[p++], b = data[p++];
                const unsigned offset = a | ((b & 240u) << 4);
                for (unsigned k = 0; k < (b & 15u) + 3; ++k)
                    if (!emit(ring[(offset + k) & 4095])) return false;
            }
        }
    }
    return out == raw.size();
}

inline bool Decode(const uint8_t* data, size_t size, Decoded& result)
{
    constexpr size_t header = 0x4414, width = 320, height = 200;
    constexpr size_t samples_per_frame = 735, initial_samples = 44 * samples_per_frame;
    if (!data || size < header || ReadLE32(data + 0x4408) != width ||
        ReadLE32(data + 0x440c) != height || ReadLE32(data + 0x4410) != 4) return false;
    const size_t count = ReadLE32(data);
    // Bound allocation; all known retail CAN clips fit within this limit.
    if (!count || count > 10000 || count > (size - header) / (samples_per_frame + 4)) return false;
    Decoded decoded;
    const uint8_t* tables = data + 4;
    std::copy_n(tables + 65 * 256, decoded.palette.size(), decoded.palette.begin());
    decoded.frames.reserve(count);
    decoded.audio.reserve(count * samples_per_frame);
    std::array<uint8_t, 68000> raw{};
    size_t p = header;
    for (size_t i = 0; i < count; ++i) {
        const size_t audio_size = i == 0 ? initial_samples : samples_per_frame;
        if (size - p < audio_size + 4) return false;
        // Same duration policy as the Python prototype: trim excess tail, not initial samples.
        for (size_t k = 0; k < audio_size && decoded.audio.size() < count * samples_per_frame; ++k)
            decoded.audio.push_back(static_cast<int16_t>((int(data[p + k]) - 128) * 256));
        p += audio_size;
        const size_t packed = ReadLE32(data + p);
        p += 4;
        if (packed > size - p || !ExpandFrame(data + p, packed, raw)) return false;
        p += packed;
        auto& frame = decoded.frames.emplace_back(width * height);
        for (size_t by = 0; by < height / 4; ++by) {
            for (size_t bx = 0; bx < width / 4; ++bx) {
                const uint8_t* block = raw.data() + (by * (width / 4) + bx) * 17;
                for (size_t y = 0; y < 4; ++y) {
                    for (size_t x = 0; x < 4; ++x) {
                        const unsigned intensity = block[1 + y * 4 + x];
                        if (intensity > 16) return false;
                        frame[(by * 4 + y) * width + bx * 4 + x] = tables[intensity * 4 * 256 + block[0]];
                    }
                }
            }
        }
    }
    if (p != size || decoded.audio.size() != count * samples_per_frame) return false;
    result = std::move(decoded);
    return true;
}
}
