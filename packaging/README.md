# Building and releasing Spellcross Reloaded

This guide builds the Windows x64 game and its pinned standalone editor. For installation and gameplay, see the [main README](../README.md).

## Requirements

- Windows with Visual Studio C++ build tools and MSBuild. The projects use the **v145** toolset.
- A compatible wxWidgets/vcpkg x64 installation.
- The editor submodule initialized at the revision pinned by this repository.
- All **37 fallback MP4s** matching `data/movie_fallback/MANIFEST.tsv`.
- **WinRAR** for the optional graphical SFX installer.

Original commercial Spellcross archives are not bundled.

## One-command release build (commit and push)

After reviewing the changes in `main`, run:

```powershell
git switch main
git pull --ff-only
git submodule sync --recursive
git submodule update --init --recursive
.\packaging\Build-CombinedRelease.ps1 -Rebuild `
    -PreviousStageDirectory 'D:\Spellcross\previous-release' `
    -PreviousVersion '0.9.6.1'
```

The script asks for one new version (for example `0.9.6`). Alternatively,
supply `-Version`; the same previous-stage parameters remain mandatory:

```powershell
.\packaging\Build-CombinedRelease.ps1 -Version 0.9.6 -Rebuild `
    -PreviousStageDirectory 'D:\Spellcross\previous-release' -PreviousVersion 0.9.5
```

It executes **four phases**:

1. Update `app_version.h` and the two `resources.rc` Windows VERSIONINFO
   values. Fully rebuild and validate the game and standalone editor executables.
2. **Commit the updated source version** to Git (only the two metadata files).
3. Package the completed ZIP and optional WinRAR SFX using the real version
   commit in `BUILD-INFO.txt`. Binaries and PDB hashes are recorded.
4. Create an annotated tag (`v0.9.6`) and **atomically push the commit plus
   tag to origin/main**. No force push is used.

If compilation fails, the original version files are restored byte-for-byte
and there is no version commit. If packaging or the final push fails **after
the version commit**, the commit is deliberately kept locally. This avoids
producing binaries whose version no longer matches Git. Resolve the failure
and finish packaging or push manually; the script never rewrites Git history.

The main repository must have no uncommitted changes, and its current
branch must match `origin/main`. **Local modifications inside the
`external/map-editor` submodule are allowed**, provided the submodule is
still checked out at the commit pinned by main. In that case, the release
script automatically builds and packages the editor from a temporary clean
Git worktree at that pinned commit. The developer's edited submodule is
never cleaned, reset, stashed, or overwritten. A changed submodule commit
(gitlink), by contrast, still blocks release. A missing or uninitialized
submodule also blocks release with an actionable message.

The script checks Git tags to avoid releasing the same version twice.

For local-only testing, `-NoPush` creates the version commit and tag locally
without uploading them. On a feature branch, both `-NoPush -AllowNonMain` are
required. To check version injection with no commit/build/push, use
`.\packaging\Release.ps1 -Version 0.9.6 -ValidateOnly`.

With custom dependencies or movie location:

```powershell
.\packaging\Build-CombinedRelease.ps1 -Version 0.9.6 `
    -WxRoot "$HOME\vcpkg\installed\x64-windows" `
    -MovieSource "D:\Games\Spellcross-previous-build"
```

Output includes the full build and mandatory patch pair:

```text
dist/Spellcross_Reloaded_v0.9.6.zip
dist/Spellcross_Reloaded_v0.9.6.exe
dist/patch-manifest.json
dist/Spellcross_Patch_v0.9.5_to_v0.9.6.zip
```

The EXE is only created when WinRAR is installed; ZIP creation is mandatory
for an automated release. `-SkipBuild` is still available for manual
repackaging of binaries that match the already committed version.

**Reproducibility:** checkout of the pushed release tag contains the version
that was actually compiled. A plain rebuild from that tag is consistent with
the published `Spellcross.exe` and `BUILD-INFO.txt`.

## Version and package contents

The release command permanently commits the release version in `spellcross-map-edit-main/source/app_version.h` and `source/resources.rc`:

| Field | example v0.9.6 |
| --- | --- |
| Windows FileVersion / ProductVersion | `0.9.6.0` |
| Display and package label | `0.9.6` |
| Git tag | `v0.9.6` |

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

## Optional in-game patches (experimental)

The game can now offer a **voluntary** update when it starts, and the Info
window has a manual **Install patch** action. Each full ZIP/SFX includes the
PowerShell updater; an older build without the client needs one initial full
download. Normal releases require patch inputs; pass
`-PreviousStageDirectory <previous extracted release>` and
`-PreviousVersion <previous version>` to this release script.
Publish with `packaging/Publish-Release.ps1`; it rejects a missing or mismatched
`patch-manifest.json` / `Spellcross_Patch_vFROM_to_vTO.zip` pair. See
[patch-updates.md](../docs/patch-updates.md)
for the update-chain behavior, safety checks and unverified edge cases.

## Packaging options

| Option | Purpose |
| --- | --- |
| `-Version <x.y.z[.n][-RCn]>` | Select the release version instead of entering it at the prompt |
| `-Rebuild` | Clean rebuild for a release (automatic in versioned builds) |
| `-NoPush` | Commit and tag locally; skip GitHub push |
| `-AllowNonMain` | Combined with `-NoPush`, permit a feature-branch dry run |
| `-ValidateOnly` | Supported by Release.ps1, inject and restore version without compiling |
| `-NoPatch` | Explicit bootstrap/full-only release exception; normal releases require a previous stage |
| `-WxRoot <path>` | Explicit wxWidgets/vcpkg x64 installation |
| `-MovieSource <path>` | Directory containing the manifest-matching fallback videos |
| `-SkipBuild` | Repackage executables already compiled for this exact version |
| `-NoZip` | Only for legacy `-SkipBuild` packaging, not the automated release path |
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

## Publish the tested build

The release script pushes the version commit and annotated tag. After testing,
publish through the validating wrapper so the updater assets cannot be omitted:

```powershell
.\packaging\Publish-Release.ps1 -Version 0.9.6
```

Omit the EXE if WinRAR was unavailable. Do not create the same tag a second
time. In the event of a rejected push the local version commit/tag remain
available; resolve the conflict without force-pushing.
