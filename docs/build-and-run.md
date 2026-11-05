# Configuration, dependencies, build and running

## Real build targets

Despite its historic filename, [`spellcross-map-edit-main/source/spellcross_map_editor.vcxproj`](../spellcross-map-edit-main/source/spellcross_map_editor.vcxproj) compiles the **game** as `Spellcross.exe` for x64. Its `ClCompile` list includes `main.cpp`, `map.cpp`, `spellcross.cpp`, `forms/form_level.cpp` and the other C++ engine modules. The **standalone editor** is a separate pinned Git submodule in `external/map-editor` (`.gitmodules`), compiled using that submodule's project. Both are packaged by `packaging/Build-CombinedRelease.ps1`.

The game project specifies MSVC `PlatformToolset=v145` and x64 `LanguageStandard=stdcpp17`, uses wxWidgets headers and libraries under the `wxwin` property, links Windows `dsound.lib` / `winmm.lib` and includes the vendored `RtAudio` and `cxxmidi` sources. Windows multimedia/Media Foundation is used by `forms/form_video_box.cpp`. No CMake build was found for the native game.

## Requirements

- Windows x64 with Visual Studio C++ toolchain and MSBuild supporting the project's `v145` toolset.
- Compatible wxWidgets MSVC x64 dependencies (for example from a vcpkg `installed/x64-windows` location), passed as `-WxRoot` in the release script or `/p:wxwin=...`.
- Git submodules initialized, including `external/map-editor` and its `data/COMMON.fs`.
- A legally owned original Spellcross CZ/EN game-data set to play. The source does not distribute original commercial archives.
- For a **complete distributable package**, all 37 fallback MP4 files matching `spellcross-map-edit-main/data/movie_fallback/MANIFEST.tsv`; optional WinRAR and its `Default.SFX` for an installer.

## Developer build

From the root in Windows Developer PowerShell:

```powershell
git clone --recurse-submodules https://github.com/luboshorak/spellcross-reloaded.git
cd spellcross-reloaded
msbuild .\spellcross-map-edit-main\source\spellcross_map_editor.vcxproj /m /t:Build /p:Configuration=Release /p:Platform=x64 "/p:wxwin=$HOME\vcpkg\installed\x64-windows\"
```

The project sets its x64 output directory to `spellcross-map-edit-main/builds/x64/Release/`. Debug and Release configurations differ; check the `.vcxproj` rather than assuming a toolchain-specific generated path. This review did **not** run an MSVC build.

## Packaging game plus editor

```powershell
git submodule sync --recursive
git submodule update --init --recursive
.\packaging\Build-CombinedRelease.ps1 -Rebuild -WxRoot "$HOME\vcpkg\installed\x64-windows" -MovieSource "D:\OwnedSpellcrossFallbacks"
```

The actual script parameters are `-SkipBuild`, `-Rebuild`, `-WxRoot`, `-OutputDirectory`, `-MovieSource`, `-NoZip`, `-NoSfx`, `-WinRARPath` and `-PruneOldBuilds`. Use `-NoSfx` to skip optional WinRAR; it does not waive required movie hash checks. The packager reads `source/app_version.h`, validates compiled FileVersion/ProductVersion (including with `-SkipBuild`), verifies SHA-256 for 37 MP4 files, bundles runtime DLL dependencies discovered using `dumpbin` and stages the two executables with their data/licenses. See [packaging/README.md](../packaging/README.md) for complete release, tagging and smoke-test instructions.

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

## Automated checks

Run dependency-free Python source checks from the repository root:

```bash
python -m unittest discover -s spellcross-map-edit-main/tests -p 'test_*.py' -v
```

`.github/workflows/strategy-regression.yml` compiles additional C++ gameplay tests; `.github/workflows/fullscreen-menu.yml` uses wxWidgets and Windows-specific GUI checks. The whole native game, retail-media integration tests, 37-MP4 staging and complete campaign must be tested separately on a suitable Windows machine. Repository path changes also require updating the workflow trigger filters and compiler commands.

The output ZIP/optional SFX installer has `Spellcross.exe`, `data/`, `locales/`, `Editor/spellcross_map_editor.exe` and editor `data/`. Release packages exclude personal configurations, saves and imported retail archives. Detailed verification is in [debugging.md](debugging.md) and [verification.md](verification.md).
