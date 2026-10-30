# Original Spellcross strategic countdown: verification after PR #75

## Authority (NOT the Reloaded `main` branch)

Compared with original **English retail** `SPELL/DATA/COMMON.FS`, the
`LEVEL_XX.DEF` scripts extracted from that archive, original
`SPELL/SAVE/**/BIG_MAP.SAV` data, and the Czech printed manual.
The original saved-game binary format is decoded using the provided
Spellcross Mod Launcher LZW codec (`LZ_spell.cpp`); territory records
start at raw offset `0x48E3` (56 bytes each), territory mission deadline
remaining value is signed i32 at `+0x28`, ownership flags are at `+0`,
and the current chapter's turn is i16 at `0x4835`.

## Primary original-save evidence

| Retail EN save | Chapter | Turn | Territory | Flags | Mission | Remaining Time |
|---|---:|---:|---:|---|---|---:|
| `SAVE/_backup 2/SAVE0002/BIG_MAP.SAV` | 5 | 6 | 6 | `0x00` | `m05_06a` | 10 |
| `SAVE/_backup 2/SAVE0001/BIG_MAP.SAV` | 5 | 9 | 6 | `0x00` | `m05_06a` | 10 |
| `SAVE/SAVE0003/BIG_MAP.SAV` | 3 | 3 | 9 | `0x00` | `m03_09a` | 12 |

In two different original-game chapter 5 saves the inaccessible territory 6
still has a `Time(10)` countdown of **10** across turns **6 -> 9**.
A deadline represented solely by an absolute turn, without pausing while
inaccessible, would incorrectly show 7 at turn 9.

Note: the retail chapter 5 save originates from a game variant that still
contains M05_06A; this mission/territory is commented out in the supplied
English COMMON.FS level script. The **binary save values** are the
primary evidence for the inaccessible-countdown rule, not the commented script.

## Original scripted mission stages (LEVEL_03.DEF)

- `M03_04A`: `Time(5)`; `EndBadMission(M03_04B)`;
  `EndOKEvent(5)` and `EndBadEvent(5)`.
- `Event(5)`: `ChangeMission(5,M03_05B)`;
  `M03_05B`: `Time(4)`, `EndBadMission(M03_05C)`.
- `M03_09A`: `Time(12)`, success -> B, expiry/failure -> C.

`Time(n)` is a property of the **active mission variant**, not an
everlasting property of the geographical territory. Upon an actual
`ChangeMission` to a different variant, the previous stage's deadline
must be discarded and the new one initialized on the change turn.

## Fix and verification

- Imported original remaining deadlines are paused during turns when
  the corresponding territory is not accessible. The remake currently
  derives accessibility from its own owned/frontier visibility model.
- Scripted `ChangeMission` resets the old mission deadline only when
  the mission actually changes; the successor timer initializes in
  the same strategic turn.
- `test_original_strategy_deadlines.cpp` checks the two original save
  observations and the active countdown decrement. It is compiled in CI.
- New warning dialogs honor the selected original game language; Czech
  source text was normalized to UTF-8.

## What still requires in-game verification

The reported screenshots from 2026-10-30 at **round 16 (left 6/right 5),
round 22 (right 9), round 23 (left 7/right 9)** were referenced by the
reporter but not available at the exact given filesystem paths in this
session. Consequently their on-screen territories could not be matched to
original `LEVEL_XX.DEF` mission tokens or to the user's specific save.

The original archive has only single-digit `VM_0.ICO` to `VM_9.ICO`
markers. Reloaded's map currently **clamps all remaining values above 9
to VM_9**, so an apparent `9 -> 9` on two turns does **not** prove
the internal timer failed to decrement. The native DOS display
semantics for values greater than 9 still need visual verification; this
PR deliberately does not invent a new digit-rendering scheme.

This patch is an isolated improvement grounded in original saves, not
a claim of perfect 1:1 DOS event scheduling or validated Windows gameplay.
Do not merge until native build plus a fresh campaign and save/load check.
