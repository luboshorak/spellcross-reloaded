// Run a synthetic fixture, or pass extracted retail CAN clips as arguments.
// No copyrighted fixtures are bundled.
#include "../source/can_video_decoder.h"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv)
{
    if (argc < 2) {
        // One invented frame with uniform base index 7 and intensity 8.
        // Literal LZSS groups exercise header, palette lookup, blocks and PCM.
        std::vector<uint8_t> data(0x4414 + 44 * 735, 0);
        auto set32 = [&](size_t at, uint32_t n) {
            for (unsigned j = 0; j < 4; ++j)
                data[at + j] = static_cast<uint8_t>(n >> (j * 8));
        };
        set32(0, 1);
        set32(0x4408, 320);
        set32(0x440c, 200);
        set32(0x4410, 4);
        data[4 + 32 * 256 + 7] = 42;
        data[4 + 65 * 256 + 42 * 3] = 123;
        data[0x4414] = 255;
        const size_t length = data.size();
        data.resize(length + 4);
        for (size_t n = 0; n < 68000; n += 8) {
            data.push_back(255);
            for (size_t k = 0; k < 8; ++k)
                data.push_back((n + k) % 17 == 0 ? 7 : 8);
        }
        set32(length, static_cast<uint32_t>(data.size() - length - 4));
        spellcross_can::Decoded decoded;
        if (!spellcross_can::Decode(data.data(), data.size(), decoded) ||
            decoded.frames.size() != 1 || decoded.frames[0].size() != 64000 ||
            decoded.palette[42 * 3] != 123 || decoded.audio.size() != 735 ||
            decoded.audio[0] != 32512 ||
            !std::all_of(decoded.frames[0].begin(), decoded.frames[0].end(),
                [](uint8_t p) { return p == 42; })) return 1;
        spellcross_can::Decoded invalid;
        if (spellcross_can::Decode(data.data(), data.size() - 1, invalid)) return 1;
        std::cout << "Synthetic CAN fixture OK\n";
        return 0;
    }
    for (int i = 1; i < argc; ++i) {
        std::ifstream file(argv[i], std::ios::binary);
        if (!file) return 1;
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
        spellcross_can::Decoded decoded;
        if (!spellcross_can::Decode(data.data(), data.size(), decoded)) {
            std::cerr << argv[i] << ": decode failed\n";
            return 1;
        }
        if (decoded.frames.size() != spellcross_can::ReadLE32(data.data()) ||
            decoded.audio.size() != decoded.frames.size() * 735) return 1;
        for (const auto& frame : decoded.frames)
            if (frame.size() != 320 * 200) return 1;
        spellcross_can::Decoded invalid;
        if (spellcross_can::Decode(data.data(), data.size() - 1, invalid)) return 1;
        std::cout << argv[i] << ": " << decoded.frames.size() << " frames OK\n";
    }
    return 0;
}
