# Native CAN playback

CAN, DPK, DP2 and SMK cinematics are decoded directly from the selected
original retail archives. No separate converted movie files or runtime
switch are required. The Czech intro, first mission animation, further
campaign movies and full ending sequence were confirmed in game by the
maintainer on the native-only experimental branch.

The decoder implements LZSS, 4x4 blocks, indexed palette lookup and 8-bit
mono PCM at 11025 Hz. The palette transform row = intensity * 4 and the
15 FPS timing are experimental. Audio is trimmed to the frame duration
(the first samples are retained); synchronization is not yet validated.
All frames are held in memory: INTRO.CAN alone uses about 243 MB of
indexed frame data. No original assets are included.

Standalone testing (from spellcross-map-edit-main):

    g++ -std=c++17 -O2 tests/can_decoder_test.cpp -o can_decoder_test
    ./can_decoder_test /path/to/extracted/*.CAN

The C++ decoder was exercised on every frame of 19 CAN files from one
Czech retail archive. This establishes structural decoding, not visual
fidelity or correctness of the full game integration.
