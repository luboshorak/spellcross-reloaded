"""#183: EN / other editions must play original clips without Czech MP4 fallback."""
from pathlib import Path
import unittest

SOURCE = (Path(__file__).resolve().parents[1] /
          "source/forms/form_video_box.cpp").read_text(encoding="utf-8")


class NativeEditionVideoRegression(unittest.TestCase):
    def test_fallback_is_reserved_for_czech_edition(self):
        constructor = SOURCE.split("FormVideoBox::FormVideoBox(", 1)[1].split(
            "    int frame_x = 320;", 1
        )[0]
        self.assertIn("spell_data->GetTextLanguage() == SpellLang::CZE", constructor)
        self.assertIn("? FindBundledMovieFallback(spell_data, stem) : fs::path{}", constructor)
        self.assertIn('if (!czechEdition)', constructor)
        self.assertIn('Check the selected retail MOVIE.FS/SPEAKER.FS sources.', constructor)
        noncz = constructor.split('if (!czechEdition)', 1)[1].split(
            'throw runtime_error(string_format(\n            "Loading Czech', 1
        )[0]
        self.assertNotIn("fallback.mp4", noncz)
        self.assertNotIn("no bundled fallback", noncz)

    def test_original_archive_sources_are_exhaustive_and_edition_scoped(self):
        lookup = SOURCE.split("bool TryLoadLegacyVideo(", 1)[1].split(
            "\n}\n\n#ifdef _WIN32", 1
        )[0]
        self.assertIn("for (const auto& path : archive_candidates)", lookup)
        self.assertIn("for (const auto& entry : entries)", lookup)
        self.assertIn("SpellVideo(&archive, entry)", lookup)
        self.assertIn("if (czechEdition)", lookup)
        self.assertIn('entries.push_back(stem + ".DPK")', lookup)
        self.assertIn('entries.push_back(stem + ".CAN")', lookup)
        self.assertLess(lookup.index('entries.push_back(stem + ".DPK")', lookup.index('else\n        {')),
                        len(lookup))
        self.assertNotIn("std::wstring archive_path;", lookup)
        self.assertIn('selected edition MOVIE.FS/SPEAKER.FS archive not found', lookup)

    def test_czech_transcode_only_remains_available_where_needed(self):
        constructor = SOURCE.split("FormVideoBox::FormVideoBox(", 1)[1].split(
            "    int frame_x = 320;", 1
        )[0]
        self.assertIn('(ext == ".CAN" || ext == ".SMK") && !fallback_path.empty()', constructor)
        self.assertIn('TryLoadLegacyVideo(spell_data, entry_name, m_video, legacy_error)', constructor)


if __name__ == "__main__":
    unittest.main()
