"""Guard the repository's game-source path and release entry points."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
GAME = "spellcross-remaster-main"
LEGACY = "spellcross-map-" + "edit-main"


class RepositoryLayoutTest(unittest.TestCase):
    def test_game_and_editor_build_projects_exist(self):
        self.assertTrue((ROOT / GAME / "source/spellcross_map_editor.vcxproj").is_file())
        self.assertTrue((ROOT / GAME / "source/spellcross_map_editor.sln").is_file())
        self.assertTrue((ROOT / GAME / "source/project_attribution.h").is_file())
        self.assertTrue((ROOT / GAME / "locales/cs.lang").is_file())
        self.assertTrue((ROOT / GAME / "locales/en.lang").is_file())
        self.assertTrue((ROOT / "third_party/libsmacker/smacker.c").is_file())
        self.assertIn("external/map-editor", (ROOT / ".gitmodules").read_text())

    def test_no_tracked_legacy_directory_or_stale_text_reference(self):
        # Use tracked paths only: a developer may still have untracked old
        # build artifacts locally after pulling the directory rename.
        files = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
        self.assertFalse([p for p in files if p.startswith(LEGACY + "/")])
        result = subprocess.run(
            ["git", "grep", "-I", "-n", "-i", "-F", LEGACY, "--"],
            cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace",
        )
        self.assertIn(result.returncode, (0, 1), result.stderr)
        unexpected = []
        for line in result.stdout.splitlines():
            if line.startswith("docs/archive/README.md:") and "The original paths were under" in line:
                continue
            if line.startswith("docs/repository-layout.md:") and "Removed after archiving" in line:
                continue
            if "github.com/luboshorak/spellcross_restoration_tools/" in line:
                continue  # historical upstream URL, not a path in this checkout
            unexpected.append(line)
        self.assertFalse(unexpected, "\n".join(unexpected))

    def test_packagers_use_new_game_root(self):
        for name in ("Build-CombinedRelease.ps1", "Build-UniversalNoRelease.ps1",
                     "Release.ps1", "Add-DistributionLicenses.ps1"):
            script = (ROOT / "packaging" / name).read_text(encoding="utf-8-sig")
            self.assertIn(GAME, script, name)
            self.assertNotIn(LEGACY, script, name)


if __name__ == "__main__":
    unittest.main()
