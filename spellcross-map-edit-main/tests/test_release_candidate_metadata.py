"""Source metadata regression for the RC1 packaging pipeline."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = (ROOT / "source/app_version.h").read_text(encoding="utf-8")
RESOURCE = (ROOT / "source/resources.rc").read_text(encoding="utf-8")
IDENTITY = (ROOT / "source/app_identity.h").read_text(encoding="utf-8")
PACKAGER = (ROOT.parent / "packaging/Build-CombinedRelease.ps1").read_text(encoding="utf-8")


def macro(name):
    match = re.search(r'^#define\s+' + re.escape(name) + r'\s+"([^"]+)"\s*$', HEADER, re.M)
    if not match:
        raise AssertionError(f"Missing {name}")
    return match.group(1)


class ReleaseCandidateVersionTest(unittest.TestCase):
    def test_numeric_windows_resource_matches_header(self):
        self.assertEqual(macro("SPELLCROSS_VERSION_STRING"), "0.9.3.0")
        self.assertIn("#define SPELLCROSS_VERSION_COMMA 0,9,3,0", HEADER)
        self.assertEqual(RESOURCE.count('"0.9.3.0\\0"'), 2)
        self.assertNotIn('"0.9.2.3\\0"', RESOURCE)

    def test_release_label_and_tag(self):
        self.assertEqual(macro("SPELLCROSS_RELEASE_LABEL"), "0.9.3-RC1")
        self.assertEqual(macro("SPELLCROSS_RELEASE_TAG"), "v0.9.3-RC1")
        self.assertIn('SPELLCROSS_RELEASE_LABEL', IDENTITY)

    def test_packaging_uses_label_but_checks_binary_version(self):
        self.assertIn('$packageVersion', PACKAGER)
        self.assertIn('Spellcross_Reloaded_v$packageVersion.zip', PACKAGER)
        self.assertIn('Spellcross_Reloaded_v$packageVersion.exe', PACKAGER)
        self.assertIn('$builtFileVersion.Trim() -ne $releaseVersion', PACKAGER)
        self.assertIn('$builtProductVersion.Trim() -ne $releaseVersion', PACKAGER)
        self.assertIn('Release tag: $releaseTag', PACKAGER)
        self.assertIn('MP4 integrity check failed', PACKAGER)

    def test_release_notes_present(self):
        notes = ROOT.parent / "packaging/releases/v0.9.3-RC1.md"
        self.assertTrue(notes.is_file())


if __name__ == "__main__":
    unittest.main()
