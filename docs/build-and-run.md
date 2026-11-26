# Configuration, dependencies, build and running

## Real build targets

Despite its historic filename, [`spellcross-remaster-main/source/spellcross_map_editor.vcxproj`](../spellcross-remaster-main/source/spellcross_map_editor.vcxproj) compiles the **game** as `Spellcross.exe` for x64. Its `ClCompile` list includes `main.cpp`, `map.cpp`, `spellcross.cpp`, `forms/form_level.cpp` and the other C++ engine modules. The **standalone editor** is a separate pinned Git submodule in `external/map-editor` (`.gitmodules`), compiled using that submodule's project. Its language standard is managed separately and is not upgraded by this game-only change. Both are packaged by `packaging/Build-CombinedRelease.ps1`.

The game project specifies MSVC `PlatformToolset=v145` and x64 `LanguageStandard=stdcpp23`, uses wxWidgets headers and libraries under the `wxwin` property, links Windows `dsound.lib` / `winmm.lib` and includes the vendored `RtAudio`, `cxxmidi` and LGPL-2.1-or-later `libsmacker` sources. The native cinematic player decodes CAN, DPK and DP2; original SMK2 movies are decoded by libsmacker. No CMake build was found for the native game.

## Requirements

- Windows x64 with Visual Studio 2026 version 18.12 or newer and MSVC 14.52+ (`v145` toolset), required for stable `/std:c++23` support.
- Compatible wxWidgets MSVC x64 dependencies (for example from a vcpkg `installed/x64-windows` location), passed as `-WxRoot` in the release script or `/p:wxwin=...`.
- Git submodules initialized, including `external/map-editor` and its `data/COMMON.fs`.
- A legally owned original Spellcross CZ/EN game-data set to play. The source does not distribute original commercial archives.
- Optional WinRAR and its `Default.SFX` for an installer.

## Developer build

From the root in Windows Developer PowerShell:

```powershell
git clone --recurse-submodules https://github.com/luboshorak/spellcross-reloaded.git
cd spellcross-reloaded
msbuild .\spellcross-remaster-main\source\spellcross_map_editor.vcxproj /m /t:Build /p:Configuration=Release /p:Platform=x64 "/p:wxwin=$HOME\vcpkg\installed\x64-windows\"
```

The project sets its x64 output directory to `spellcross-remaster-main/builds/x64/Release/`. Debug and Release configurations differ; check the `.vcxproj` rather than assuming a toolchain-specific generated path. This review did **not** run an MSVC build.

## Packaging game plus editor

```powershell
git submodule sync --recursive
git submodule update --init --recursive
.\packaging\Build-CombinedRelease.ps1 -Rebuild -WxRoot "$HOME\vcpkg\installed\x64-windows"
```

The actual script parameters are `-SkipBuild`, `-Rebuild`, `-WxRoot`, `-OutputDirectory`, `-NoZip`, `-NoSfx`, `-WinRARPath` and `-PruneOldBuilds`. Use `-NoSfx` to skip optional WinRAR. The packager reads `source/app_version.h`, validates compiled FileVersion/ProductVersion (including with `-SkipBuild`), bundles runtime DLL dependencies discovered using `dumpbin` and stages the two executables with their data/licenses. See [packaging/README.md](../packaging/README.md) for complete release, tagging and smoke-test instructions.

## Runtime settings and paths

`MyApp::OnInit` calls `FindRuntimeRoot`, `FindConfigPath` and `EnsureSpellcrossSourceConfiguration` (`source/main.cpp`). Empty/missing `config.ini` is allowed; `source/config.example.ini` provides a template, and the original-data wizard stores actual paths. Selected keys:

```ini
[STATE]
last_map =
default_midi = M_MUS04
sound_volume = 70
music_volume = 30
display_scale = 125
display_smoothing = false
display_brightness = 100
display_gamma = 1.3
display_temperature = 6500

[SPELCROS]
spell_path =
spellcd_path =

[DATA]
spec_data_path = data\
units_aux_data_path = data\units.fsa
```

Run `Spellcross.exe --select-game-data` to reselect retail CZ/EN sources. Display scaling is independent of Windows DPI: see `source/display_settings.h` and [coordinates.md](coordinates.md). Do not commit a machine-specific `config.ini` or personal saves.

## PAK datasets (compatibility import)

Export **All game data** in Spellcross Asset Converter 0.9.10 (or compatible PAK v1 exporter), then use its Validate PAK action. In Reloaded run `Spellcross.exe --select-game-data` and choose **Import Spellcross Asset Converter PAK dataset (compatibility mode)**. Select the directory containing the root `manifest.json` and the referenced .pak files; do not select a single package.

The loader verifies dataset/package schemas, complete-dataset flags, ZIP entry sizes/CRC32, record counts, status totals, original ranges, payload and fallback references. It rejects missing required archives and ambiguous duplicates. No archive-controlled filename is written to disk. Only recognized original FS/FSU archives are materialized, including optional movie/speaker archives when present, into a new `game_data/imports/PAK-*` directory. All configured game-data paths switch together; absent optional audio is disabled rather than taken from an older edition. Original ISO or mounted CD is no longer needed for these archives. The legacy engine reads them through the existing loader. Keep this generated directory; subsequent launches do not need to reimport the PAK files.

This is a **compatibility bridge**, not native PAK gameplay. Modern atlas PNGs, map layers, UTF-8 data, WAV/MIDI and MKV payloads are not consumed by the engine yet. The importer validates ZIP data and basic manifest references, not the complete semantic/atlas contract or authenticity of the original game. Loose CD media not inside recognized archives is not materialized. Partial exports and duplicate source archives are rejected; select one complete edition. Reimport to switch datasets, or select a retail source using the unchanged wizard. No change to the editor or packager.

The next native-runtime milestone is a read-only PAK resource provider and atlas-backed sprite rendering, with original payloads as explicit fallback. Avoid rewriting the renderer or moving campaign logic until that path has been tested side-by-side.

## Automated checks

Run dependency-free Python source checks from the repository root:

```bash
python -m unittest discover -s spellcross-remaster-main/tests -p 'test_*.py' -v
```

`.github/workflows/strategy-regression.yml` compiles additional C++ gameplay tests; `.github/workflows/fullscreen-menu.yml` uses wxWidgets and Windows-specific GUI checks. The whole native game, retail-media integration tests and complete campaign must be tested separately on a suitable Windows machine. Repository path changes also require updating the workflow trigger filters and compiler commands.

The output ZIP/optional SFX installer has `Spellcross.exe`, `data/`, `locales/`, `Editor/spellcross_map_editor.exe` and editor `data/`. Release packages exclude personal configurations, saves and imported retail archives. Detailed verification is in [debugging.md](debugging.md) and [verification.md](verification.md).
