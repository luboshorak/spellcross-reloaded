# Rescue and transport objectives — original Spellcross parity

The original English game `DATA/COMMON.FS` contains these mission scripts
(extracted from the user's original distribution, not invented rules).

## Definitive M07_03A reference

```text
M07_03A.DEF:
  AddMissionObjective(SaveUnit,48,Save colonel)
  AddSpecialEvent(SeePlace,1025,20,100%)
  EventData(20): AddSpecialUnit(SpecUnit1,79,1025,10,001,Colonel)
  AddStartSquare(3308,3358,3359,3407,3408,3409,3457,3458,3459,3460)
  ... further AddStartSquare entries through 3908
```

The colonel is an event-spawned **SpecUnit1**, ID **48**, not a static map unit.
`SaveUnit` completes once that *living* unit reaches **any START square**;
it does **not** use EscapeSquare, and it does not require destroying enemies.
The event template is not the playable unit — the spawn code copies it.

`TransportUnit` has a different destination: **ESCAPE squares**.

## Other original missions to regression-test

| Mission | Original objective |
|---|---|
| M04_10A | SaveUnit(48) — major Davidson |
| M06_11A | SaveUnit(48) — engineers |
| M07_03A | SaveUnit(48) — colonel |
| M01_01A | TransportUnit(48) — fight through to base |
| M06_04A | TransportUnit(48) and TransportUnit(49) — convoy escorts |
| M06_06A | TransportUnit(48) — transmitter specialists |
| M08_09A | TransportUnit(48) — satellite repair |
| M09_10A | TransportUnit(48) and TransportUnit(49) — convoy |
| M10_03B | TransportUnit(48) — transport Sword |

## Manual tactical regression

1. Start M07_03A. Before the colonel is revealed, the rescue objective
   must not complete merely because one of your ordinary companies reaches START.
2. Trigger `SeePlace(1025)`; inspect the spawned `SpecUnit1` (ID 48, Colonel).
   Confirm the `SaveUnit` objective is linked to the **live** unit and that
   its own `trig_events` includes the rescue objective.
3. Escort the colonel (not another unit) to any `AddStartSquare` coordinate.
   Objective must complete on arrival; normal successful mission flow follows.
4. Escort to a tile *not* in START: objective must remain incomplete.
5. Move the rescued unit away from START while a completion message is pending:
   the achieved objective must not reset.
6. If the colonel dies before rescue, the objective must not report completion;
   the mission must fail normally.
7. Save and reload after the colonel has spawned: objective must relink by ID
   to the playable copy, not the template. Repeat arrival test.
8. Test M04_10A / M06_11A: same SaveUnit extraction rule (START).
9. Test M06_04A / M09_10A: both transport units must reach ESCAPE squares.
10. Test M01_01A: intro escape still works and uses its own original objective.
11. Test normal missions without SaveUnit/TransportUnit to ensure no unexpected
    mission completions.

## Implementation

- `spell_map_event.cpp::RelinkUnits` searches playable map units first
  and **stops after the first matching ID** (never overwrites a live pointer
  with a same-ID event template).
- `map.cpp::ProcEventsList` and `MissionStartEvent` relink after spawning
  playable instances.
- `spell_map_event.cpp::CheckUnitInPos` latches completed rescue/transport
  objectives and ignores dead units.
- `map.cpp::CheckAndTriggerMissionEnd` verifies the live extraction position,
  even if the movement trigger was missed or a tactical save was loaded.

Windows MSVC Rebuild and in-game tests are required; source inspection by
itself cannot prove runtime behavior.
