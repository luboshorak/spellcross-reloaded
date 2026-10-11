<p align="center">
  <img src="./spellcross-map-edit-main/data/logo.png" alt="Spellcross" width="430">
</p>

# Spellcross Reloaded

**Version 0.8.1** — a fan-made restoration, modernization and reverse-engineering project for **Spellcross (1997)**.

Spellcross Reloaded started years ago as a collection of tools for decoding and editing the original game data. It has since grown into a playable restoration of the game itself while keeping the map/resource editing and reverse-engineering tools that made the project possible.

The goal is deliberately conservative: **use the original Spellcross data, graphics, rules and UI wherever possible**, restore missing behaviour from the DOS executable and manuals, and make the game comfortable to run on modern Windows without turning it into a different game.

> **Status:** playable restoration in active development. The game is already usable end-to-end, but some edge cases and editor tools are still being refined.

---

## Unreleased v0.8.2 candidate — custom maps and editor workflow

The in-development v0.8.2 branch adds **File → Load custom map...** to the playable tactical window. Choose a standalone tactical **`.DEF`** mission saved by the editor; keep its referenced **`.DTA`** data alongside it. The normal battle loader initializes units, mission events, HUD and game mode. When replacing an already running tactical mission, the game offers **Save and load / Load without saving / Cancel**. Custom battles are detached from the strategic campaign and are intended for testing/user-created missions; **`LEVEL_XX.DEF` strategic definitions are not custom tactical missions**.

The integrated editor also adds drag-and-drop moves of existing **L2 map objects** (with Undo and destination collision checks) and rotates flat single-/multi-cell terrain/road brushes using `R` / `Shift+R`. Layer-L1 buildings are not yet draggable because the original ground underneath cannot be restored reliably. Maps containing newly generated rotated sprites require a matching Reloaded build and cannot be opened by the original 1997 game or upstream v0.90 editor.

These features remain in draft PRs, have not been released in v0.8.1 and require Windows compilation and runtime acceptance testing.

## What's new in v0.8.1

**The playable restoration and Stanislav Mašláň's original map editor are now available together in one portable Windows x64 release.** See the [v0.8.1 release and download](https://github.com/luboshorak/spellcross-reloaded/releases/tag/v0.8.1), or download the file **Spellcross_Reloaded_v0.8.1.zip**.

