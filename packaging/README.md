# Combined Windows x64 release (game + map editor)

This build produces **one portable ZIP** containing the playable Spellcross Reloaded and the latest standalone map editor. Original retail Spellcross archives are **not** included.

## Build

From the repository root on Windows (PowerShell 5.1+):

~~~powershell
git submodule update --init --recursive
.\packaging\Build-CombinedRelease.ps1
~~~

The script builds both C++ projects in Release x64 through MSBuild, finds MSVC dumpbin.exe, bundles the dynamic DLL imports from the configured wxWidgets/vcpkg x64 installation, and writes **dist/Spellcross_Reloaded_v0.8.1.zip**. Rebuilding this version replaces the previous ZIP of the same name; dated staging directories remain eligible for cleanup.

For a typical vcpkg installation:

~~~powershell
.\packaging\Build-CombinedRelease.ps1 -WxRoot "$HOME\vcpkg\installed\x64-windows"
~~~

The 37 MP4 cutscenes present in v0.8.0 are intentionally ignored by Git
source control and must be supplied from an existing local installation.
The packager automatically looks in the game build output and game data
source directory. If absent, unpack v0.8.0 and pass its root directory:

~~~powershell
.\packaging\Build-CombinedRelease.ps1 -WxRoot "$HOME\vcpkg\installed\x64-windows" -MovieSource "D:\Games\Spellcross-v0.8.0"
~~~

Every MP4 is SHA-256 verified against data/movie_fallback/MANIFEST.tsv.
A missing/mismatched file causes the build to stop instead of silently
shipping an incomplete video pack.

To package two already-built Release executables without compiling:

~~~powershell
.\packaging\Build-CombinedRelease.ps1 -SkipBuild
~~~

The package needs the Microsoft Visual C++ Redistributable x64 on a clean system:
https://aka.ms/vc14/vc_redist.x64.exe

## Cleaning old builds in dist/

Older packaging runs created dated ZIPs and matching uncompressed staging
directories. The new stable filename Spellcross_Reloaded_v0.8.1.zip is always
protected by the cleanup script. Clean older builds safely:

~~~powershell
.\packaging\Clean-OldBuilds.ps1 -Keep 3             # preview
.\packaging\Clean-OldBuilds.ps1 -Keep 3 -Apply      # delete only eligible old builds
~~~

The three newest timestamped builds are always preserved. An older staging
folder containing user configuration, imported game archives, save or temp
data will be **protected**, not deleted. Unrelated files in dist are ignored.

To opt in to automatically pruning old outputs after a **successful** future
build:

~~~powershell
.\packaging\Build-CombinedRelease.ps1 -SkipBuild -WxRoot "$HOME\vcpkg\installed\x64-windows" -PruneOldBuilds
~~~

## Layout

~~~text
Spellcross_Reloaded_v0.8.1.zip  (files at ZIP root)
  Spellcross.exe
  data/                   # Reloaded auxiliary assets and menus
  Editor/
    spellcross_map_editor.exe
    data/                 # editor auxiliary archives, contexts and fonts
  LICENSE-Reloaded.txt
  LICENSE-Map-Editor.txt
  README-FIRST.txt
~~~

On first use, Spellcross.exe creates/configures config.ini at the package root. It can import DATA/INSTALL.DTA from the user's original **mounted** CD/ISO into game_data/DATA/.

The editor maintains its own Editor/config.ini. It can find the game root and use original game archives from game_data/DATA or game root [FILES] configuration. Its own Editor/data/COMMON.fs is distinct from the retail COMMON.FS.

The packager deliberately whitelists support assets. It does not copy game_data/, saves, earlier config.ini files, the upstream editor's old hard-coded INI, or retail archives.

## Acceptance test before publishing

1. Build **both** Release x64 projects.
2. Extract the ZIP to a different writable directory **outside** the source checkout.
3. Run Spellcross.exe and supply legal original Spellcross archives or import from a mounted ISO/CD.
4. Exit and restart the game; verify graphics, audio and settings.
5. Launch Editor/spellcross_map_editor.exe without any prior Editor/config.ini.
6. Open a map, edit it, save under a different filename, and reopen it.
7. Test on clean Windows 10/11 x64 with the VC++ Redistributable, but without Visual Studio or wxWidgets.
8. Don't merge or publish the draft until these tests pass.

## Scope and cautions

- Windows x64 test ZIP, not a signed installer or automatic GitHub release.
- Runtime DLLs from vcpkg carry upstream licenses; review all third-party notices before public redistribution.
- The importer accepts a mounted ISO / original CD directory; it does not mount raw .iso files.
- The code has not been compiled in the assistant environment. Build and portability testing remain necessary.
