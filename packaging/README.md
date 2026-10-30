# Building Spellcross Reloaded for Windows

This is the build guide for the **game and its standalone map editor**. If you just want to play, you don't need any of this: **[download the latest published release](https://github.com/luboshorak/spellcross-reloaded/releases/latest)** and follow the [main README](../README.md).

The build produces a portable Windows x64 ZIP **and**, if WinRAR is installed, a self-extracting SFX `.exe` installer. Both packages include the game, standalone map editor, and support files, but **not the original commercial Spellcross archives**.

## Build the game and editor

You'll need Windows, Visual Studio/MSVC C++ build tools, and a compatible wxWidgets installation (the integrated projects currently use the v145 MSVC toolset). Run these commands in PowerShell from your existing repository checkout:

```powershell
git switch main
git pull --ff-only origin main
git submodule sync --recursive
git submodule update --init --recursive
.\packaging\Build-CombinedRelease.ps1 -Rebuild
```

If PowerShell refuses to run `Build-CombinedRelease.ps1` because it is not digitally signed, allow scripts **for this PowerShell session only** and rerun the build:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
.\packaging\Build-CombinedRelease.ps1
```

This does not change the execution policy permanently. A policy enforced by your organization may still prevent the script from running.

Use `-Rebuild` when preparing an official release so the application version and embedded Windows EXE version resource are freshly compiled. If you only need an incremental developer build, running the script without `-Rebuild` is fine.

If you're starting from scratch, first clone with submodules:

```powershell
git clone --recurse-submodules https://github.com/luboshorak/spellcross-reloaded.git
cd spellcross-reloaded
```

If the build can't find wxWidgets, specify the path to your vcpkg x64 installation:

```powershell
.\packaging\Build-CombinedRelease.ps1 -WxRoot "$HOME\vcpkg\installed\x64-windows"
```

The output is:

```text
dist/Spellcross_Reloaded_v0.9.2.2.zip
dist/Spellcross_Reloaded_v0.9.2.2.exe    (WinRAR SFX, when WinRAR is installed)
```

To regenerate the SFX **without compiling or rebuilding the ZIP**, run:

```powershell
.\packaging\Build-CombinedRelease.ps1 -SkipBuild -NoZip
```

The version is read automatically from `spellcross-map-edit-main/source/app_version.h` (currently `0.9.2.2`), so the filename changes with the game version. The build also leaves dated staging folders in `dist/`; the same stable versioned package names are reused on a rebuild.

Packaging checks the compiled `Spellcross.exe` FileVersion and ProductVersion against that header and **stops** if they do not match. In particular, `-SkipBuild` cannot silently repackage an outdated executable under a new release number.

### WinRAR SFX installer

With WinRAR installed, the regular command also creates the graphical installer automatically. It has:

- Standard (93x302) and high-DPI (186x604) vertical Spellcross logos on the left side.
- The supplied `spellcross_reloaded.ico` as the setup executable icon.
- An automatic **Spellcross Reloaded** shortcut on the player's Desktop, using that icon.
- A **user-selectable extraction directory** and **SavePath** to remember the directory on later updates.
- An initial writable destination of `%USERPROFILE%\Games\Spellcross Reloaded`, which the player can freely change.

The layout inside both archives has `Spellcross.exe` **at the root**, not inside a version-numbered directory. The installed shortcut therefore correctly follows whichever installation directory the player selects. When SFX generation is enabled, the staged release bundles `setup/spellcross_reloaded.ico`, because the shortcut needs that icon after extraction.

Keep these three files in the repository's `packaging/setup/` directory:

```text
packaging/setup/spellcross_sfx_logo.png      (93 x 302)
packaging/setup/spellcross_sfx_logo_hd.png   (186 x 604)
packaging/setup/spellcross_reloaded.ico
```

WinRAR is detected through `PATH`, `C:\Program Files\WinRAR\WinRAR.exe`, or the x86 Program Files location. You can also specify its exact location:

```powershell
.\packaging\Build-CombinedRelease.ps1 -WinRARPath 'C:\Program Files\WinRAR\WinRAR.exe'
```

If WinRAR is not installed, the build still creates the ZIP and warns that it skipped SFX. If SFX is enabled but its branding files or the WinRAR `Default.SFX` module are missing, the script stops with a clear error rather than producing an incorrectly branded installer. To disable SFX explicitly, use `-NoSfx`.

**Note:** An unsigned SFX executable may still trigger a Microsoft Defender SmartScreen warning for an unknown publisher or uncommon download. Using WinRAR does not sign the installer.

### About the video files

The project uses **37 MP4 fallback cutscenes**, which aren't stored in Git. The packager looks for existing local copies and verifies their SHA-256 hashes against the manifest. If you have them in an older extracted build, supply that directory:

```powershell
.\packaging\Build-CombinedRelease.ps1 -MovieSource "D:\Games\Spellcross-v0.8.0"
```

Missing or mismatched clips cause packaging to stop, rather than producing an incomplete release.

## Useful build options

| Option | When to use it |
| --- | --- |
| `-Rebuild` | Perform a clean MSVC rebuild (recommended when changing the release version) |
| `-WxRoot <path>` | wxWidgets/vcpkg isn't detected automatically |
| `-MovieSource <path>` | Point to existing copies of the fallback videos |
| `-SkipBuild` | Repackage already compiled Release executables (only after building this exact version) |
| `-NoZip` | Prepare the staged files without creating the ZIP (SFX still created) |
| `-NoSfx` | Skip the WinRAR installer even when WinRAR is installed |
| `-WinRARPath <path>` | Supply an explicit path to `WinRAR.exe` |

Older staging folders can be removed manually from `dist/` after checking their contents. The previously documented `Clean-OldBuilds.ps1` helper is not present in the current repository, so do not use `-PruneOldBuilds` for now.

## What's in the ZIP and SFX?

```text
Spellcross.exe                 playable game
data/                          game support files and video fallbacks
Editor/
  spellcross_map_editor.exe   standalone editor
  data/                        editor support files
