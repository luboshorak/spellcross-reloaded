# Spellcross Reloaded — developer documentation

This is the entry point for developers and researchers working on the **current source tree**, not a substitute for the retail game manual. The source was surveyed on **2026-11-05** at `main` commit `d3f478ca`. Paths in these pages are relative to the repository root; for readability, references written as `source/...`, `forms/...` or `tests/...` **inside a game module discussion** abbreviate `spellcross-map-edit-main/source/...`, `spellcross-map-edit-main/source/forms/...` and `spellcross-map-edit-main/tests/...` respectively. For historical/original-game behavior, see [evidence and verification](verification.md) before treating a reconstruction as definitive.

## Recommended reading order

1. [Overview and glossary](overview.md) — what the repository contains and basic vocabulary.
2. [Architecture](architecture.md) — entry point, runtime ownership, modules and strategic/tactical data flow.
3. [Coordinate systems](coordinates.md) — staggered map tiles, render pixels, UI pixels, scaling and picking.
4. [Rendering and composition](rendering.md) — exact tactical draw sequence, strategic native graphics and caches.
5. [Lifecycle, state and input](lifecycle.md) — timers, mission transitions, AI, worker threads and cutscenes.
6. [Files, assets and persistence](data-formats.md) — retail archives, definitions, localization, saves and cache.
7. [Build and run](build-and-run.md) — Windows prerequisites, local development and release packaging.
8. [Extending the codebase](extending.md) — safe feature/asset/mission changes and regression tests.
9. [Debugging and troubleshooting](debugging.md) — diagnostics, test commands and reported failure modes.
10. [Conventions and engineering decisions](conventions.md) — invariants and compatibility.
11. [Research utilities](tools.md) — Python/extractor programs outside the shipping game.
12. [Repository structure and migration audit](repository-layout.md) — current layout, path dependencies and a safe cleanup sequence.
13. [Needs verification](verification.md) — **centralized list of remaining uncertainties**.

## Existing focused documentation (retained)

- [Commander casualty information](commander-death-notifications.md) — original language tokens, queueing and save behavior.
- [Health indicators, unit activity icons and slope firing](restoration-health-status-terrain.md) — measured native geometry and original executable evidence.
- [Runtime crash diagnostics](runtime-crash-diagnostics.md) — bounded rotating `debug.log` and incident collection.
- [Historical strategic regression plans](archive/README.md) — issues #51–#54, #57 and hidden enemy movement. Some plans record the status of an **older branch**; they are evidence/checklists, not present-tense main-branch status.
- [Game UI localization guide](../spellcross-map-edit-main/locales/README.md).
- [Packaging and release guide](../packaging/README.md), [restoration principles](../packaging/restoration-development-principles.md), and [historical release notes](../packaging/releases/).

## Scope and evidence rules

The **shipping game** is compiled from `spellcross-map-edit-main/source/spellcross_map_editor.vcxproj`; despite the historic project filename its x64 output is `Spellcross.exe`. The **standalone editor** is a separate pinned submodule in `external/map-editor`. Code under `spell_decomp/`, `spell_extractfs/` and `spellcross-master-pytools/` is research/tooling, not automatically part of the game build.

Each guide names source files and functions so it can be updated alongside code. A current implementation, an original-game observation and a proposed improvement are three different kinds of evidence. The pages here are **static source review**, not a Windows build, DOS trace, full campaign replay or proof that every archived note is still accurate.

Start at [overview](overview.md). Report gaps in [verification](verification.md) rather than making undocumented assumptions.
