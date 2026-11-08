"""Regression guard for About / optional patch localization and restored header.

These are source-level tests. A Windows wxWidgets UI smoke build is still required
for checking the actual rendered icon size and theme behavior.
"""
from __future__ import annotations

import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]
SOURCE = ROOT / "spellcross-map-edit-main" / "source"
LOCALES = ROOT / "spellcross-map-edit-main" / "locales"


def catalog(language: str) -> dict[str, str]:
    result = {}
    for raw_line in (LOCALES / f"{language}.lang").read_text(encoding="utf-8-sig").splitlines():
        line = raw_line.strip()
        if not line or line.startswith(("#", ";")) or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        if key in result:
            raise AssertionError(f"duplicate {language} key: {key}")
        result[key] = value
    return result


class AboutUpdateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.about = (SOURCE / "forms" / "form_about.cpp").read_text(encoding="utf-8")
        cls.main = (SOURCE / "main.cpp").read_text(encoding="utf-8")
        cls.cs = catalog("cs")
        cls.en = catalog("en")

    def test_every_about_and_startup_update_label_has_both_translations(self):
        on_init = self.main.split("bool MyApp::OnInit()", 1)[1]
        update = on_init.split("// Check for updates before entering the game UI.", 1)[1].split("#endif", 1)[0]
        keys = set(re.findall(r'\bTr\(\s*"([^"]+)"', self.about + "\n" + update))
        self.assertGreaterEqual(len(keys), 35)
        for language, translations in (("cs", self.cs), ("en", self.en)):
            self.assertFalse(keys - translations.keys(), f"missing {language}: {keys - translations.keys()}")
            for key in keys:
                self.assertTrue(translations[key], f"empty {language} key: {key}")

    def test_czech_and_english_are_distinct(self):
        for key in ("about.title", "about.subtitle", "about.description",
                    "updates.install", "updates.confirm", "updates.startup_question"):
            self.assertNotEqual(self.cs[key], self.en[key], key)

    def test_header_uses_large_resource_icon_and_restores_text_backgrounds(self):
        self.assertIn('wxIcon icon("IDI_ICON2", wxBITMAP_TYPE_ICO_RESOURCE, logoPixels, logoPixels)', self.about)
        self.assertIn("const int logoPixels = FromDIP(64);", self.about)
        apply_theme = self.about.index("spellcross_strategic_ui::Apply(this);")
        restore = self.about.index("for (wxStaticText* headerText : {appName, subtitle, version})")
        self.assertLess(apply_theme, restore)
        self.assertIn("headerText->SetBackgroundColour(spellcross_menu::Highlight());", self.about)

    def test_startup_uses_detected_retail_edition(self):
        on_init = self.main.split("bool MyApp::OnInit()", 1)[1]
        locale = on_init.index("spellcross_ui_locale::SetLanguageFromEdition(")
        offer = on_init.index("// Check for updates before entering the game UI.")
        self.assertLess(locale, offer)
        self.assertIn('Tr("updates.startup_question"', on_init)
        self.assertIn('Tr("updates.later"', on_init)
        self.assertNotIn('SetYesNoLabels("Install patch"', self.about)


if __name__ == "__main__":
    unittest.main()
