# Game source and runtime assets

This historically named directory contains the **Spellcross Reloaded game**, not the standalone bundled map editor. `source/spellcross_map_editor.vcxproj` builds `Spellcross.exe` for x64; entry point is `source/main.cpp` (`MyApp::OnInit`). The independent editor is pinned at `../external/map-editor` and is built by the root release script.

- `source/` — wxWidgets C++ gameplay, tactical map, strategic campaign, rendering, original data import, sound/video.
- `data/` — auxiliary metadata, fonts/menu artwork and fallback movie manifest; original commercial archives are **not** distributed here.
- `locales/` — authored UI translations; see [locale README](locales/README.md).
- `tests/` — source-level Python and C++ regressions referenced by `../.github/workflows/`.
- `doc/` — older issue-specific regression plans. Start with [historical note index](doc/README.md).
- `tools/` — native-data inspection utilities.
- `save/` — tracked example/fixture save data. Player runtime saves are distinct.

Start at the root [developer documentation](../docs/README.md) and [building guide](../docs/build-and-run.md). The shipping release layout and paths are specified in [packaging/README.md](../packaging/README.md). Do not rename the directory/project without migrating MSVC, CI, PowerShell and runtime path contracts listed in [repository-layout.md](../docs/repository-layout.md).
