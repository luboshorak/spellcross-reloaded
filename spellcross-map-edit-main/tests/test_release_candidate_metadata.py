"""Source metadata regression for the current release packaging pipeline."""
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


class ReleaseMetadataTest(unittest.TestCase):
    def test_numeric_windows_resource_matches_header(self):
        # The release version changes frequently; verify that its four numeric
        # components agree across the executable VERSIONINFO and header.
        components = []
        for key in ("MAJOR", "MINOR", "PATCH", "BUILD"):
            match = re.search(
                r"^#define\s+SPELLCROSS_VERSION_" + key + r"\s+(\d+)\s*$",
                HEADER, re.M,
            )
            self.assertIsNotNone(match, f"Missing numeric version component: {key}")
            components.append(int(match.group(1)))
        version = ".".join(map(str, components))
        self.assertEqual(macro("SPELLCROSS_VERSION_STRING"), version)
        self.assertIn(
            "#define SPELLCROSS_VERSION_COMMA " + ",".join(map(str, components)),
            HEADER,
        )
        self.assertEqual(RESOURCE.count('"' + version + '\\0"'), 2)
        self.assertIn("FILEVERSION SPELLCROSS_VERSION_COMMA", RESOURCE)

    def test_release_label_and_tag(self):
        version = macro("SPELLCROSS_VERSION_STRING")
        # Stable x.y.z builds use a numeric Windows-only trailing zero, while
        # hotfix releases such as 0.9.6.1 keep their fourth component in the
        # public label and tag as well.
        expected_label = version[:-2] if version.endswith(".0") else version
        self.assertEqual(macro("SPELLCROSS_RELEASE_LABEL"), expected_label)
        self.assertEqual(macro("SPELLCROSS_RELEASE_TAG"), "v" + macro("SPELLCROSS_RELEASE_LABEL"))
        self.assertIn("SPELLCROSS_RELEASE_LABEL", IDENTITY)

    def test_packaging_uses_label_but_checks_binary_version(self):
        self.assertIn('$packageVersion', PACKAGER)
        self.assertIn('Spellcross_Reloaded_v$packageVersion.zip', PACKAGER)
        self.assertIn('Spellcross_Reloaded_v$packageVersion.exe', PACKAGER)
        self.assertIn('$builtFileVersion.Trim() -ne $releaseVersion', PACKAGER)
        self.assertIn('$builtProductVersion.Trim() -ne $releaseVersion', PACKAGER)
        self.assertIn('Release tag: $releaseTag', PACKAGER)
        self.assertIn('MP4 integrity check failed', PACKAGER)

    def test_release_notes_present(self):
        notes = ROOT.parent / "packaging/releases/v0.9.5.md"
        self.assertTrue(notes.is_file())


if __name__ == "__main__":
    unittest.main()
