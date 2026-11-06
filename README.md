<p align="center">
  <img src="./spellcross-map-edit-main/data/logo.png" alt="Spellcross Reloaded" width="430">
</p>

# Spellcross Reloaded

**The classic 1997 strategy game, restored for modern Windows.**

Spellcross Reloaded is an open-source, fan-made restoration of **Spellcross: The Last Battle**. Lead the Alliance through the original campaign, manage your army on the strategic map and fight tactical battles using the original artwork, sound and game data.

**[Download the latest published release](https://github.com/luboshorak/spellcross-reloaded/releases/latest)** · [Report a bug](https://github.com/luboshorak/spellcross-reloaded/issues) · [Map editor](https://github.com/luboshorak/spellcross-reloaded-editor)

## Developer documentation

Start with the [developer documentation index](./docs/README.md) for architecture, coordinate systems, rendering, data formats, lifecycle, build instructions, extension patterns, diagnostics, code conventions, research utilities and the repository-layout audit. Source-level uncertainties are collected in [needs verification](./docs/verification.md).

## v0.9.5

This release improves tactical mouse controls and unifies the remaining native gray settings dialogs with the dark-green strategic UI:

- Restore **right-button drag** to pan the tactical map. **Escape** deselects groups; right-click does not. Edge scrolling has been removed.
- **Left-button drag-select** is now enabled by default and can still be toggled in Options.
- Match the strategy palette in **Audio, Screen, About and Controls** dialogs, including custom green buttons and sliders.
- Improve crash diagnostics with automatic `crash-last.txt` and `crash-last.dmp`, packaged symbols and build identity.
- Harden movement/attack group transitions, overlapping combat animations and fullscreen mouse capture recovery.
- Allow a user-confirmed exit without saving when an unfinished tactical action blocks a safe Continue snapshot.

See the [v0.9.5 release notes](./packaging/releases/v0.9.5.md) and [previous releases](./packaging/releases). For a crash, attach `debug.log` and `crash-last.txt` (or a `.dmp` if needed); see the [crash diagnostics guide](./docs/runtime-crash-diagnostics.md).

## Download and play

1. Download the **Windows x64 ZIP** or **EXE installer** from [Releases](https://github.com/luboshorak/spellcross-reloaded/releases).
2. Extract or install the complete package into a writable folder.
3. Start `Spellcross.exe` and follow the game-data wizard. Select your original CD, ISO or installed `DATA` folder, or use the supported CZ/EN download option.

Original commercial game archives are **not included** in the package. Czech and English editions are supported; Polish and Italian retail-data loading remains experimental.

To switch editions, close the game and start `Spellcross.exe --select-game-data`. A fresh Windows installation may also need the [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vc14/vc_redist.x64.exe).

## Playing and saving

The campaign includes research, recruitment, upgrades, commanders and formations. Tactical battles use terrain, shared visibility, morale, experience and defensive fire.

Open **Help → Controls** in the game for keyboard and mouse instructions. **Alt+Enter** toggles fullscreen. **Escape** closes tactical overlays or cancels the active mode; otherwise it opens the main menu. In the strategic screen, Escape saves progress and returns to the menu.

**Continue** restores the saved campaign or battle after restarting. Finish deployment and let an active action or enemy turn complete before exiting. Keep a backup of saves when updating.

This is an actively developed fan restoration. Please [report bugs](https://github.com/luboshorak/spellcross-reloaded/issues) with the game version, reproduction steps and a save or startup log where possible.

## Map editor

The package includes a standalone map editor under `Editor/spellcross_map_editor.exe`. Create or edit terrain, objects, units and mission events, then load a custom tactical mission in Reloaded with **Ctrl+Alt+O**.

[Map editor repository](https://github.com/luboshorak/spellcross-reloaded-editor) · [Building and packaging guide](./packaging/README.md)

## Credits and license

Spellcross Reloaded is an independent fan project and is not an official Cauldron release.

The project grew from **Stanislav Mašláň's [Spellcross Map Editor](https://github.com/smaslan/spellcross-map-edit)** and research. His original work remains the foundation of the editor and restoration.

The source code is available under the [MIT License](./LICENSE). Original Spellcross artwork, music, cinematics and other commercial assets remain the property of their respective rights holders.
