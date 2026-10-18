# Building Spellcross Reloaded for Windows

This is the build guide for the **game and its standalone map editor**. If you just want to play, you don't need any of this: **[download v0.9.1](https://github.com/luboshorak/spellcross-reloaded/releases/tag/v0.9.1)** and follow the [main README](../README.md).

The build produces one portable Windows x64 ZIP. It includes both applications and their support files, but **not the original commercial Spellcross archives**.

## Build the game and editor

You'll need Windows, Visual Studio/MSVC C++ build tools, and a compatible wxWidgets installation (the integrated projects currently use the v145 MSVC toolset). Run these commands in PowerShell from your existing repository checkout:

```powershell
git switch main
git pull --ff-only origin main
git submodule sync --recursive
git submodule update --init --recursive
.\packaging\Build-CombinedRelease.ps1
```

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
dist/Spellcross_Reloaded_v0.9.1.zip
```

The build also leaves dated staging folders in `dist/`. The same versioned ZIP name is reused when you rebuild.

### About the video files

The project uses **37 MP4 fallback cutscenes**, which aren't stored in Git. The packager looks for existing local copies and verifies their SHA-256 hashes against the manifest. If you have them in an older extracted build, supply that directory:

```powershell
.\packaging\Build-CombinedRelease.ps1 -MovieSource "D:\Games\Spellcross-v0.8.0"
```

Missing or mismatched clips cause packaging to stop, rather than producing an incomplete release.

## Useful build options

| Option | When to use it |
| --- | --- |
| `-WxRoot <path>` | wxWidgets/vcpkg isn't detected automatically |
| `-MovieSource <path>` | Point to existing copies of the fallback videos |
| `-SkipBuild` | Repackage the already compiled Release executables |
| `-NoZip` | Prepare the staged files without creating the ZIP |
| `-PruneOldBuilds` | Remove eligible old staging outputs after a successful build |

You can preview safe cleanup without deleting anything:

```powershell
.\packaging\Clean-OldBuilds.ps1 -Keep 3
```

To perform that cleanup:

```powershell
.\packaging\Clean-OldBuilds.ps1 -Keep 3 -Apply
```

The cleanup script keeps the newest three dated builds, protects user data and ignores unrelated files.

## What's in the ZIP?

```text
Spellcross.exe                 playable game
data/                          game support files and video fallbacks
Editor/
  spellcross_map_editor.exe   standalone editor
  data/                        editor support files
README-FIRST.txt
LICENSE-Reloaded.txt
LICENSE-Map-Editor.txt
```

The game creates its `config.ini` and imported `game_data/DATA/` on first use. The editor keeps a separate `Editor/config.ini`. Its own `Editor/data/COMMON.fs` supplies editor resources and is different from the original retail `COMMON.FS`.

The packager intentionally leaves out your saves, settings, imported commercial archives and other local/private files. Keep those separately.

## Check a build before distributing it

A successful compile is a good start, but a portable release needs a quick hands-on test:

1. Extract the ZIP **outside** your source checkout and launch `Spellcross.exe` without Visual Studio installed.
2. Import original data from a **raw ISO file**, or try an existing `DATA` directory. Check that the game still works after restarting.
3. Open a tactical mission, check graphics, sound and a save/load cycle.
4. Launch the map editor from the game. Test the save/open/cancel prompts and what happens if the editor executable or mission file is missing.
5. In the editor, paint terrain, roads and buildings; test Undo, flat tile rotation, L2 object dragging and save/reopen.
6. Load a standalone `.DEF` + `.DTA` mission in the playable game and check that it starts correctly.
7. On a clean Windows 10/11 x64 machine, confirm that the portable package needs only the [VC++ Redistributable](https://aka.ms/vc14/vc_redist.x64.exe), not your development environment.

The v0.9.1 source is prepared; the public GitHub release will be published separately after the build and checks.

## Notes

The program and support files are distributed separately from the original commercial game archives, which users must supply themselves. Review third-party runtime licenses before redistribution. Original game cinematics and other retail assets are not covered by the project's source-code MIT license.
