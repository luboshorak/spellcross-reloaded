# Tactical click / air-ground selection regression checklist

Scope: original Spellcross tactical controls, **not** the embedded map editor.
The original DOS executable contains the native `i_move`, `i_select`,
`i_attack`, `i_lower`, and `i_upper` graphics. Reloaded loads the
matching `I_*` resources and draws the original `I_TAB` two-choice panel.
The DOS manual explicitly allows one aircraft and one ground unit to share
a tile; it also describes separate air/ground unit selection.

This checklist must be executed in the Windows game build. Source inspection
alone does not establish runtime or visual parity.

| # | Setup / input | Expected result |
|---|---|---|
| 1 | Empty reachable tile; one selected friendly unit | One click starts MOVE; no action popup |
| 2 | Single enemy ground unit in range; no MOVE | One click fires at ground target; no popup |
| 3 | Single enemy aircraft in range; no MOVE | One click fires at air target; no popup |
| 4 | Friendly unit on other layer, with no other valid action | Single click SELECTs that unit |
| 5 | Friendly ground and friendly aircraft stacked; select via layer switch | HUD changes active unit to the chosen layer; AP/weapon range matches selected unit |
| 6 | Same stacked friendly tile; click the other unit | SELECT changes to the unselected friendly unit, never an enemy |
| 7 | Aircraft can MOVE onto an enemy **ground** unit tile and ATTACK it | Native MOVE/ATTACK choices; MOVE shares hex on different layer, ATTACK engages ground |
| 8 | Ground unit can MOVE into a tile with an enemy **aircraft** and ATTACK it | Native MOVE/ATTACK choices; MOVE shares hex on different layer, ATTACK engages aircraft |
| 9 | Enemy ground and enemy aircraft on same tile; both can be attacked | Native LOWER/UPPER choices; each attacks *only* its own layer |
| 10 | Both enemy layers present but only one is attackable | ATTACK targets only the attackable layer |
| 11 | Friendly ground and enemy air on same tile; attacker can attack air | Native SELECT/ATTACK; SELECT friendly ground, ATTACK enemy air |
| 12 | Friendly aircraft and enemy ground on same tile; attacker can attack ground | Native SELECT/ATTACK; SELECT friendly aircraft, ATTACK enemy ground |
| 13 | Another unit of the same layer occupies the target | MOVE is unavailable; no overlapping two aircraft or two ground units |
| 14 | Invalid / unreachable tile while a movement order is already active | Click does not wipe the previous movement order |
| 15 | Select an aircraft and immediately click an ambiguous enemy-ground tile (before the next frame) | Both MOVE and ATTACK choices correspond to the newly selected aircraft |
| 16 | More than two actions on a modded tile (e.g. target/object + selection) | All choices remain accessible using stacked original I_TAB artwork |
| 17 | Open action chooser, press ESC | Popup closes without any selection, movement or attack |
| 18 | Click ambiguous tile at lower-right viewport edge | Entire popup remains on screen |
| 19 | Enemy turn, reaction fire, panic phase | No player tactical orders may be executed |
| 20 | Deployment phase | Original deployment cursor and UKONCEN flow; no tactical action popup |
| 21 | EN group mode | Group membership and group MOVE unchanged; no tactical fire popup |
| 22 | Native single-attack / multiple-target chooser | Cursor shows ground/air/ambiguous attack correctly; correct I_* artwork and hover text |

Implementation notes:
- `map.cpp`: `GetUnitOptions`, `CanSelectUnit`, `SelectUnit`,
  `CanUnitMove`, `MoveUnit`, `Attack`, `OnHUDswitchAirLand`.
- `main.cpp`: `OnCanvasLMouseDown`, `OnCanvasMouseMove`,
  `OnUnitClick_cb`.
- `forms/form_unit_options.cpp/.h`: native action popup, cancel behavior.
- Keep the game-only refactor isolated; do not merge into Maslan's upstream editor.

Validation required: **Windows MSVC Rebuild Solution** and in-game test cases above.
