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
