#include "smacker_video_decoder.h"

#include "../../third_party/libsmacker/smacker.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

namespace
{
constexpr std::size_t kMinimumSmackerBytes = 104;
constexpr unsigned long kMaximumDimension = 4096;
constexpr unsigned long kMaximumFrames = 100000;
constexpr std::size_t kMaximumDecodedPixels = 512u * 1024u * 1024u;
constexpr std::size_t kMaximumAudioSamples = 512u * 1024u * 1024u / sizeof(std::int16_t);

struct SmackerCloser
{
    void operator()(smk value) const
    {
        if (value)
            smk_close(value);
    }
};

using SmackerHandle = std::unique_ptr<smk_t, SmackerCloser>;

bool IsSmackerSignature(const std::uint8_t* data)
{
    return data[0] == 'S' && data[1] == 'M' && data[2] == 'K' &&
        (data[3] == '2' || data[3] == '4');
}
}

bool DecodeSmackerVideo(const std::uint8_t* data, std::size_t size,
    SmackerDecodedVideo& output, std::string& error)
{
    output = SmackerDecodedVideo{};
    error.clear();

    if (!data || size < kMinimumSmackerBytes ||
        size > static_cast<std::size_t>((std::numeric_limits<unsigned long>::max)()))
    {
        error = "invalid Smacker buffer size";
        return false;
    }
    if (!IsSmackerSignature(data))
    {
        error = "missing SMK2/SMK4 signature";
        return false;
    }

    SmackerHandle movie(smk_open_memory(data, static_cast<unsigned long>(size)));
    if (!movie)
    {
        error = "libsmacker rejected the movie stream";
        return false;
    }

    unsigned long frame_count = 0;
    unsigned long width = 0;
    unsigned long height = 0;
    double microseconds_per_frame = 0.0;
    unsigned char y_scale = SMK_FLAG_Y_NONE;
    if (smk_info_all(movie.get(), nullptr, &frame_count, &microseconds_per_frame) != 0 ||
        smk_info_video(movie.get(), &width, &height, &y_scale) != 0 ||
        frame_count == 0 || frame_count > kMaximumFrames ||
        width == 0 || height == 0 || width > kMaximumDimension || height > kMaximumDimension ||
        microseconds_per_frame <= 0.0)
    {
        error = "invalid Smacker video metadata";
        return false;
    }
    if (y_scale != SMK_FLAG_Y_NONE)
    {
        error = "unsupported Smacker vertical scaling mode";
        return false;
    }

    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
    if (pixel_count > kMaximumDecodedPixels ||
        static_cast<std::size_t>(frame_count) > kMaximumDecodedPixels / pixel_count)
    {
        error = "decoded Smacker video exceeds the safety limit";
        return false;
    }

    unsigned char track_mask = 0;
    unsigned char channels[7]{};
    unsigned char bit_depth[7]{};
    unsigned long rates[7]{};
    if (smk_info_audio(movie.get(), &track_mask, channels, bit_depth, rates) != 0)
    {
        error = "invalid Smacker audio metadata";
        return false;
    }

    int audio_track = -1;
    for (int candidate = 0; candidate < 7; ++candidate)
    {
        if ((track_mask & (1u << candidate)) == 0)
            continue;
        if ((channels[candidate] == 1 || channels[candidate] == 2) &&
            (bit_depth[candidate] == 8 || bit_depth[candidate] == 16) &&
            rates[candidate] >= 4000 && rates[candidate] <= 192000)
        {
            audio_track = candidate;
            break;
        }
    }

    if (smk_enable_video(movie.get(), 1) != 0 ||
        (audio_track >= 0 && smk_enable_audio(movie.get(),
            static_cast<unsigned char>(audio_track), 1) != 0))
    {
        error = "cannot enable Smacker video/audio tracks";
        return false;
    }

    output.width = static_cast<int>(width);
    output.height = static_cast<int>(height);
    output.fps = 1000000.0 / microseconds_per_frame;
    output.frames.reserve(static_cast<std::size_t>(frame_count));
    output.palettes.reserve(static_cast<std::size_t>(frame_count));
    if (audio_track >= 0)
    {
        output.audio_channels = channels[audio_track];
        output.audio_rate = static_cast<int>(rates[audio_track]);
    }

    for (unsigned long index = 0; index < frame_count; ++index)
    {
        const int status = index == 0 ? smk_first(movie.get()) : smk_next(movie.get());
        if (status == SMK_ERROR || status == SMK_DONE)
        {
            error = "Smacker frame decoding failed at frame " + std::to_string(index);
            return false;
        }

        const unsigned char* pixels = smk_get_video(movie.get());
        const unsigned char* palette = smk_get_palette(movie.get());
        if (!pixels || !palette)
        {
            error = "Smacker decoder returned an empty frame";
            return false;
        }
        output.frames.emplace_back(pixels, pixels + pixel_count);
        output.palettes.emplace_back();
        std::memcpy(output.palettes.back().data(), palette,
            output.palettes.back().size());

        if (audio_track < 0)
            continue;
        const unsigned long audio_bytes = smk_get_audio_size(movie.get(),
            static_cast<unsigned char>(audio_track));
        const unsigned char* audio = smk_get_audio(movie.get(),
            static_cast<unsigned char>(audio_track));
        const std::size_t bytes_per_sample = bit_depth[audio_track] / 8u;
        const std::size_t bytes_per_frame = bytes_per_sample * channels[audio_track];
        if ((audio_bytes != 0 && !audio) || audio_bytes % bytes_per_frame != 0)
        {
            error = "invalid decoded Smacker audio chunk";
            return false;
        }
        const std::size_t scalar_samples = audio_bytes / bytes_per_sample;
        if (scalar_samples > kMaximumAudioSamples - output.audio_samples.size())
        {
            error = "decoded Smacker audio exceeds the safety limit";
            return false;
        }
        const std::size_t old_size = output.audio_samples.size();
        output.audio_samples.resize(old_size + scalar_samples);
        for (std::size_t sample = 0; sample < scalar_samples; ++sample)
        {
            if (bit_depth[audio_track] == 16)
            {
                const std::uint16_t value = static_cast<std::uint16_t>(audio[sample * 2]) |
                    (static_cast<std::uint16_t>(audio[sample * 2 + 1]) << 8);
                output.audio_samples[old_size + sample] = static_cast<std::int16_t>(value);
            }
            else
            {
                output.audio_samples[old_size + sample] = static_cast<std::int16_t>(
                    (static_cast<int>(audio[sample]) - 128) << 8);
            }
        }
    }

    // The legacy player uses the sound stream as its playback clock. Silent
    // logo/transition SMKs therefore need an equally long silent PCM stream.
    if (output.audio_samples.empty())
    {
        output.audio_channels = 1;
        output.audio_rate = 11025;
        const double duration = static_cast<double>(frame_count) / output.fps;
        const std::size_t silent_samples = static_cast<std::size_t>(
            std::ceil(duration * output.audio_rate));
        output.audio_samples.assign(silent_samples, 0);
    }

    return true;
}
