#include "../source/smacker_video_decoder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void AppendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

std::vector<std::uint8_t> SyntheticSmacker()
{
    // Minimal, independently generated 4x4 SMK2. All four Huffman tables use
    // the format's absent-tree representation, which decodes one black block.
    std::vector<std::uint8_t> bytes{'S', 'M', 'K', '2'};
    AppendU32(bytes, 4); // width
    AppendU32(bytes, 4); // height
    AppendU32(bytes, 1); // frames
    AppendU32(bytes, static_cast<std::uint32_t>(-6666)); // 15.0015 fps
    AppendU32(bytes, 0); // flags
    for (int i = 0; i < 7; ++i) AppendU32(bytes, 0); // max audio buffers
    AppendU32(bytes, 1); // combined Huffman stream size
    for (int i = 0; i < 4; ++i) AppendU32(bytes, 0); // unpacked tree sizes
    for (int i = 0; i < 7; ++i) AppendU32(bytes, 0); // audio metadata
    AppendU32(bytes, 0); // dummy
    AppendU32(bytes, 4); // one four-byte frame chunk
    bytes.push_back(0); // frame type
    bytes.push_back(0); // 4x absent Huffman trees (start/end bits)
    bytes.insert(bytes.end(), 4, 0); // frame data; absent trees consume no bits
    return bytes;
}

void VerifySynthetic()
{
    const auto source = SyntheticSmacker();
    Check(source.size() == 114, "Synthetic SMK layout changed");
    SmackerDecodedVideo decoded;
    std::string error;
    Check(DecodeSmackerVideo(source.data(), source.size(), decoded, error), error.c_str());
    Check(decoded.width == 4 && decoded.height == 4, "Synthetic SMK resolution mismatch");
    Check(decoded.frames.size() == 1 && decoded.frames[0].size() == 16,
        "Synthetic SMK frame count mismatch");
    Check(std::all_of(decoded.frames[0].begin(), decoded.frames[0].end(),
        [](std::uint8_t pixel) { return pixel == 0; }), "Synthetic SMK pixels mismatch");
    Check(decoded.palettes.size() == 1, "Synthetic SMK palette missing");
    Check(std::abs(decoded.fps - 15.0015) < 0.001, "Synthetic SMK frame rate mismatch");
    Check(decoded.audio_channels == 1 && decoded.audio_rate == 11025 &&
        decoded.audio_samples.size() == 735, "Silent SMK playback clock mismatch");
}

void VerifyRetailMovie(const char* path)
{
    std::ifstream input(path, std::ios::binary);
    Check(static_cast<bool>(input), "Cannot open retail SMK integration fixture");
    const std::vector<std::uint8_t> source((std::istreambuf_iterator<char>(input)), {});
    SmackerDecodedVideo decoded;
    std::string error;
    Check(DecodeSmackerVideo(source.data(), source.size(), decoded, error), error.c_str());
    Check(decoded.width == 320 && decoded.height == 200, "Retail SMK resolution mismatch");
    Check(decoded.frames.size() > 1 && decoded.frames.size() == decoded.palettes.size(),
        "Retail SMK frames/palettes missing");
    Check(decoded.audio_channels >= 1 && decoded.audio_rate > 0 &&
        !decoded.audio_samples.empty(), "Retail SMK audio missing");
    std::cout << "PASS retail SMK: " << path << " frames=" << decoded.frames.size()
              << " fps=" << decoded.fps << " audio=" << decoded.audio_channels
              << "ch@" << decoded.audio_rate << "Hz\n";
}
}

int main(int argc, char** argv)
{
    try
    {
        VerifySynthetic();
        for (int i = 1; i < argc; ++i)
            VerifyRetailMovie(argv[i]);
        std::cout << "PASS: native Smacker v2 decoder, frame palettes, audio and silent clock\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
