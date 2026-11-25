# Experimental native CAN playback

Build the usual game from this branch and start it normally using original
Czech game data. Native CAN is enabled by default on this isolated branch.
Bundled MP4 playback is deliberately disabled in this branch by the
`allowMp4Fallback = false` constant in `source/forms/form_video_box.cpp`.
Even if the MP4 files are installed, the game cannot choose them. Native
decode failures are reported rather than silently using the transcodes.
Setting `SPELLCROSS_EXPERIMENTAL_CAN=0` disables the CAN picture decoder,
but does not reenable MP4 playback. Non-CAN DPK, DP2 and SMK decoding
algorithms are unchanged. To restore MP4 playback for comparison, set
`allowMp4Fallback = true` and rebuild.

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
