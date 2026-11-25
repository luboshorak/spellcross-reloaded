# Experimental native CAN playback

Build the usual game from this branch and start it normally using original
Czech game data. Native CAN is enabled by default on this isolated branch.
To disable the experiment for comparison, close the game and set
`$env:SPELLCROSS_EXPERIMENTAL_CAN = "0"` in PowerShell before starting the
executable from that shell. Remove the variable to restore native playback.

With the experiment enabled the Czech CAN/SMK resolver tries original archive
entries before MP4. If native decoding fails, the existing Czech MP4
fallback remains available on Windows. For a definitive native test,
temporarily move all matching MP4 fallbacks out of the search paths.
Debug logs distinguish experimental CAN picture playback and fallback.
Non-CAN DPK, DP2 and SMK decoding algorithms are unchanged.

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
