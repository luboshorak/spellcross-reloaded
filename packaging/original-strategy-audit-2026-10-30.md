# Original Spellcross strategic campaign: evidence and regression contract

**Scope:** original CZ/EN retail game data, existing original-save reader, and
Reloaded's source-level strategic event/defence implementation. This is a
correctness repair, not a redesign and not an asserted full DOS emulation.

## Evidence checked (read-only)

- Original CZ `COMMON.FS`, original EN `COMMON.FS`, all ten
  `LEVEL_XX.DEF` scripts in each edition; 57 (CZ) / 59 (EN) occurrences
  of `EndOKEvent(n)` and `EndBadEvent(n)` combined.
- Original CZ and EN `BIG_MAP.SAV` snapshots decoded with the game's LZW
  format. 10 CZ and 22 EN saves contained valid strategic data, including
  levels 2–7 and mid-chapter saves.
- Original Mod Launcher's `SpellSaves.cpp/.h` defines territory flags:
  `0x0F=Player`, `0x0D=Accessible`, `0x0C=Was accessible`,
  `0x00=Enemy`, `0x10=Locked`, `0x1F=Unlocked`,
  `0x1D=Was unlocked (bug)`.
  Only **0x0D** satisfies its `isAccessible()` predicate.
- Original **CZ** `LEVEL_03.DEF`: `Start(1)`, `Start(3)`,
  `Territory(4,m03_04a,m03_04,mus05)`; `Mission(M03_04A)` has
  `Time(5)`, `EndBadMission(M03_04B)`, `EndOKEvent(5)`,
  `EndBadEvent(5)`. `Event(5)` executes
  `ChangeMission(5,M03_05B)`. `Event(9)` is an independent
  `AbsTime(9)` Other Side `Army(...)` attack.
- CZ `LEVEL_02.DEF` uses `EndBadEvent(4)` referring to
  `Event(4) { Time(1); Army(...); }`: an original mission event
  activation must respect the event's own countdown.
- CZ/EN `LEVEL_09.DEF` uses `RunEvents` to activate later
  `Time(N)` events. Executing them on the activation turn is incorrect.

## Corrected invariants

1. The `LevelLoader` retains **both** `EndOKEvent` and `EndBadEvent`;
   a missing `Time` clause is distinguished from `Time(0)`.
2. Mission success, mission failure and expired `Time(N)` deadlines
   activate the referenced original events. Activating a timed event
   schedules it; an untimed, eligible event can run immediately.
3. `AbsTime(-1)` runs the chapter intro on its first turn, not after
   reloading a mid-chapter legacy save with absent event metadata.
4. A timer expiring is **not itself a lost territory**. The
   `EndBadMission` and `EndBadEvent` script changes are applied
   separately from actual enemy control.
5. `RunEvents` activates `Time(N)` events without bypassing their
   delay; the initial activation turn is retained through campaign saves.
6. Declining a generated `Army` defence and ending a turn with a
   still-unresolved accepted defence lead to the same ownership loss
   (only once) while retaining the occupying force for `Rxxxx`
   recapture. The user can cancel the end-turn instead.
7. Importing an original `BIG_MAP.SAV` chooses initial attackable
   territory only from currently `0x0D`-accessible sectors; historical
   `0x0C` and `0x1D` are not currently attackable.

## What this DOES NOT yet prove

- Original DOS-generated `Army` **target weighting** remains only
  approximately reconstructed (see `strategic-counterattack-review.md`).
- Dynamic enemy-force generation, rewards, interactive battle lifecycle,
  original-game display of simultaneous attacks and special legacy saves
  still require Windows playthrough comparison.
- This PR is **not** proof that the 30 October screenshot save has fully
  recoverable ownership, because its `save/strategic/level_03/autosave.json`
  was not part of the submitted artifacts. Preserve a copy before testing.

## Test gate

`python -m unittest discover -s spellcross-map-edit-main/tests -p 'test_*.py'`

Compile and run `tests/test_original_level_events.cpp` against
`source/level.cpp` using C++17 (in CI). These are genuine parser
regressions plus source-invariant checks, not a substitute for an x64
MSVC/wxWidgets build.

**Required in-game cases before merging:** CZ level 03 fresh and saved
turns 3–11; sector 4 timer expiration and `Event(5)` change; successful
and unsuccessful `M03_04A`; Other Side `Event(9)` defence accept and
immediate tactical mission; refusal; accept then skip via End Turn;
victory, loss and subsequent recapture with preserved `Army` list;
save/load in an unresolved defence; chapter 02 delayed `Event(4)`;
chapter 09 staged `RunEvents`; loaded original
`BIG_MAP.SAV` with `0x0C` versus `0x0D` accessibility.

**Merge gate:** do not merge until native Windows build and those
in-game transitions succeed. Preserve existing localized texts, original
archives and saved games unmodified.
