# Spellcross Reloaded: game-only refactor test plan

This refactor removes the embedded editor from Reloaded. The optional
external Map Editor remains a separately maintained program.

Removed from the game build:
- GAMEMODEOFF in both consoles, the game/editor switch and editor UI.
- Map-editing mouse branches, selection/copy tools, sprite/object/unit/event
  editor windows, the editor ribbon, and editor-only source modules.
- Dead editor forms and their Visual Studio project/filter entries.

Retained:
- Tactical gameplay, AI, objectives, events, range/fog, HUD, minimap.
- Campaign progression, mission DEF/DTA loading and custom missions.
- Tactical and strategic save/load, cutscenes, audio and console ALLDONE.
- External Map Editor launcher (no live runtime state is sent).

Verify on Windows **before merging**:
1. Compile Release x64 from the existing .sln (legacy project filename kept).
2. Launch new game and strategic campaign; start, win, lose and retreat.
3. Test tactical and strategic save/load including original formats.
4. Test map scrolling, movement, attacking, group movement, ESC and HUD.
5. Open custom DEF missions; test sounds, cutscenes, minimap and options.
6. Confirm ALLDONE works, GAMEMODEOFF is unknown, and no embedded editor opens.
7. Check standalone editor launch from main menu and tactical game.

No Windows build or runtime test has been performed in this environment.
The branch must not be merged until it builds and passes manual testing.

## Original game research — Tutorial and unit deployment

Source references (studied, not redistributed):
- Original English retail archive `SPELL/DATA/COMMON.FS`:
  `MAINM_BG.LZ` = 255x272 7-slot English main menu;
  `TUTMNU.LZ` = 250x214 native tutorial selector.
- Original `TUTORIAL.DEF`: four independent missions TUT01, TUT02,
  TUT03 and TUT04, each declaring `EndOKMission(none)` and
  `EndBadMission(none)`.
- Original `TUT01.TUT`: `TutCommand(0..15)`, text events, M/RMB input,
  `CenterOnSquare(581)`, `LClickOnSquare(868)`,
  `UnitToSquare(48,1115)`, `WaitObjDest(1372)`,
  `SeeSquare(518)`, `UnitToSquare(49,1386)`, `TutorialEnd()`.
- Czech game manual under "ÚVODNÍ ROZMÍSTĚNÍ JEDNOTEK": deployment
  on START squares is required for normal strategic attacks, confirmed via
  an icon; the initial John Alexander mission is the exception.
- The Czech retail main-menu art has six slots, so Reloaded renders a
  seventh Tutorial button without altering the original six bitmaps.
- No retail content (graphics, maps or scripts) is checked into this repo.
  Original tutorial resources are loaded from the configured user-supplied
  game data; editions without them show a missing-file warning.

## Additional test matrix for this feature

1. English ISO: verify **seven** main-menu hitboxes, especially TUTORIAL,
   CREDITS, INTRO and EXIT. Menu resize/scale and hover must align.
2. Select TUTORIAL, see original 250x214 TUTMNU artwork, click each of
   BASIC, FIRST CONTACT, MISSION 1 and MISSION 2 and verify TUT01..TUT04.
   ESC inside the selector should return to the main menu.
3. In TUT01 verify original text, M+RMB input gate, map click, movement,
   object destruction, sight and TutorialEnd; do not call this fully
   verified until playtested with retail data.
4. Czech source: original six buttons remain aligned, supplemental Tutorial
   click/hover works; check behavior when Czech files lack TUTxx.DEF and
   TUTMNU.LZ (copyrighted English assets must not be silently bundled).
5. John Alexander introductory escape mission: starts immediately,
   **without** the deployment UI.
6. From strategy, launch a selected company force: gameplay remains frozen
   before confirmation; START crosses are visible; no modal/auxiliary
   deployment panel is displayed. Click friendly units or use the original
   two top HUD cycling buttons to select a company. Click an empty START
   cross to place it (Shift-click swaps two companies); invalid/occupied
   squares cannot silently reposition the unit.
7. Confirm ONLY with the original flashing **UKONCEN** glyph in the EMPTY
   THIRD top HUD slot (the regular end-turn icon must not appear there).
   Verify MissionStart events, AI, movement and objectives begin only after
   confirmation, and the initial state/save matches the chosen deployment.
8. Repeat with a generated DTA-only defensive/counterattack mission, with
   mixed ground/air unit types, a save/load, RETREAT and mission-end stats.

Windows/MSVC compile and retail-game runtime testing are still required.

## Menu image alignment (English)

The original `MAINMENU.LZ` black cutout begins at (172,165) and spans
255x272 native pixels. `MAINM_BG.LZ` matches it exactly. Previous code
positioned the menu panel at Y=183, leaving an 18-pixel black band above it.
Now English artwork is positioned at (172,165); the six-slot Czech artwork
retains its own existing placement.

The DOS original deployment confirmation graphic is `UKONCEN`, already
exposed as `wm_glyph_end_placement` by SpellData. The correct position is
the third top HUD button, not the right-side end-turn slot.
