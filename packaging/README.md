# Building and releasing Spellcross Reloaded

This guide builds the Windows x64 game and its pinned standalone editor. For installation and gameplay, see the [main README](../README.md).

## Requirements

- Windows with Visual Studio C++ build tools and MSBuild. The projects use the **v145** toolset.
- A compatible wxWidgets/vcpkg x64 installation.
- The editor submodule initialized at the revision pinned by this repository.
- All **37 fallback MP4s** matching `data/movie_fallback/MANIFEST.tsv`.
- **WinRAR** for the optional graphical SFX installer.

Original commercial Spellcross archives are not bundled.

## One-command release build

From a reviewed `main` checkout:

```powershell
git switch main
git pull --ff-only
git submodule sync --recursive
git submodule update --init --recursive
.\packaging\Build-CombinedRelease.ps1 -Rebuild
```

The script asks for **one release version** (for example `0.9.6`).
It automatically sets all version macros in `app_version.h` and both
Windows VERSIONINFO strings in `resources.rc`, compiles the game and map
editor in Release x64, verifies the compiled EXE version, and packages
the usual ZIP and optional WinRAR SFX.

**No manual version edits.** Both source files are restored **byte-for-byte**
in a `finally` block after a successful build or a normal error. Your
next `git pull --ff-only` is not blocked by generated version changes.
Do not forcibly kill PowerShell or turn off the PC during the build:
a forced process termination cannot execute cleanup.

You can also pass the desired release version directly:

```powershell
.\packaging\Build-CombinedRelease.ps1 -Version 0.9.6 -Rebuild
.\packaging\Build-CombinedRelease.ps1 -Version 0.9.6-RC1 -Rebuild
.\packaging\Build-CombinedRelease.ps1 -Version 0.9.4.2 -Rebuild
```

Or use `.\packaging\Release.ps1 -Version 0.9.6`. Both routes force a
clean rebuild to prevent stale version resources. `-SkipBuild` remains
available as a legacy repackage action against the version stored in
the checked-in header; `-SkipBuild -Version` is rejected.

With a custom dependency or movie location:

```powershell
.\packaging\Build-CombinedRelease.ps1 -Version 0.9.6 `
    -WxRoot "$HOME\vcpkg\installed\x64-windows" `
    -MovieSource "D:\Games\Spellcross-previous-build"
```

Output, when WinRAR is installed:

```text
dist/Spellcross_Reloaded_v0.9.6.zip
dist/Spellcross_Reloaded_v0.9.6.exe
```

Without WinRAR the complete ZIP is still built. To test version injection
without running the compiler: `.\packaging\Release.ps1 -Version 0.9.6
-ValidateOnly`.

**Reproducibility:** the release version is a build parameter, not a
permanent source edit. `BUILD-INFO.txt` records the Git source commit,
effective release version, and that it was injected by the release helper.
To rebuild the tagged commit, pass the same `-Version`. A raw compilation
of that tag without specifying the release version can have the older
checked-in version number.

If the two version source files already contain local changes, the
helper refuses to overwrite them: finish or stash those edits first.

## Version and package contents

The release command temporarily overrides the checked-in version in `spellcross-map-edit-main/source/app_version.h`:

| Field | v0.9.5 value |
| --- | --- |
| Windows FileVersion / ProductVersion | `0.9.5.0` |
| Display and package label | `0.9.5` |
| Git tag | `v0.9.5` |

The script synchronizes the Windows resource strings and still stops if the compiled game EXE does not match the selected release version.

Both distributions have the same layout:

```text
Spellcross.exe
data/
locales/
Editor/
  spellcross_map_editor.exe
  data/
README-FIRST.txt
BUILD-INFO.txt
LICENSE-Reloaded.txt
LICENSE-Map-Editor.txt
```

When SFX generation is enabled, `setup/` also contains the installer icon and branding. `BUILD-INFO.txt` records the source commit, release label, numeric version, executable SHA-256 and packaged PDB SHA-256. The package also contains `symbols/Spellcross.pdb` so an unexpected Windows crash can be recorded with a source-level call stack in `crash-last.txt`, alongside `crash-last.dmp` and `debug.log`. Including the PDB increases package size; players do not need to send the EXE to report a crash. The packager excludes personal settings, saves and imported retail archives.

Game and editor keep separate configurations. The editor's `Editor/data/COMMON.fs` contains editor resources; it is distinct from the original game's `COMMON.FS`.

## Packaging options

| Option | Purpose |
| --- | --- |
| `-Version <x.y.z[.n][-RCn]>` | Select the release version instead of entering it at the prompt |
| `-Rebuild` | Clean rebuild for a release (automatic in versioned builds) |
| `-WxRoot <path>` | Explicit wxWidgets/vcpkg x64 installation |
| `-MovieSource <path>` | Directory containing the manifest-matching fallback videos |
| `-SkipBuild` | Repackage executables already compiled for this exact version |
| `-NoZip` | Stage files without rebuilding the ZIP |
| `-NoSfx` | Disable the EXE installer |
| `-WinRARPath <path>` | Explicit path to `WinRAR.exe` |

For example, regenerate only the SFX from exact-version executables:

```powershell
.\packaging\Build-CombinedRelease.ps1 -SkipBuild -NoZip
```

The SFX lets players choose the install directory and creates a Desktop shortcut. Its default destination is `%USERPROFILE%\Games\Spellcross Reloaded`. Branding files and WinRAR's `Default.SFX` module are required; missing files stop SFX generation. The EXE is unsigned unless signed separately.

## Verify before publication

Use a clean Windows x64 rebuild and test the extracted package outside the source checkout.

- Launch the game and editor, import CZ/EN data and restart.
- Check strategic and tactical Continue, objectives, temporary companies, fullscreen, console and Escape.
- In the desert chapter, complete the major rescue, refuse an OS attack on territory 10 and verify that recapture uses a generated battle. Save/reload and check that the rescue stays completed.
- Exercise a chapter transition, sound, cinematics and tactical save/load.
- Test editor save/reopen and a custom mission.
- Run the SFX into a custom folder and verify its Desktop shortcut.
- On a clean Windows 10/11 x64 machine, check the [Visual C++ Redistributable](https://aka.ms/vc14/vc_redist.x64.exe) requirement.

## Tag and publish the tested build

Tag and publish only after full testing. This example uses v0.9.6; the same -Version is required to rebuild the source commit. The script does not create tags or upload releases.

```powershell
git status --short
git tag -a v0.9.6 -m "Spellcross Reloaded v0.9.6"
git push origin v0.9.6
gh release create v0.9.6 `
    dist/Spellcross_Reloaded_v0.9.6.zip `
    dist/Spellcross_Reloaded_v0.9.6.exe `
    --verify-tag `
    --title "Spellcross Reloaded v0.9.6" `
    --notes-file packaging/releases/v0.9.6.md
```

If only the ZIP was built, omit the EXE argument. Review third-party runtime licenses before distribution. Original game assets are not covered by the source-code MIT license.
