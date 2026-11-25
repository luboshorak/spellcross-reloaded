"""#183: EN / other editions must play original clips without Czech MP4 fallback."""
from pathlib import Path
import unittest

SOURCE = (Path(__file__).resolve().parents[1] /
          "source/forms/form_video_box.cpp").read_text(encoding="utf-8")
VIDEO = (Path(__file__).resolve().parents[1] /
         "source/spell_video.cpp").read_text(encoding="utf-8")


class NativeEditionVideoRegression(unittest.TestCase):
    def test_all_editions_use_native_archives(self):
        constructor = SOURCE.split("FormVideoBox::FormVideoBox(", 1)[1].split(
            "    // frame border graphics", 1)[0]
        self.assertIn("TryLoadLegacyVideo(spell_data, entry_name, m_video, legacy_error)", constructor)
        self.assertIn("Check the selected retail MOVIE.FS/SPEAKER.FS sources.", constructor)
        for removed in ("FindBundledMovieFallback", "movie_fallback", "MFPlay", "use_fallback"):
            self.assertNotIn(removed, SOURCE)

    def test_original_archive_sources_are_exhaustive_and_edition_scoped(self):
        lookup = SOURCE.split("bool TryLoadLegacyVideo(", 1)[1].split(
            "// load video and make form", 1
        )[0]
        self.assertIn("for (const auto& path : archive_candidates)", lookup)
        self.assertIn("for (const auto& entry : entries)", lookup)
        self.assertIn("SpellVideo(&archive, entry)", lookup)
        self.assertIn("if (czechEdition)", lookup)
        self.assertIn('entries.push_back(stem + ".DPK")', lookup)
        self.assertIn('entries.push_back(stem + ".CAN")', lookup)
        self.assertIn('entries.push_back(requested.filename().string())', lookup)
        smk = lookup.split('else if (ext == ".SMK")', 1)[1]
        self.assertLess(smk.index('entries.push_back(requested.filename().string())'),
                        smk.index('entries.push_back(stem + ".DPK")'))
        self.assertLess(lookup.index('entries.push_back(stem + ".DPK")', lookup.index('else\n        {')),
                        len(lookup))
        self.assertNotIn("std::wstring archive_path;", lookup)
        self.assertIn('selected edition MOVIE.FS/SPEAKER.FS archive not found', lookup)

    def test_can_picture_decoder_is_unconditional(self):
        decode = VIDEO.split("int SpellVideo::DecodeCAN(", 1)[1].split(
            "int SpellVideo::DecodeDPK(", 1)[0]
        self.assertIn("spellcross_can::Decode(", decode)
        self.assertIn("frames = std::move(decoded.frames)", decode)
        self.assertNotIn("ExperimentalCANEnabled", VIDEO)
        self.assertNotIn("SPELLCROSS_EXPERIMENTAL_CAN", VIDEO)
        self.assertNotIn("!m_video->isCAN()", SOURCE)

    def test_real_smk_decoder_is_wired_into_archive_playback(self):
        self.assertIn('else if(ext == ".SMK" && DecodeSMK(v_data, v_size))', VIDEO)
        self.assertIn('DecodeSmackerVideo(data, static_cast<size_t>(len), decoded, error)', VIDEO)
        self.assertIn('frame_palettes = std::move(decoded.palettes)', VIDEO)


if __name__ == "__main__":
    unittest.main()
