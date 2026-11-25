#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct SmackerDecodedVideo
{
    int width = 0;
    int height = 0;
    double fps = 0.0;
    std::vector<std::vector<std::uint8_t>> frames;
    std::vector<std::array<std::uint8_t, 256 * 3>> palettes;
    std::vector<std::int16_t> audio_samples;
    int audio_channels = 0;
    int audio_rate = 0;
};

// Decode a complete retail Smacker v2/v4 movie held in an FS archive entry.
// The output owns all frames, per-frame palettes and interleaved PCM samples.
bool DecodeSmackerVideo(const std::uint8_t* data, std::size_t size,
    SmackerDecodedVideo& output, std::string& error);
