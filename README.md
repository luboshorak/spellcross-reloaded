<p align="center">
  <img src="./spellcross-map-edit-main/data/logo.png" alt="Spellcross Reloaded" width="430">
</p>

# Spellcross Reloaded

**The classic 1997 strategy game, brought back to life for modern Windows.**

Spellcross Reloaded is an open-source, fan-made restoration of **Spellcross: The Last Battle**. Command the Alliance against the invading Other Side, build your army, and fight your way through the original campaign.

The idea is simple: **keep Spellcross looking, sounding and playing like Spellcross** — with its original artwork, atmosphere and gameplay, but without needing an old DOS computer.

**[Download the latest release](https://github.com/luboshorak/spellcross-reloaded/releases/latest)** · [Map editor](https://github.com/luboshorak/spellcross-reloaded-editor) · [Report a bug](https://github.com/luboshorak/spellcross-reloaded/issues)

## Back to the battlefield

**Lead the campaign.** Choose your battles on the strategic map, recruit and upgrade troops, research new technology, manage your resources and organize commanders into formations.

**Fight tactical battles.** Use infantry, tanks, artillery and aircraft on the original maps. Terrain, visibility, morale, experience and defensive fire can make all the difference.

**Enjoy the original Spellcross.** Reloaded uses the original graphics, animations, sounds, music and game data wherever possible. It supports both the Czech and English editions and includes converted versions of 37 original cinematics in the Windows package.

## Download and play

1. Download the **Windows 64-bit** ZIP or EXE from [GitHub Releases](https://github.com/luboshorak/spellcross-reloaded/releases).
2. If you chose the ZIP, extract it into a writable folder. If you chose the EXE installer, select where you want the game installed.
3. Start `Spellcross.exe`.
4. On first launch, follow the game-data wizard. You can download a supported Czech or English ISO through the wizard, or point it to your original CD, ISO or existing Spellcross `DATA` folder.

**Switching Czech/English editions:** Close the game and launch `Spellcross.exe --select-game-data` to reopen the source wizard, even if the configured archives are still valid. Select a *complete* original ISO/CD or installed `DATA` directory. Reloaded does not silently mix archives from two editions or translate names inside the original `COMMON.FS`; the retail names come from `JEDNOTKY.DEF`. Imported editions are stored in separate folders under `game_data/imports/` so switching versions or a failed import does not overwrite your existing game data.

You may also need the [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vc14/vc_redist.x64.exe) on a fresh Windows installation.

**Please note:** Original commercial Spellcross game data is not included in this repository or release package.

## What's new in 0.9.2.2?

This maintenance release focuses on more reliable strategic counter-attacks and tactical battles, without changing the original game's look or assets.

- **Counter-attacks and deployment:** defensive battles now use the original map-authored deployment positions, and mission definitions load correctly regardless of whether the `.DEF` extension is uppercase or lowercase.
- **Correct combat range:** melee units such as Magotar no longer gain unrealistic long-range attacks from elevation differences.
- **Smarter enemy reactions:** enemy groups can respond after being attacked even without immediately spotting the attacker, and units no longer unnecessarily step away from an already reached movement destination.
- **Full battlefield access:** the tactical camera can reach valid tiles near the map edges, including units arriving there through group movement or enemy AI.
- **Safer unit placement:** fixed out-of-bounds memory access when searching for free placement tiles near the map boundary.
- **Quiet menus:** battlefield ambient sounds and the tactical simulation stop while the tactical window is hidden behind the main menu or strategic screen.

[Read the v0.9.2.2 release notes](./packaging/releases/v0.9.2.2.md) for the complete list of fixes.

## What's new in 0.9.2.1?

- **Strategic counter-attacks and recaptures:** fixed generated `Rxxxx` mission map lookup, improved attack target selection and corrected crossed-swords indicators.
- **Shared campaign save slots:** nine slots now follow the player across strategic regions, with non-destructive migration of previous level-specific slots.
- **Consistent tactical loss statistics:** restoring an in-game tactical save restores the casualty history of that saved position.
- **Region transition videos:** sequential cutscenes are queued and shown above the strategic map rather than playing audio behind it.
- **Build diagnostics:** packaged `BUILD-INFO.txt` records source revision and executable checksum; full builds can use `-Rebuild`.

## What's new in 0.9.2?

- **A more authentic main menu**, with the original hover artwork, improved tutorial screens and smoother rendering.
- **Faster enemy turns**, especially when enemies are moving outside your field of view.
- **More reliable battles**, with fixes to unit movement, pathfinding and occupied tiles.
- **Sound and video stability improvements**, reducing problems with playback and timing.
- **Easier Windows installation**, with a portable ZIP and an optional graphical EXE installer.

Spellcross Reloaded is still being developed and improved. Some missions and less common game situations may still have bugs.

## Make your own maps

The download also includes a **standalone map editor** based on Stanislav Mašláň's original Spellcross Map Editor. You can edit terrain, buildings, units and mission events, then try your own tactical missions in Reloaded.

Launch `Editor/spellcross_map_editor.exe`, or open a custom tactical mission in the game with `Ctrl+Alt+O`.

[Visit the map editor project](https://github.com/luboshorak/spellcross-reloaded-editor)

## Useful controls

| Key | Action |
| --- | --- |
| `Space` | Switch between normal, movement and attack modes |
| Hold `M` / `A` | Show movement range / firing range |
| `Tab` | Open the tactical map |
| `I` / `O` | Unit information / tactical options |
| `1`–`8` | Choose a unit group (English-edition feature) |
| `Esc` | Cancel or leave the current mode |
| `Ctrl+Alt+O` | Load a custom tactical mission |

## About the project

Spellcross Reloaded is an independent fan project, not an official release by Cauldron. It grew out of reverse-engineering and map-editing tools and continues to aim for a faithful restoration rather than a modern redesign.

Special thanks to **Stanislav Mašláň**, whose [Spellcross Map Editor](https://github.com/smaslan/spellcross-map-edit) and research laid the groundwork for this project.

The project's source code is available under the [MIT License](./LICENSE). The original Spellcross game, artwork, music and other commercial assets remain the property of their respective rights holders.

Want to build it yourself? See the [building and packaging guide](./packaging/README.md).
