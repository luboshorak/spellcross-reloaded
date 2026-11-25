// Exercise real PCM mixing without opening an audio device.
#include "spell_sound.h"
#include <array>
#include <algorithm>
#include <iostream>
#include <stdexcept>

struct SpellSoundPlaybackTestAccess
{
    static void Start(SpellSound& sound) { sound.sub_id = 0; sound.pos = 0; }
};

static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static int Render(SpellSound& sound)
{
    std::array<int16_t, 64> pcm;
    pcm.fill(-123);
    sound.cb_GetFrame(pcm.data(), 32);
    Check(std::all_of(pcm.begin(), pcm.end(), [&](int16_t x) { return x == pcm[0]; }),
        "Inconsistent left/right PCM samples");
    return pcm[0];
}

int main()
{
    try
    {
        SoundChannels pool(0);
        pool.SetVolume(0.6);
        SpellSample sample;
        sample.channels = 1; sample.fs = 11025; sample.samples = 4096;
        sample.data.assign(sample.samples, 12000);
        SpellSound ambient(&pool, &sample), video(&pool, &sample, true);
        SpellSoundPlaybackTestAccess::Start(ambient);
        SpellSoundPlaybackTestAccess::Start(video);
        const int original = Render(ambient);
        Check(original > 0 && Render(video) == original, "Initial volume mismatch");
        {
            SoundChannels::BackgroundMute movie(pool);
            Check(Render(ambient) == 0, "Already playing mission audio leaked into video");
            Check(Render(video) == original, "Movie narration was muted");
            SpellSound newAmbient(&pool, &sample), copiedVideo(video);
            SpellSoundPlaybackTestAccess::Start(newAmbient);
            SpellSoundPlaybackTestAccess::Start(copiedVideo);
            Check(Render(newAmbient) == 0, "New mission sound bypassed movie mute");
            Check(Render(copiedVideo) == original, "Copied video audio lost its category");
            {
                SoundChannels::BackgroundMute overlappingMovie(pool);
                Check(Render(ambient) == 0, "Overlapping movie mute failed");
            }
            Check(Render(ambient) == 0, "Closing one movie restored audio too early");
            pool.SetVolume(0.3);
            Check(pool.GetVolume() == 0.3, "Mute overwrote user volume");
            Check(Render(ambient) == 0 && Render(video) > 0 && Render(video) < original,
                "Changing volume during video broke audio routing");
        }
        Check(Render(ambient) > 0 && Render(ambient) < original && pool.GetVolume() == 0.3,
            "Closing/skipping movie did not restore current volume");
        try
        {
            SoundChannels::BackgroundMute failedMovie(pool);
            Check(Render(ambient) == 0, "Startup mute failed");
            throw 42; // simulate failure after the mute guard was acquired
        }
        catch (int) {}
        Check(Render(ambient) > 0, "Failed movie left mission muted");
        pool.SetVolume(0.0);
        {
            SoundChannels::BackgroundMute movie(pool);
            Check(Render(video) == 0, "Movie ignored user's zero volume");
        }
        Check(Render(ambient) == 0, "Movie exit undid user's mute");
        std::cout << "PASS: real PCM background/video routing, overlapping scopes, volume changes, failure unwind and user mute\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
