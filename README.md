<p align="center">
  <img src="./spellcross-map-edit-main/data/logo.png" alt="Spellcross Reloaded" width="430">
</p>

# Spellcross Reloaded

**An unofficial, open-source restoration of *Spellcross: The Last Battle* (1997) for modern Windows.**

Spellcross Reloaded brings the original turn-based strategy game to a native Windows application. Command the Earth Alliance against the Other Side, manage your forces on the strategic map, and lead them through tactical battles. The project uses the original game's campaign data, graphics and sound rather than replacing them with a new game.

**[Releases](https://github.com/luboshorak/spellcross-reloaded/releases)** · **[Report an issue](https://github.com/luboshorak/spellcross-reloaded/issues)** · **[Map editor](https://github.com/luboshorak/spellcross-reloaded-editor)**

## Download and play

1. Download a Windows **x64** package from [Releases](https://github.com/luboshorak/spellcross-reloaded/releases). The ZIP is portable; some releases also provide an EXE installer.
2. Extract the complete ZIP to a writable folder, or use the installer if one is available.
3. Run `Spellcross.exe`. On first launch, follow the setup prompts to locate your **original Spellcross game data** (for example, from a CD, mounted ISO or existing installation).

**The original commercial game data is not included.** The Czech and English editions are the supported sources. Polish and Italian game-data compatibility is still experimental and should not be considered fully supported.

To choose another game-data source later, close the game and run:

```text
Spellcross.exe --select-game-data
```

If Windows reports missing Microsoft Visual C++ runtime DLLs, install the [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vc14/vc_redist.x64.exe).

## Gameplay

Spellcross combines two layers of turn-based strategy:

- **Strategic campaign:** conquer and defend territories, recruit and replenish units, develop new technologies, purchase upgrades and organize commanders and formations.
- **Tactical battles:** deploy companies, use terrain and visibility to your advantage, and manage movement, attacks, morale and unit experience.

Reloaded also adds modern display scaling, fullscreen support, mouse controls and in-game help while retaining the original game's presentation.

This is an **actively developed fan restoration**, not a finished commercial remaster. Some mechanics and edge cases may differ from the 1997 game, and bugs or regressions are still possible.

## Controls and saved games

Open **Help → Controls** in the game for the current mouse and keyboard shortcuts. **Alt+Enter** toggles fullscreen. Tactical camera movement supports right-button dragging; group selection can be enabled or disabled in Options.

Use **Continue** to resume saved progress. Let active actions and enemy turns finish before leaving a tactical battle, and **back up your saved games before updating** to another build.

## Map editor

Release packages include a separate editor at:

```text
Editor/spellcross_map_editor.exe
```

It can be used to work with tactical maps, units, terrain, objects and mission data. In Reloaded, **Ctrl+Alt+O** opens a custom tactical map.

The editor is maintained in its [own repository](https://github.com/luboshorak/spellcross-reloaded-editor).

## Bugs, development and building

Found a problem? [Open an issue](https://github.com/luboshorak/spellcross-reloaded/issues) and include the game version, what you were doing and how to reproduce it. For crashes, `debug.log` and `crash-last.txt` can help; see the [crash-reporting guide](./docs/runtime-crash-diagnostics.md). Review diagnostic files for private information before sharing them.

For source code and technical documentation, start with the [developer guide](./docs/README.md). Windows build requirements, packaging and release instructions are in the [build guide](./packaging/README.md). Changes specific to individual versions belong in [release notes](./packaging/releases/), not on this page.

## Credits and legal notice

Spellcross Reloaded is an independent fan project, **not an official Cauldron release**.

The project builds on the work of **Stanislav Mašláň**, author of the original open-source [Spellcross Map Editor](https://github.com/smaslan/spellcross-map-edit), and the contributors to this restoration.

The project's source code is available under the [MIT License](./LICENSE). **That license does not cover the original Spellcross game data, graphics, music, sound, video or trademarks**, which remain the property of their respective rights holders.
