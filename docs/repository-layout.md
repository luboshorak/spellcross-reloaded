# Repository layout audit and safe migration plan

## Current layout, as checked in

The GitHub repository was formerly named `spellcross_restoration_tools`; it now resolves to `luboshorak/spellcross-reloaded`. The live game is nested in `spellcross-map-edit-main/`, while a **separate editor** is pinned at `external/map-editor` by `.gitmodules`. Despite that name, `spellcross-map-edit-main/source/spellcross_map_editor.vcxproj` produces `Spellcross.exe`. This naming history is the main reason the repository feels disorganized.

```text
.github/workflows/               CI (path-sensitive source checks)
docs/                            cross-cutting source documentation
external/map-editor/             pinned independent editor submodule
packaging/                       combined release script, release notes, audit history
spellcross-map-edit-main/
  source/                        Windows wxWidgets/C++ game
    forms/                       strategic and tactical wx windows
    spellcross_ui_coordinate_catalog/  extracted QH/LST coordinate research
  data/                          auxiliary assets; fallback-video manifest
  locales/                       UI translations
  tests/                         Python + C++ regressions
  doc/                           historical issue-specific test notes
  tools/                         source-level research script
  save/                          tracked example strategic saves
spell_extractfs/                 Python GUIs, extractors, generated exports
spell_decomp/                    standalone LZ programs and wrappers
spellcross-master-pytools/       older Python format/image utilities
```

The repo also contains `spell_extractfs/build`, packaged `*.exe` tools, IDE `.idea` metadata, generated `__pycache__`, sample saves and experimental binaries. Their purposes differ; do **not** indiscriminately delete them without tracing their consumers and licenses.

## Existing documentation audit

Before this guide, root `docs/` contained three valuable focused documents: `commander-death-notifications.md`, `restoration-health-status-terrain.md` and `runtime-crash-diagnostics.md`. They are preserved and linked from [the index](README.md). The `spellcross-map-edit-main/doc/` issue regressions, `packaging/` original-game investigations and `locales/README.md` are retained as source evidence, not silently merged into unsourced prose.

Some historical notes include a past branch name or an "not yet tested" declaration; those reflect **the note's date**, not necessarily the current main revision. Examples: `spellcross-map-edit-main/doc/HIDDEN_ENEMY_SIMULATION_REGRESSION.md` describes an older experimental branch; `packaging/strategic-localization-review.md` refers to `reverse`. They should be revalidated before a present-tense status claim. `docs/runtime-crash-diagnostics.md` originally mentioned a volatile-threads branch in its heading despite runtime logging now being in main; the heading can be made branch-neutral without changing the underlying diagnostic instructions.

## Proposed physical layout (future, not silently applied)

```text
src/game/                 formerly spellcross-map-edit-main/source/
assets/game-auxiliary/    formerly spellcross-map-edit-main/data/
locales/                  formerly spellcross-map-edit-main/locales/
tests/game/               formerly spellcross-map-edit-main/tests/
tools/archive/            from spell_extractfs/ and spell_decomp/ (after audit)
tools/research/           from spellcross-master-pytools/
tools/coordinates/        extracted coordinate tools/data (after audit)
external/map-editor/      retain pinned submodule identity
packaging/               keep release system
docs/                    canonical documentation, legacy notes indexed
```

This is a *design target*, not an assertion that the repository is already arranged this way. A single bulk `git mv` without path migration would be unsafe.

## Why the source/data folders were **not** moved in this documentation change

- `packaging/Build-CombinedRelease.ps1` directly constructs `$game/source/spellcross_map_editor.vcxproj`, `$game/builds/x64/Release/Spellcross.exe`, `$game/data`, `$game/locales`, `external/map-editor`, editor's `data/COMMON.fs` and `dist`. A source move must rewrite **every** reference and re-test staging.
- `spellcross_map_editor.vcxproj` references every compiled `.cpp/.h`, wx include/lib locations, Windows resources and outputs relative to `$(ProjectDir)` and `$(SolutionDir)`. Move its `.sln` and `.filters` together, adjusting resource paths.
- `.github/workflows/strategy-regression.yml` and `fullscreen-menu.yml` have literal `spellcross-map-edit-main/source` and `tests` paths in both trigger filters and shell compilation. After a rename, a green CI run could be **misleading** if path filters no longer trigger.
- Python source regressions such as `tests/test_original_strategy_scripts.py` and `test_worker_thread_safety.py` compute paths relative to `parents[1]`, while helper utilities have separate assumptions about adjacent executables/images.
- `MyApp::OnInit` / `FindRuntimeRoot` / `FindConfigPath`, `SpellData::SpellData` and strategic `GetStrategicSaveDir` use a stable runtime base and config/data/temp/save locations. Moving binary/data relative to each other can break external game-data imports and Continue even after compilation succeeds.
- Git `.gitmodules` pins the editor path and external source identity. A move must adjust .gitmodules, the index gitlink, build script and clone/packaging instructions.
- Full rebuild of the native Windows x64 game **and** editor, plus legal original CZ/EN archives and fallback MP4s, is required before declaring a physical move regression-free. That validation environment is not available in this documentation review.

## Recommended staged cleanup

1. **Now:** provide a single docs index, document subsystems, leave working source/package paths intact, and add small README entry points in historically named tool/source folders.
2. **After Windows test environment is ready:** add an explicit path-contract/smoke test for game/editor build output, archive loading, localization, Continue, video staging and strategic→tactical→strategic flow.
3. **First code-only refactor:** split oversized `map.cpp` / `forms/form_level.cpp` into tested responsibilities *without* moving the project root; keep save and runtime compatibility untouched.
4. **Separate path-migration PR:** update `.vcxproj`, `.sln`, `.filters`, `.rc`, PowerShell, CI path filters, test fixtures, Python helpers, submodule references and docs atomically with `git mv`.
5. **Release gate:** fresh Windows clean build, test both CZ/EN imports, nine global strategic slots, old tactical/strategic saves, chapter transitions, all cinematics, standalone editor and ZIP/SFX staging. Only then remove any verified obsolete assets/duplicate generated material.

For each future move, add a path compatibility table and verify both **source checkout** and **portable release**. A tidy folder structure is not worth another game regression. Current uncertainties are collected in [verification.md](verification.md).
