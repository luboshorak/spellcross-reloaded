// Standalone test tool: pass extracted retail CAN clips as command-line arguments.
// No copyrighted fixtures are bundled.
#include "../source/can_video_decoder.h"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: can_decoder_test file.CAN [file.CAN ...]\n";
        return 2;
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
