# Building and releasing Spellcross Reloaded

This guide builds the Windows x64 game and its pinned standalone editor. For installation and gameplay, see the [main README](../README.md).

## Requirements

- Windows with Visual Studio C++ build tools and MSBuild. The projects use the **v145** toolset.
- A compatible wxWidgets/vcpkg x64 installation.
- The editor submodule initialized at the revision pinned by this repository.
- All **37 fallback MP4s** matching `data/movie_fallback/MANIFEST.tsv`.
- **WinRAR** for the optional graphical SFX installer.

Original commercial Spellcross archives are not bundled.

## Build v0.9.4.2

From a checkout containing the v0.9.4.2 version update:

```powershell
git switch main
git pull --ff-only
git submodule sync --recursive
git submodule update --init --recursive
.\packaging\Build-CombinedRelease.ps1 -Rebuild
```

For a new checkout:

```powershell
git clone --recurse-submodules https://github.com/luboshorak/spellcross-reloaded.git
cd spellcross-reloaded
.\packaging\Build-CombinedRelease.ps1 -Rebuild
```

If script execution is blocked by a local PowerShell policy, allow it for the current process and rerun the build:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
.\packaging\Build-CombinedRelease.ps1 -Rebuild
```

For a custom dependency or video location:

```powershell
.\packaging\Build-CombinedRelease.ps1 -Rebuild `
    -WxRoot "$HOME\vcpkg\installed\x64-windows" `
    -MovieSource "D:\Games\Spellcross-previous-build"
```

The script rebuilds the game and editor, checks the compiled game version, verifies video hashes, copies runtime dependencies and stages the package. Output:

```text
dist/Spellcross_Reloaded_v0.9.4.2.zip
dist/Spellcross_Reloaded_v0.9.4.2.exe
```

The EXE is produced when WinRAR is available. Without WinRAR, the ZIP is still built.

## Version and package contents

The version comes from `spellcross-map-edit-main/source/app_version.h`:

| Field | v0.9.4.2 value |
| --- | --- |
| Windows FileVersion / ProductVersion | `0.9.4.2` |
| Display and package label | `0.9.4.2` |
| Git tag | `v0.9.4.2` |

The Windows resource strings in `source/resources.rc` must agree. Packaging stops if the actual game executable has a different version, including with `-SkipBuild`.

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
| `-Rebuild` | Clean rebuild for a release |
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

Use the exact clean source commit that produced the verified packages. v0.9.4.2 is a regular release; do not mark it as a prerelease.

```powershell
git status --short
git tag -a v0.9.4.2 -m "Spellcross Reloaded v0.9.4.2"
git push origin v0.9.4.2
gh release create v0.9.4.2 `
    dist/Spellcross_Reloaded_v0.9.4.2.zip `
    dist/Spellcross_Reloaded_v0.9.4.2.exe `
    --verify-tag `
    --title "Spellcross Reloaded v0.9.4.2" `
    --notes-file packaging/releases/v0.9.4.2.md
```

If only the ZIP was built, omit the EXE argument. Review third-party runtime licenses before distribution. Original game assets are not covered by the source-code MIT license.
