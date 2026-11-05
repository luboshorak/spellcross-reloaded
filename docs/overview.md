# Overview and glossary

## Purpose

[`README.md`](../README.md) describes Spellcross Reloaded as an open-source, fan-made restoration of the 1997 tactical/strategic game for modern Windows. The native application reuses the original game's assets and campaign definitions rather than distributing commercial data: entry point `MyApp::OnInit` ([`source/main.cpp`](../spellcross-map-edit-main/source/main.cpp)); original-data owner `SpellData` ([`source/spellcross.h`](../spellcross-map-edit-main/source/spellcross.h), [`source/spellcross.cpp`](../spellcross-map-edit-main/source/spellcross.cpp)).

`Spellcross.exe` has **two modes of play**: strategic chapters where the player manages territory/companies/research, and tactical missions on an isometric tile map. A separate map editor is available through the `external/map-editor` Git submodule and launch glue in `MainFrame::LaunchStandaloneMapEditor` (`source/main.cpp`).

## Current top-level inventory

| Location | Role | Principal entry |
| --- | --- | --- |
| `spellcross-map-edit-main/source/` | C++17 wxWidgets Windows game, shared asset/model/renderer code | `wxIMPLEMENT_APP(MyApp)`, `MyApp::OnInit` in `main.cpp` |
| `spellcross-map-edit-main/data/` | Source-bundled auxiliary assets, units metadata, menu images, fallback-video manifest | `SpellData` and `packaging/Build-CombinedRelease.ps1` |
| `spellcross-map-edit-main/locales/` | Reloaded-authored UI translations only | `spellcross_ui_locale::Tr` in `source/spell_ui_locale.h` |
| `spellcross-map-edit-main/tests/` | Python source-level checks and C++ isolated/integration tests | `.github/workflows/*.yml` |
| `docs/archive/regressions/` | Historical issue-specific tactical/strategic regression notes | Existing `*.md` files |
| `external/map-editor/` | Independently built editor repository (Git submodule) | Editor `source/spellcross_map_editor.vcxproj` after submodule init |
| `packaging/` | Release packaging, audit notes, versioned release notes | `Build-CombinedRelease.ps1` |
| `spell_extractfs/` | Archive GUI, binary inspectors, map/asset analysis scripts and generated exports | `spell_fs_gui.py`, `spellcross_level_tool_v5.py` |
| `spell_decomp/` | LZ compression/decompression GUI and binary wrappers | `spell_lz_tool_gui.py` |
| `spellcross-master-pytools/` | Older file-format and raw-image analysis utilities | `unfs.py`, `unlz.py` |
| `docs/` | Cross-cutting source documentation (this guide) | `README.md` |

The source-root folder name is historical; it must not be mistaken for the standalone editor executable. The project also tracks example strategic saves and research/analysis files, but the shipping runtime is constructed explicitly by the packaging script, not by copying the whole repository.

## Terms

| Term | Meaning in this codebase |
| --- | --- |
| **Strategic chapter / level** | A `LEVEL_XX.DEF` campaign description parsed by `LevelLoader::LoadLevelDef` (`source/level.cpp`). |
| **Territory** | A chapter sector represented by `LevelTerritory` (`source/level.h`); may have missions, revenue, timers and ownership. |
| **Tactical map** | A tile battlefield loaded by `SpellMap::Load` (`source/map.cpp`), with `.DTA` terrain and `.DEF` mission directives. |
| **Company / unit instance** | A deployed unit with mutable health, AP, identity and combat state (`MapUnit` in `source/spell_units.h`). |
| **Unit type** | Static prototype parsed from `JEDNOTKY.DEF` by `SpellUnits` (`source/spell_units.cpp`); see `SpellUnitRec`. |
| **Strategic UID** | Stable identity passed in `LevelData::PlayerUnitAdd::strategic_uid` from persistent roster to tactical context; distinct from local tactical `MapUnit::id`. |
| **Temporary company** | Chapter support unit, represented by `LevelData::PlayerUnitAdd::temporary` and `StrategicLevelFrame::m_unitStates`, not a permanent recruit. |
| **Formation** | Battalions/regiments/brigades coordinated by commanders; active metadata assembled by `StrategicLevelFrame::GetHierarchyBattleMeta`. |
| **AP** | Action points spent on movement and attacks; methods `MapUnit::GetWalkAP` and `MapUnit::UpdateFireAP`. |
| **Other Side / OS** | Enemy campaign faction and its AI; strategic attacks are stored in `CounterAttackState` (`forms/form_level.h`). |
| **FS / FSU** | Original game archive formats read by `FSarchive` and `FSUarchive`. |
| **L1/L2/L3/L4** | Battlefield ground sprite, object sprite and two animation streams; see `MapSprite`, `MapLayer3`, `MapLayer4`. |
| **Native/original strategic UI** | 640×480 asset-accurate rendering by `StrategicOriginalRenderer` with live overlays and hit testing owned by `StrategicLevelFrame`. |
| **Original game data** | Retail CZ/EN archives supplied by the player; not checked-in replacement commercial media. |
| **Needs verification** | Behavior not established by source/test or original executable evidence; tracked centrally in [verification.md](verification.md). |

The distinction between **original game behavior** and **current Reloaded behavior** matters throughout this guide. The authoritative function for a particular runtime behavior is always the current source; contemporary parity claims require separate evidence.