README-FIRST.txt
LICENSE-Reloaded.txt
LICENSE-Map-Editor.txt
setup/                        included when WinRAR SFX is enabled
  spellcross_reloaded.ico   icon for the installed desktop shortcut
  spellcross_sfx_logo.png   standard branding asset
  spellcross_sfx_logo_hd.png high-DPI branding asset
```

The game creates its `config.ini` and keeps new original-data imports under `game_data/imports/DATA-*/` on first use. Existing `game_data/DATA/` folders from older releases are preserved and remain usable. The editor keeps a separate `Editor/config.ini`. Its own `Editor/data/COMMON.fs` supplies editor resources and is different from the original retail `COMMON.FS`.

The packager intentionally leaves out your saves, settings, imported commercial archives and other local/private files. Keep those separately.

## Check a build before distributing it

A successful compile is a good start, but a portable release needs a quick hands-on test:

1. Extract the ZIP **outside** your source checkout and launch `Spellcross.exe` without Visual Studio installed. Run the SFX on a test Windows system, select a custom install folder, and check the Desktop shortcut target and icon.
2. Import original data from a **raw ISO file**, or try an existing `DATA` directory. Check that the game still works after restarting.
3. Open a tactical mission, check graphics, sound and a save/load cycle.
4. Launch the map editor from the game. Test the save/open/cancel prompts and what happens if the editor executable or mission file is missing.
5. In the editor, paint terrain, roads and buildings; test Undo, flat tile rotation, L2 object dragging and save/reopen.
6. Load a standalone `.DEF` + `.DTA` mission in the playable game and check that it starts correctly.
7. On a clean Windows 10/11 x64 machine, confirm that the portable package needs only the [VC++ Redistributable](https://aka.ms/vc14/vc_redist.x64.exe), not your development environment.

Publish the ZIP and SFX as separate optional GitHub Release assets after a Windows smoke test.

### Publishing v0.9.2.2

After the release-preparation PR is merged to `main`:

1. Run `.\packaging\Build-CombinedRelease.ps1 -Rebuild` on Windows, supplying `-WxRoot` and `-MovieSource` if required. The script must produce `Spellcross_Reloaded_v0.9.2.2.zip` and, with WinRAR available, `Spellcross_Reloaded_v0.9.2.2.exe`.
2. Open `Spellcross.exe` properties and verify **File version** and **Product version** are both **0.9.2.2**. Verify `BUILD-INFO.txt` contains the new source revision and executable SHA-256.
3. Smoke-test a counter-attack with all assigned units, load a mission via lowercase `.def`, verify the menu is free of battlefield ambience, and check that melee attacks stay adjacent. Test the enemy's response after an attack.
4. Group-move allied units toward all four map edges, select them again to verify the camera can locate them, and check enemy movement and combat near those edges.
5. After verifying both ZIP and SFX on a clean installation, create the Git tag **`v0.9.2.2`** on the exact tested `main` commit and publish a GitHub Release with both packages. Use [release notes](./releases/v0.9.2.2.md) as its description.

Do **not** tag or upload the release before the Windows build and gameplay checks pass. The sources do not contain the 37 MP4 fallback videos, so a complete distributable still requires access to the verified local movie files.

## Notes

The program and support files are distributed separately from the original commercial game archives, which users must supply themselves. Review third-party runtime licenses before redistribution. Original game cinematics and other retail assets are not covered by the project's source-code MIT license.
