<p align="center">
  <img src="./spellcross-map-edit-main/data/logo.png" alt="Spellcross Reloaded" width="430">
</p>

# Spellcross Reloaded

**Spellcross (1997), brought back to life for modern Windows.**

Spellcross Reloaded brings the original turn-based strategy game to current PCs. Lead the Alliance against the Other Side, manage your army between missions, and fight on the original tactical maps. The aim is to restore the game—not replace its graphics, rewrite its rules or turn it into something else.

The project also includes a standalone **map editor**, so you can explore the original game files and make your own missions.

**[Download Spellcross Reloaded v0.8.2](https://github.com/luboshorak/spellcross-reloaded/releases/tag/v0.8.2)** · [Map editor](https://github.com/luboshorak/spellcross-reloaded-editor) · [Report a problem](https://github.com/luboshorak/spellcross-reloaded/issues)

> **You'll need your own original Spellcross game data.** The game itself and the editor are provided here, but the original commercial archives are not included.

## Get started

1. Download the Windows x64 ZIP from the [v0.8.2 release](https://github.com/luboshorak/spellcross-reloaded/releases/tag/v0.8.2).
2. Extract the whole ZIP into a **writable folder**. Don't run it from inside the archive.
3. Launch `Spellcross.exe`.
4. On first run, point the game to your original Spellcross data. You can select a **raw ISO file** (no mounting required), an original CD or mounted/extracted CD folder, an existing `DATA` directory, or the original archive files.
5. Want to edit maps? Launch `Editor/spellcross_map_editor.exe` from the same folder.

There's no installer or registry setup. You may need the [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vc14/vc_redist.x64.exe) on a fresh Windows installation.

## The game

Reloaded keeps Spellcross's two connected parts:

**Strategic campaign.** Choose which territory to attack, recruit and upgrade units, organize commanders into formations, manage research and resources, and respond to attacks from the Other Side.

**Tactical battles.** Fight on the original terrain with infantry, vehicles, artillery and aircraft. Cover, line of sight, morale, experience, entrenchment, defensive fire and special abilities all matter. The project also restores behavior from the original Czech and English editions, including the English version's eight-group movement controls.

You'll find the original graphics, unit artwork, animations, sounds, music and strategic interface wherever they can be recovered from the game files. The Windows package also includes 37 video fallbacks for cinematics that the old video formats cannot reliably play on modern systems.

## What's new in v0.8.2?

**Easier setup.** Reloaded now reads a standard ISO 9660 `.iso` directly. No virtual drive or manual extraction is needed.

**Game and editor work together.** Open the editor from the main menu without closing the game. While playing a tactical mission, you can also open its **original mission file** in the editor. The game asks whether you want to save first, proceed without saving, or cancel. This opens the source DEF/DTA mission, **not** your current live battle state.

**Try your own maps.** In the tactical window, use **File → Load custom map...** (`Ctrl+Alt+O`) to play a standalone `.DEF` mission. Keep its referenced `.DTA` file available. Custom maps run separately from the main campaign.

**Better map editing.** The included editor has fixes for painting terrain and objects, clearer brush status messages, 90° rotation of supported flat terrain/road brushes (including multi-cell selections), and drag-and-drop movement of supported L2 objects with Undo.

Some editor tools are still experimental. For example, rotating a flat road is supported, but dragging a building baked into the L1 ground layer is not.

## Handy controls

| Key / action | What it does |
| --- | --- |
| `Space` | Switch between normal, movement and attack-range modes |
| Hold `M` / `A` | Show movement / attack range |
| `Tab` | Tactical map |
| `I` | Show or hide unit information |
| `O` | Tactical options |
| `Esc` | Cancel the current action |
| `Ctrl+Alt+O` | Open a custom tactical mission (v0.8.2) |

### Group movement

The English release of Spellcross had a group movement feature that was missing from the Czech version. Reloaded restores it:

| Key / action | What it does |
| --- | --- |
| `1`–`8` | Choose a group |
| Left-click a friendly ground unit | Add or remove it from that group |
| `Shift` + left-click | Add or remove an air unit |
| `Alt+A` | Add all Alliance units |
| Left-click the map | Move the selected group |
| `Alt` + left-click the map | Move while leaving action points for a shot |
| Right-click / `Esc` | Leave group mode |

Units move one after another, not simultaneously, as in the original English game.

## Maps, saves and compatibility

Reloaded has its own save-state format to preserve restored game behavior, and can load supported original strategic saves when the mission data matches. Tactical saves can also be opened from the main or strategic menu.

The editor is a separate application that shares your original game archives. You can edit terrain, objects, units and mission events; see the [editor guide](https://github.com/luboshorak/spellcross-reloaded-editor#readme).

**A note about rotated tiles:** the new 90° rotation tool creates virtual sprite references rather than modifying the original archive files. Maps saved with these references require matching Reloaded/editor versions. They will not open correctly in the original 1997 game, older Reloaded builds or the upstream editor v0.90. Keep backups of your original maps.

## Build it yourself

The repository contains two C++ projects: the playable game and a separately maintained map editor included as a Git submodule. Building requires Windows x64, MSVC/Visual Studio C++ tools and wxWidgets.

```powershell
git clone --recurse-submodules https://github.com/luboshorak/spellcross-reloaded.git
cd spellcross-reloaded
.\packaging\Build-CombinedRelease.ps1
```

If wxWidgets is not detected automatically, pass its location using `-WxRoot`. The 37 fallback videos aren't stored in Git; the packaging script needs local copies and checks their SHA-256 hashes. See the [build and packaging guide](./packaging/README.md) for details, including `-MovieSource` and safe cleanup of older builds.

## Why this project exists

Spellcross has a distinctive atmosphere, original artwork and unusually deep mechanics. Reloaded aims to keep that intact while making the game practical to play and explore today. When the original data or executable tells us how something worked, that's our starting point. Some details still need investigation and correction; this is an ongoing restoration.

The standalone editor is based on [Stanislav Mašláň's Spellcross Map Editor](https://github.com/smaslan/spellcross-map-edit). His reverse engineering and editing tools are an important foundation of this project, and the [Reloaded editor fork](https://github.com/luboshorak/spellcross-reloaded-editor) retains that credit.

**License and game data:** Reloaded source code is provided under the [MIT License](./LICENSE). Original commercial game archives, artwork, music and converted cinematics are not covered by that source-code license. This is an independent fan project, not an official Cauldron release.
