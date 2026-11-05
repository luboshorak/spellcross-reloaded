# Debugging, tests and troubleshooting

## Reproduce before changing code

Always record the **source commit/release**, retail data edition (CZ or EN), mission and territory, current game layer, difficulty, and an exact save made before the failure. Attach the saved state and rotating log where appropriate. For gameplay regressions, compare a fixed scenario before and after a change; do not infer success from a non-crashing title screen.

`MyApp::OnInit` starts `spellcross_diag::Initialize` in `source/main.cpp`. The actual diagnostic implementation is `source/runtime_diagnostics.h` and detailed player guidance is [runtime-crash-diagnostics.md](runtime-crash-diagnostics.md). The runtime writes `debug.log` plus `debug.1.log`–`debug.3.log` (512 KiB each); this is **not autosave**. It reports startup, loads, mission/turn transitions, saves, AI, worker stalls, exceptions and clean exit. A crash without a log record may require Windows Event Viewer or a dump.

## What can be tested on which host

| Check | Source / invocation | What it establishes |
| --- | --- | --- |
| Python static/source regressions | `python -m unittest discover -s spellcross-map-edit-main/tests -p 'test_*.py' -v` | Source invariants, event scripts, locale catalogs, thread fields, release metadata; **not** full runtime behavior. |
| Linux/native C++ invariants | `.github/workflows/strategy-regression.yml` | Compiles and runs selected header-only and `LevelLoader` tests; does not build `Spellcross.exe`. |
| wx menu/display tests | `.github/workflows/fullscreen-menu.yml` | GUI and coordinate behavior with wx on Ubuntu; dedicated Windows C++ tests. |
| Full Windows game/editor | `packaging/Build-CombinedRelease.ps1 -Rebuild` | Compiles both native projects and validates packaging requirements with the specified dependencies. |
| Retail integration | `tests/test_hierarchy_roundtrip.cpp`, `test_recapture_mission_lifecycle.cpp`, `test_video_background_audio.cpp` | Requires appropriate native harness or legal retail fixtures; source files alone do not prove they ran. |
| Manual campaign/ISO QA | `packaging/README.md`, `spellcross-map-edit-main/doc/STRATEGIC_ISSUES_51_54_REGRESSION.md` | End-to-end input, map, saving, video and strategic mission lifecycle. |

The automated tests have no single universal runner in the inspected files: check each test's arguments. For example, `test_hierarchy_roundtrip.cpp` expects four paths (retail DATA, auxiliary data, extracted COMMON, writable output directory) beyond the executable; `test_recapture_mission_lifecycle.cpp` expects DATA, auxiliary data and extracted COMMON.

## Symptom → start here

| Symptom | Relevant code and first check |
| --- | --- |
| Original data import failure / mixed CZ-EN text | `EnsureSpellcrossSourceConfiguration` (`source/main.cpp`), `FormLoader`, `SpellData::SpellData`; use `--select-game-data`, verify each archive and edition in `JEDNOTKY.DEF`; see `locales/README.md`. |
| Wrong/missing sprites, strategic colors or menu chrome | `FSarchive::GetFile/LoadFile/DumpToFolder`, runtime `temp/` cache, `StrategicOriginalRenderer::LoadExact/BuildPalette`; confirm original `COMMON.FS` and palettes. |
| Terrain shifted, wrong click hitbox, HUD misalignment | `SpellMap::GetSurfPos/GetPosOrigin`, `MainFrame::TacticalMousePoint`, `spellcross_tactical::MapPixel`, `StrategicLevelFrame::OnOriginalStrategicLeftDown`; see [coordinates.md](coordinates.md). |
| Black tactical map or fog inconsistency | `SpellMap::Render` and `unit_view->view`, `ViewRange` worker; distinguish deployment masking from normal fog and editor debug. |
| Tactical AI hangs, invisible movement stalls | `SpellMap::Tick/EnemyTurnStep/CheckReactionFire/ProcessReactionFire`, worker state; use `debug.log` and historical hidden-enemy acceptance plan. |
| Already-completed rescue repeats after enemy occupation | `ResolveMissionTokenForTerritory`, `SurrenderCounterAttack`, `LoadGeneratedStrategicBattleFromDtaPath`; run `test_recapture_mission_lifecycle.cpp` and its save/reload scenario. |
| Unit names or temporary/formation markers lost | `LevelData::PlayerUnitAdd`, `GetHierarchyBattleMeta`, `RestoreTacticalCompanyIdentities`, `SaveTacticalGameWithCampaignContext`. |
| Losses doubled / different after tactical reload | `SpellMap::RecordMissionLoss`, `GetMissionLossStats`, `SaveTacticalGameWithCampaignContext` and `ReadTacticalCampaignContext`. |
| Background battle audio plays over a movie | `SoundChannels::BackgroundMute` in `source/spell_sound.h`, `FormVideoBox`; run `tests/test_video_background_audio.cpp`. |
| Crash on continue or missing saved map | `SaveContinueGame/LoadContinueGame`, `SpellMap::LoadGameStateFromFile`, legacy map-path relocation and `LoadStrategicStateFromPath`. |
| SFX/ZIP missing runtime DLL or video | `packaging/Build-CombinedRelease.ps1`: `dumpbin` dependency copy, `MANIFEST.tsv` SHA-256 validation, submodule path. |

## When changing behavior

For a timing or race defect, log meaningful **state transitions**, not every 10 ms timer frame. Preserve input/save state before reproducing. Confirm the same battle under saved strategic context, and replay a complete mission result with video followed by a visible strategic frame. Avoid modifying retail data in-place. If code/data and a historical regression note disagree, record the discrepancy in [verification.md](verification.md), test it, then update the note.

Do not report a full game build, Windows test, original DOS comparison or complete campaign verification unless those actions were actually performed.
