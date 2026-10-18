# Hidden enemy movement — regression plan

Status: implementation on `fix/hidden-enemy-simulation`; **not yet built or play-tested**.

## Scope

The tactical `SpellMap::Tick()` now batches up to 32 unseen normal enemy movement tiles per tick. It does **not** teleport units from source to destination: every intermediate coordinate is processed with the original AP deduction, `SortUnits()`, visibility update, tile-trigger detection, and `CheckReactionFire()`. Processing yields on visible tiles, reaction-fire candidates, pending/new events, and the step budget.

The same turn-level logic (`EnemyTurnStep()`, tactical groups, attack targeting, AP resets) remains unchanged. Unseen enemy teleport animations are skipped without skipping their single landing event, AP deduction or landing reaction check. Player movement and visible enemy movement still use the ordinary presentation cadence. `MapUnit::PlayStop()` tolerates an absent movement sound stream.

There are intentionally **no changes to enemy gunfire/attack animation policy** in this patch; `StartAttack_NoHUD()` may explicitly reveal an enemy shooter, which is independent of hidden movement.

## Manual acceptance tests

1. **Fully hidden path:** Start an enemy turn with an enemy whose entire path lies outside `unit_view->view == 2`. No movement sound is played; its final location, AP, occupancy and next AI action must match the old version. The turn should complete more quickly.
2. **Hidden → visible:** Let an enemy enter a currently observed tile halfway along a longer path. Process exactly that step, reveal it on the correct tile, then resume normal one-step-per-tick movement and sound. Do not reveal its earlier path.
3. **Visible → hidden:** Let an enemy leave observed terrain. Stop movement sound, do not keep the sprite on the map, and fast-forward subsequent hidden steps. Repeat with a path that re-enters sight.
4. **Reaction fire:** Set an Alliance defender with reserved AP at the boundary of direct sight. Verify initiative roll, valid attack range, number of shots, damage, death cleanup and movement resumption. Crucially, no later movement tile may be processed before the defensive volley completes.
5. **Tile events:** Move an unseen unit through `TransportUnit`/`SaveUnit` and other scripted triggers. Confirm events execute **before** more hidden steps, including cases where they spawn or remove units, end the mission, or change the path.
6. **Path/AP/obstacles:** Check a long route with rough terrain and nearby occupied ground/air tiles. Compare each AP delta and final coordinates/occupancy with the unmodified engine.
7. **Teleport hidden → hidden:** Check that only the departure/landing logic runs, AP is spent and the unit remains hidden. No movement audio, no animated wait.
8. **Teleport hidden → visible:** Arrival presentation is not bypassed and a landing reaction is still possible. For visible → visible teleport, verify original animation timings and audio behaviour.
9. **Non-enemy paths:** Move Alliance units normally, both on regular and teleport paths. Confirm original sound, animation and movement speed.
10. **Stress:** Run a mission with many moving enemies and sparse player vision; repeat 20+ turns. Observe responsiveness and audio-stream count/memory usage; no runaway growth or stuck enemy turns.

## Determinism and investigation

Use identical tactical saves and, when possible, a fixed RNG seed for control and patched runs. Capture per-step `(unit_id, x, y, AP, move_state, visibility, pending_events, reaction_fire_queue.size())` and per-turn loss/mission state. Compare resulting game state, not total animation ticks; deliberate sound/animation timing differences are expected.

For debugging, focus on `SpellMap::Tick()`, `EnemyTurnStep()`, `CheckReactionFire()`, `UpdateEnemyVisibilityFromPlayerView()` and `MapUnit::PlayStop()`. The UI renderer's `TileIsVisible()` is **not** the fog-of-war check.

## Release gate

Do not merge into `main` until a Visual Studio build succeeds and the acceptance tests involving reaction fire, visibility boundaries, teleports and mission events have been performed in an actual game session.
