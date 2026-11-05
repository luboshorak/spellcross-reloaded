# Code conventions and recorded design decisions

## What is present (not an invented style guide)

The C++ application is a mixture of inherited Spellcross Map Editor code and newer restoration code. The MSVC project uses C++17 and Windows/wxWidgets. Inherited methods often return `int` status codes and accept pointers; newer helpers use `bool`, `std::vector`, `std::filesystem`, `std::atomic` and enums. File names and class names are historically mixed (e.g. `spellcross_map_editor.vcxproj` builds `Spellcross.exe`). Do **not** assume a universal formatter, naming rule or architecture boundary that the code does not enforce.

The repository's explicit restoration policy is [`packaging/restoration-development-principles.md`](../packaging/restoration-development-principles.md): prove original behavior where possible, label inference, prefer observable fidelity, preserve compatibility, fix crashes and avoid speculative game-balance changes.

## Rules to preserve when editing

| Rule | Concrete source anchor |
| --- | --- |
| Separate game state from presentation when possible | `StrategicOriginalRenderer` renders static original asset pixels; `StrategicLevelFrame::RefreshOriginalStrategicView` adds dynamic text and hit testing (`source/strategic_original_renderer.*`, `forms/form_level.cpp`). |
| Never tick an invisible tactical battle | `MainFrame::OnTimer` checks `IsShown()` before `SpellMap::Tick` (`source/main.cpp`). |
| Do not mutate finished battles behind overlays/videos | `MainFrame::OnTimer`, `StartMissionEndFlow` and snapshot of `m_mission_earned_units`. |
| Maintain strategic-company identities and temporary scope | `LevelData::PlayerUnitAdd::strategic_uid/temporary` (`source/level.h`), `StrategicLevelFrame::NormalizeStrategicUnitInstances` and `RestoreTacticalCompanyIdentities`. |
| Keep tactical JSON-like header and binary payload compatible | `SpellMap::SaveGameStateToFile/LoadGameStateFromFile` (`source/map.cpp`); appended `TacticalCampaignContext` in `main.cpp`. |
| Keep source edition and authored UI text separate | `SpellUnits::IsEnglish`, `spellcross_ui_locale::SetLanguageFromEdition/Tr`, `locales/README.md`. |
| Enforce palette-index and pixel geometry invariants | `SpellMap::Render`, `StrategicOriginalRenderer::BuildPalette`, `display_settings.h` and `tactical_zoom.h`. |
| Synchronize range/view worker access | `SpellMap::MoveRange/ViewRange`, `HaltUnitRanging` and `tests/test_worker_thread_safety.py`. |
| Avoid invented campaign objectives on generated recapture | `ResolveMissionTokenForTerritory` / `LoadGeneratedStrategicBattleFromDtaPath` and `test_recapture_mission_lifecycle.cpp`. |
| Do not couple optional video muting to user volume | `SoundChannels::BackgroundMute`, `tests/test_video_background_audio.cpp`. |

## Recommended additions for new code

- Name new variables by **space and identity** (`tileX`, `surfaceX`, `screenX`, `strategicUid`) instead of overloaded `x`/`id` when crossing subsystem boundaries.
- Keep source-backed original evidence near a behavior change. Distinguish "original data demonstrates" from "Reloaded currently implements".
- Prefer a narrow testable rule in a header/module over another untested block in `map.cpp` (~17k lines) or `forms/form_level.cpp` (~22k lines at review time).
- Keep persistent models backward-readable, and centralize new conversion code rather than reimplementing scaling in multiple click handlers.
- Preserve copyright/license attribution inherited from Stanislav Mašláň and the current MIT source license. Original game assets have separate owners.
- Review changes touching project paths in `.vcxproj`, `.vcxproj.filters`, `packaging/Build-CombinedRelease.ps1`, `.github/workflows` and tests at the same time.

## Document status

Many `packaging/*.md` and `docs/archive/regressions/*.md` files are dated investigations, not universal design rules. Retain their observations; mark outdated branch/test statements as historical rather than silently rewriting evidence. New claims should cite a file/function and, for parity, original-game evidence. Unresolved cases belong in [verification.md](verification.md).