- **Standalone map editor included:** \`Editor/spellcross_map_editor.exe\`, maintained in the separate [Reloaded editor fork](https://github.com/luboshorak/spellcross-reloaded-editor) and brought into this repository as a pinned Git submodule.
- **Shared original game data:** the game and editor can reuse original Spellcross archive files without maintaining two installations. The editor can locate Reloaded's imported \`game_data/DATA\` archives and read its \`[FILES]\` configuration.
- **Portable setup:** both applications have executable-local configuration files. They can work from an extracted package outside the development checkout; the original CD / mounted ISO \`INSTALL.DTA\` importer is supported by both.
- **Restored tactical save loading from the menu:** a \`.scsave\` no longer requires a tactical map to be open first. The loader can recover a relocated map file by its filename when an older save contains a developer-machine path.
- **All 37 MP4 fallback cutscenes included:** conversions already shipped in v0.8.0 remain part of the combined package; the build script checks their SHA-256 hashes before packaging them.
- **Reproducible combined packaging:** Windows x64 build scripts gather both executables, support assets and runtime dependencies into the versioned ZIP, with a separate tool for cleaning old build folders.

The editor integration preserves **Stanislav Mašláň's original editing functionality and authorship**. It is an additional application, not a replacement for the game's strategic or tactical interfaces.

---

## What works in 0.8.1

### Playable campaign and strategic layer

- Original-style main menu and game flow.
- Strategic campaign map with territory ownership, mission selection and turn progression.
- Mission briefings and debriefings.
- Alliance attacks and Other Side counterattacks.
- Mission objectives, success/failure handling and final-mission flow.
- Money, research points and strategic turn handling.
- Unit recruitment, replacement, rearming and upgrades.
- Research screen and technology progression.
- Army hierarchy/formations with commanders, battalions, regiments and brigades.
- Unit information, statistics, resources/territory management and strategic options.
- Restored strategic UI built largely from the original Spellcross resources instead of newly drawn replacements.

### Tactical battles

- Original terrain, objects, unit sprites, animations and palette data.
- Fog of war, visibility and line-of-sight handling.
- Terrain-aware movement and A* pathfinding.
- Direct and indirect fire, minimum/maximum ranges and air/ground targeting.
- Defensive/reaction fire.
- Unit experience and combat modifiers.
- Restored morale system: combat morale changes, recovery, long-term inactivity penalties, panic/flee behaviour, commander-loss shock and morale-related special attacks.
- Restored entrenchment rules using the original per-unit data and round timing.
- Special unit actions including radar states, transformations, aircraft take-off/landing and original Other Side abilities.
- Reworked enemy AI with contact behaviour, melee closing, mobile/static defence handling and difficulty modes.
- Original enemy-turn HUD overlay.
- Original movement/attack range modes and hotkeys.
- Save/load support for tactical game state.

### English-version group movement

Version 0.8.0 restores the multi-unit movement feature that existed in the **original English release** but was not exposed by the Czech release.

- Eight unit groups (`1`–`8`).
- Add/remove ground and air units from a group.
- Move a whole group toward a remote destination.
- Optional movement that keeps enough AP for one shot.
- Sequential group movement with replanning between units, matching the behaviour recovered from the English executable rather than moving every unit in parallel.
- Group membership is preserved in Spellcross Reloaded save states.

### Compatibility and original data

- Reads the original Spellcross archive/resource formats directly.
- Supports both Czech and English retail data where the underlying content differs.
- Can import required game data directly from an original Spellcross CD or mounted ISO (`DATA\INSTALL.DTA`).
- Can also use an existing installed/copied Spellcross `DATA` directory.
- Support for loading original Czech/English strategic save data, with compatibility checks for mission sets.
- Original sound effects and MIDI music playback.
- Original DPK/DP2 video playback; the proprietary CAN video codec is not fully decoded (audio is supported). The Windows release also bundles **37 MP4 fallback videos** for cinematics that are not reliably decoded from the original files.

### Editor and reverse-engineering tools

**Two distinct applications are now part of the project:**

- **Spellcross Reloaded (`Spellcross.exe`)** is the playable game restoration, with its own strategic and tactical interfaces and the original reverse-engineering tools in its source tree.
- **Spellcross Map Editor (`Editor/spellcross_map_editor.exe`)** is the separately maintained [integration fork](https://github.com/luboshorak/spellcross-reloaded-editor) of [Stanislav Mašláň's editor](https://github.com/smaslan/spellcross-map-edit). It provides map editing, map-layer inspection, unit/event editing, resource viewers and graphics/text import/export tools.

The integrated editor is still experimental in places: terrain autotiling and some advanced mission-editing operations remain incomplete. See the [editor's README](https://github.com/luboshorak/spellcross-reloaded-editor#readme) and the [archived detailed original guide](https://github.com/luboshorak/spellcross-reloaded-editor/blob/main/doc/UPSTREAM_EDITOR_GUIDE.md) for its capabilities and usage.

The two executables share the user's **original Spellcross game archives**, but each maintains its **own configuration**.

---

## System requirements

The current release is intended for **64-bit Windows**.

**Tested on:**

- Windows 10 x64
- Windows 11 x64

You also need the current **Microsoft Visual C++ Redistributable (x64)**:

- Direct Microsoft download: https://aka.ms/vc14/vc_redist.x64.exe
- Microsoft documentation: https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist

And, importantly, you need data from an **original copy of Spellcross**. Copyrighted retail game archives are not distributed with this project.

---

## Installation

1. Download **[Spellcross_Reloaded_v0.8.1.zip](https://github.com/luboshorak/spellcross-reloaded/releases/tag/v0.8.1)**.
2. Extract the ZIP to a normal **writable folder**. Do **not** run the game from inside the ZIP.
3. Install the Microsoft Visual C++ Redistributable x64 linked above if it is not already installed.
4. Run **`Spellcross.exe`** and configure or import the original Spellcross game archives when prompted.
5. To use the separate editor, run **`Editor/spellcross_map_editor.exe`** from the same extracted folder. It should find the game's configured archives automatically.

There is no installer or registry setup required for the game. The release is intended to be portable.

### What is inside v0.8.1?

~~~text
Spellcross_Reloaded_v0.8.1.zip
├── Spellcross.exe                    playable restoration
├── data/                             game support files and MP4 fallbacks
│   └── movie_fallback/               37 converted cutscenes
├── Editor/
│   ├── spellcross_map_editor.exe    original editor, Reloaded-integrated fork
│   └── data/                         editor support files
├── README-FIRST.txt
└── LICENSE-*.txt

After first import:
├── config.ini                        game configuration
├── game_data/DATA/                   your original retail archives
└── Editor/config.ini                 separate editor configuration
~~~

**The editor's `Editor/data/COMMON.fs` is auxiliary editor metadata; it is not the original game's `COMMON.FS`.** Keep the supplied support files with their respective executables. You do not need to copy the original retail archives into the editor directory.

### First launch: importing original game data

If required Spellcross resources are not configured, the game offers two paths:

**Original CD / mounted ISO — recommended**

1. Choose the option to import from the original Spellcross CD or mounted ISO.
2. Select either the CD root or its `DATA` directory.
3. Spellcross Reloaded locates `DATA\INSTALL.DTA`, extracts the installed archives and stores them locally under its `game_data` directory.
4. If the particular CD edition keeps additional archives outside `INSTALL.DTA`, the game also attempts to copy those automatically.

**Existing installation / manually copied data**

You can instead point the game to the required original archive files. Once one file is selected, the loader automatically searches the same directory for the others.

Core files include `COMMON.FS`, `T11.FS`, `PUST.FS`, `DEVAST.FS`, `UNITS.FSU`, `TEXTS.FS` and `INFO.FS`. `SAMPLES.FS` and `MUSIC.FS` are optional if you want to run without sound effects or music.

---

## Useful tactical controls

The UI is intentionally close to the original game, including its keyboard shortcuts.

| Control | Action |
| --- | --- |
| `Space` | Cycle Normal / Movement / Attack range mode |
| Hold `M` | Temporarily show movement range |
| Hold `A` | Temporarily show attack range |
| `Tab` | Tactical map |
| `I` | Toggle unit info bars |
| `O` | Tactical options |
| `Esc` | Cancel current action / leave current mode |

### Group movement controls (English original feature)

| Control | Action |
| --- | --- |
| `1`–`8` | Select/unselect one of eight groups |
| LMB on own ground unit | Add/remove it from the active group |
| `Shift` + LMB on own unit | Add/remove an air unit |
| `Alt` + `A` | Add all Alliance units to the active group |
| LMB on map | Move the active group toward that point |
| `Alt` + LMB on map | Move while reserving enough AP for one shot |
| RMB or `Esc` | Cancel group movement and leave group mode |

A unit can belong to only one group. Firing is disabled while group mode is active, as in the English original.

---

## Save-game notes

Spellcross Reloaded uses its own restoration save-state format for data that the original files cannot represent safely. The loader remains backward-compatible with older restoration saves where possible. As of v0.8.1, tactical saves can also be opened from the main or strategic menu without first loading a map; missing old absolute map paths can be resolved against the configured game data when the map file is available there.

The project also contains compatibility code for original retail strategic saves. Czech and English editions do not contain exactly the same mission set, so incompatible retail saves are rejected instead of silently loading the wrong missions.

---

## Project philosophy

This is not intended to be a visual remake with replacement art or redesigned rules. When the original game already contains an asset, table, animation, sound, UI element or gameplay rule, the preferred solution is to **decode and use the original**.

Behaviour that was missing from the early editor code is reconstructed from several sources:

- the original Czech and English game data,
- the original manuals,
- comparison captures from the DOS game,
- and reverse engineering of `SPELCROS.EXE` where exact behaviour matters.

That approach is why some seemingly small details — enemy-turn HUDs, entrenchment timing, morale thresholds, special attacks, group movement order or formation behaviour — are treated as restoration work rather than redesigned from scratch.

---

## Known limitations

- The project is still under active development; unusual mission scripts and AI edge cases may still expose bugs.
- Some map-editor functions remain crude or incomplete compared with the playable restoration.
- Mission-event/objective editing is not yet a polished general-purpose editor workflow.
- The original proprietary CAN video codec is not fully implemented; CAN audio can be handled, DPK/DP2 playback is implemented and the distributed MP4 fallback pack covers supported converted cinematics.
- Behaviour recovered from reverse engineering is continuously being checked against the original game, so small corrections are still expected.
- Only the Windows x64 builds listed above are currently tested for release use.

If you find a reproducible problem, a save made immediately before/after the failure is extremely useful for debugging tactical state-machine and AI issues.

---

## Building from source

The repository contains **two separate C++ projects**, both built for Windows x64 using MSVC and wxWidgets:

- **Game:** `spellcross-map-edit-main/source/spellcross_map_editor.sln` (C++17), producing `Spellcross.exe`.
- **Editor:** `external/map-editor/source/spellcross_map_editor.sln` (C++20), producing `spellcross_map_editor.exe`. The editor directory is a **Git submodule** linked to the [separate fork](https://github.com/luboshorak/spellcross-reloaded-editor).

The current integrated projects use the **v145 MSVC toolset** for their Windows Release x64 builds. Visual Studio C++ build tools, a compatible wxWidgets installation and its runtime libraries are required. Run the following commands from the repository root in Windows PowerShell:

~~~powershell
git submodule update --init --recursive
.\packaging\Build-CombinedRelease.ps1 -WxRoot "$HOME\vcpkg\installed\x64-windows"
~~~

The script builds both executables and produces `dist/Spellcross_Reloaded_v0.8.1.zip`. If your wxWidgets path differs, supply it through `-WxRoot`. The original video fallback MP4 files are not tracked in Git: the packager needs local copies (for example from v0.8.0) and verifies all 37 against their SHA-256 manifest. See **[combined packaging instructions](./packaging/README.md)** for video source options, a build-only/packaging-only workflow and safe cleanup of old build folders.

The project grew from older experimental reverse-engineering utilities written around 2007–2013. Its restoration increasingly builds on reusable C++ loaders, decoders, renderers and gameplay systems.

---

## Legal / original game data

Spellcross Reloaded is an independent fan project and is not an official release by the original developer or publisher.

This repository does **not** grant rights to redistribute the original Spellcross retail archives; you must provide those from your own copy. The packaged MP4 conversions of original-game cinematics are **not** covered by the source-code MIT license and should not be treated as freely reusable assets.

The restoration/source code in this repository is distributed under the [MIT License](./LICENSE).

---

## Repository

Project home: https://github.com/luboshorak/spellcross-reloaded

Releases: https://github.com/luboshorak/spellcross-reloaded/releases

Latest version: [v0.8.1](https://github.com/luboshorak/spellcross-reloaded/releases/tag/v0.8.1)

Integrated editor fork: https://github.com/luboshorak/spellcross-reloaded-editor

Original editor (Stanislav Mašláň): https://github.com/smaslan/spellcross-map-edit
