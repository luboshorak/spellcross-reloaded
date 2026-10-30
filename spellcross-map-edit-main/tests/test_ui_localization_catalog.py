"""Checks that Reloaded UI catalogs stay complete as the interface grows.

Run from any directory: python spellcross-map-edit-main/tests/test_ui_localization_catalog.py
No commercial game files or third-party packages are needed.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
LOCALES = ROOT / "locales"
SOURCES = [
    ROOT / "source" / "main.cpp",
    ROOT / "source" / "forms" / "form_level.cpp",
]
CALL = re.compile(r'spellcross_ui_locale::Tr\(\s*"([^"]+)"')


def catalog(lang):
    data = {}
    for number, line in enumerate((LOCALES / f"{lang}.lang").read_text(encoding="utf-8-sig").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith(("#", ";")):
            continue
        assert "=" in line, f"{lang}.lang:{number}: missing ="
        key, value = line.split("=", 1)
        key = key.strip()
        assert key, f"{lang}.lang:{number}: empty key"
        assert key not in data, f"{lang}.lang:{number}: duplicate {key}"
        assert value.strip(), f"{lang}.lang:{number}: empty translation {key}"
        data[key] = value
    return data


class LocalizationCatalogTests(unittest.TestCase):
    def test_all_languages_have_identical_nonempty_keys(self):
        en, cs = catalog("en"), catalog("cs")
        self.assertGreaterEqual(len(en), 80)
        self.assertEqual(en.keys(), cs.keys())

    def test_referenced_strings_are_translated(self):
        sources = "\n".join(src.read_text(encoding="utf-8") for src in SOURCES)
        referenced = set(CALL.findall(sources))
        # Dynamically constructed rank keys are intentionally not literal.
        referenced.update(f"stats.rank.{i}" for i in range(9))
        referenced.update(f"buy.rank.{i}" for i in range(9))
        for lang in ("en", "cs"):
            missing = referenced - catalog(lang).keys()
            self.assertFalse(missing, f"Missing {lang} keys: {sorted(missing)}")

    def test_edition_selection_and_fallback(self):
        app = (ROOT / "source" / "main.cpp").read_text(encoding="utf-8")
        helper = (ROOT / "source" / "spell_ui_locale.h").read_text(encoding="utf-8")
        self.assertIn("spellcross_ui_locale::SetLanguageFromEdition(", app)
        self.assertIn('SetLanguage(englishRetailEdition ? "en" : "cs")', helper)
        self.assertIn("state.english.find(key)", helper)
        self.assertIn("englishFallback", helper)

    def test_native_buy_screen_uses_translations(self):
        source = (ROOT / "source" / "forms" / "form_level.cpp").read_text(encoding="utf-8")
        for key in (
            "buy.category.infantry",
            "buy.category.tanks",
            "buy.category.artillery",
            "buy.category.transporters",
            "buy.category.radars",
            "buy.category.air_defense",
            "buy.category.other",
            "buy.action.buy",
            "buy.stats.objects",
            "buy.stats.defense",
            "buy.stats.sight",
        ):
            self.assertIn(f'spellcross_ui_locale::Tr("{key}"', source)
        self.assertIn("LocalizedOriginalRankAbbrev(commander.rank)", source)
        self.assertNotIn('L"Koupit"', source)
        self.assertNotIn(r'L"P\u011Bchota"', source)

    def test_menu_fallbacks_use_real_cpp_tab_escapes(self):
        source = (ROOT / "source" / "main.cpp").read_text(encoding="utf-8")
        menu_lines = [
            line for line in source.splitlines()
            if 'spellcross_ui_locale::Tr("menu.' in line
        ]
        self.assertEqual(sum(r"\t" in line for line in menu_lines), 5)
        # Double-escaped C++ source would display a literal \t in wx menus.
        self.assertFalse(any(r"\\t" in line for line in menu_lines))
        for lang in ("en", "cs"):
            entries = catalog(lang)
            for key in ("menu.load_custom_map", "menu.main",
                        "menu.load_strategic", "menu.save_game", "menu.load_game"):
                self.assertIn("\t", entries[key])

    def test_catalogs_resolve_from_release_and_development_roots(self):
        helper = (ROOT / "source" / "spell_ui_locale.h").read_text(encoding="utf-8")
        self.assertIn('dir / "locales"', helper)
        self.assertIn('dir / "spellcross-map-edit-main" / "locales"', helper)
        self.assertIn("addParents(exe.parent_path())", helper)
        self.assertIn("addParents(cwd)", helper)

    def test_original_text_is_not_in_ui_catalogs(self):
        # Mission text still comes from TEXTS.FS; catalogs contain only UI keys.
        engine = (ROOT / "source" / "spellcross.cpp").read_text(encoding="utf-8")
        self.assertIn('units && units->IsEnglish() ? SpellLang::ENG : SpellLang::CZE', engine)
        for lang in ("en", "cs"):
            keys = catalog(lang)
            self.assertFalse(any(k.startswith(("mission.text.", "texts_fs.")) for k in keys))

    def test_cr_only_original_wrapping_is_not_discarded(self):
        source = (ROOT / "source" / "forms" / "form_level.cpp").read_text(encoding="utf-8")
        self.assertRegex(
            source,
            r"if \(c == '\\r'\)\s*\{\s*s\.push_back\('\\n'\);",
        )


if __name__ == "__main__":
    unittest.main()
