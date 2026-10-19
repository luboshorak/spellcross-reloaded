<p align="center">
  <img src="./spellcross-map-edit-main/data/logo.png" alt="Spellcross Reloaded" width="430">
</p>

# Spellcross Reloaded

**Spellcross (1997), brought back to life for modern Windows.**

Spellcross Reloaded brings the original turn-based strategy game to current PCs. Lead the Alliance against the Other Side, manage your army between missions, and fight on the original tactical maps. The aim is to restore the game—not replace its graphics, rewrite its rules or turn it into something else.

The project also includes a standalone **map editor**, so you can explore the original game files and make your own missions.

**Current source version: v0.9.2** · **[Download the latest published release](https://github.com/luboshorak/spellcross-reloaded/releases/latest)** · [Map editor](https://github.com/luboshorak/spellcross-reloaded-editor) · [Report a problem](https://github.com/luboshorak/spellcross-reloaded/issues)

> **You'll need your own original Spellcross game data.** The game itself and the editor are provided here, but the original commercial archives are not included.

## Get started

1. Get the Windows x64 ZIP or optional SFX installer from the [latest published release](https://github.com/luboshorak/spellcross-reloaded/releases/latest). A new source version may appear on `main` before its release packages are published.
2. **ZIP:** extract the whole archive into a **writable folder**, then run the game from there. **SFX installer:** choose a writable installation directory when prompted.
3. Launch `Spellcross.exe`.
4. On first run, load original Spellcross data. Choose **Download CZ ISO** / **Download EN ISO** (with browser fallback if needed), a **raw ISO file** (no mounting required), an original CD/mounted folder, an existing `DATA` directory, or individual game archives.
5. Want to edit maps? Launch `Editor/spellcross_map_editor.exe` from the same folder.

The ZIP requires no installer or registry setup. The optional SFX installer adds a folder picker and desktop shortcut. You may need the [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vc14/vc_redist.x64.exe) on a fresh Windows installation.

## Configuration when building from source

The game **must not write settings into a file tracked by Git**. In a source
checkout, the committed `spellcross-map-edit-main/source/config.example.ini`
is a **read-only defaults template**. At first launch Spellcross creates its
own, git-ignored `spellcross-map-edit-main/source/config.ini`. This single
local file is shared by Debug and Release builds; it stores your imported
ISO/CD archive paths, volumes, display options and other local settings.
Rebuilding or pulling new commits will not overwrite it.

Portable release ZIPs still create a separate `config.ini` alongside
`Spellcross.exe`, exactly as before; the standalone editor has its own
configuration.

**One-time migration for checkouts that already contain the old tracked
`source/config.ini`**: *before updating to this commit*, back up the file
outside the repository, restore the tracked version, pull, and copy your
backup back to `source/config.ini` (which is now ignored):

```bash
cd /path/to/your/spellcross-reloaded
cp spellcross-map-edit-main/source/config.ini ~/spellcross-config.ini.backup
git restore -- spellcross-map-edit-main/source/config.ini
git pull --ff-only
cp ~/spellcross-config.ini.backup spellcross-map-edit-main/source/config.ini
git status --short
```

This one-time backup is important: Git removes the previously tracked
`config.ini` during the pull. **Do not use `git stash pop`** to migrate
the old tracked file after the deletion. Once copied back, playing the game
no longer dirties Git and no `stash` is needed for future pulls.

## Preserving local development notes when updating to v0.9.1

The `spellcross-map-edit-main/` subtree no longer tracks `.md` or
`.txt` notes, including those in its subdirectories. They remain in Git's
history. The repository's `.gitignore` now ignores any local copies.

**Git deletes previously tracked files during a pull even if they become
ignored.** If you want to retain your notes, back them up **outside** the
checkout **before** pulling. In Git Bash, from the repository root:

```bash
mkdir -p "$HOME/spellcross-local-notes-backup"
find spellcross-map-edit-main -type f \( -iname '*.md' -o -iname '*.txt' \) \
  -exec cp --parents '{}' "$HOME/spellcross-local-notes-backup/" \;
git status --short
git pull --ff-only origin main
cp -a "$HOME/spellcross-local-notes-backup/spellcross-map-edit-main/." spellcross-map-edit-main/
git status --short
```

Resolve any pre-existing uncommitted changes before pulling. Do not use
`git reset --hard` or assume `.gitignore` preserves tracked files. Once
restored, the notes will remain on disk but will no longer appear in Git status.
The top-level `README.md` and `packaging/README.md` are retained.

## The game

Reloaded keeps Spellcross's two connected parts:

**Strategic campaign.** Choose which territory to attack, recruit and upgrade units, organize commanders into formations, manage research and resources, and respond to attacks from the Other Side.

**Tactical battles.** Fight on the original terrain with infantry, vehicles, artillery and aircraft. Cover, line of sight, morale, experience, entrenchment, defensive fire and special abilities all matter. The project also restores behavior from the original Czech and English editions, including the English version's eight-group movement controls.

You'll find the original graphics, unit artwork, animations, sounds, music and strategic interface wherever they can be recovered from the game files. The Windows package also includes 37 video fallbacks for cinematics that the old video formats cannot reliably play on modern systems.

## What's new in v0.9.2?

**Original main menu restored further.** Menu highlighting now uses the original game's hover graphics instead of drawn rectangles. Tutorial-menu artwork, redrawing and window scaling have also been improved.

**Faster handling of unseen enemy movement.** The tactical engine can batch movement steps outside the player's view without playing invisible animations or movement audio. Intermediate movement costs, visibility, map events and defensive-fire checks remain part of the simulation.

**Safer pathfinding and unit movement.** A* path priorities and shared path buffers have been stabilized, and occupancy is rechecked at each movement or teleport step.

**Audio and video reliability.** Additional synchronization and lifetime safeguards protect sound streams and callbacks, while legacy video frames are now advanced by a GUI timer rather than audio callbacks.

**More Windows packaging options.** Alongside the portable ZIP, the build can generate a branded WinRAR SFX installer with a selectable destination and desktop shortcut. As before, players must provide their own original game data.

See the [changes since v0.9.1](https://github.com/luboshorak/spellcross-reloaded/compare/v0.9.1...main). The Windows release package should be built and smoke-tested before publishing.

### Previous highlights: v0.9.1

**Improved original-game mechanics.** Tactical movement costs now use the actual action-point cost from each original unit definition. Experience and equipment upgrades modify available action points; sight, radar, high-ground firing distance and formation bonuses have been reviewed against the original game data. Rescue objectives, enemy-death cleanup and turn handling have also received corrections.

**Easier tactical deployment.** The initial tactical view centers on the densest group of START squares, while the flashing placement-finish button remains clickable throughout its animation.

**CZ and EN ISO download.** On first run, download either original game ISO directly from spellcross.czech-games.net, validate it and import it without mounting a virtual drive. If Cloudflare denies the in-app transfer, the wizard offers a browser-based download and imports the selected result. Existing local ISO/CD/DATA import paths remain available.

**More faithful strategic UI.** Restored original-style strategic screens with usable scrollbar thumbs and dragging; removal of the obsolete editor-era Game mode menu; further fixes to tactical/strategic transitions and save support.

**Editor and custom missions.** Launch the bundled standalone map editor from the main menu, open a mission's original DEF/DTA source after save confirmation, or try standalone custom tactical maps with **File → Load custom map…** (`Ctrl+Alt+O`).

This is still an ongoing restoration, not a claim of perfect binary compatibility with the 1997 DOS original.

## Handy controls

| Key / action | What it does |
| --- | --- |
| `Space` | Switch between normal, movement and attack-range modes |
| Hold `M` / `A` | Show movement / attack range |
| `Tab` | Tactical map |
| `I` | Show or hide unit information |
| `O` | Tactical options |
| `Esc` | Cancel the current action |
| `Ctrl+Alt+O` | Open a custom tactical mission (v0.9.1) |

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
