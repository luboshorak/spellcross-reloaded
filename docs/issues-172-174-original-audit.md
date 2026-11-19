# Issues #172–#174: retail-data audit and regression contract

Source baseline: Spellcross Reloaded v0.9.6.6, `main` (2026-11-14).
Reference: user-provided English DOS retail `SPELCROS.EXE` (DOS/4GW)
and `DATA/COMMON.FS`. The source data, not a manually guessed
flag/mission list, governs these mechanics.

## #173 Research keys and #174 final territory

Verified against **retail** `COMMON.FS`:

- `LEVEL_09.DEF`: `Mission(M09_11A)` calls `EndOKEvent(11)`,
  which calls `SetResearchFlag(138)`.
- `Mission(M09_12A)` / `(M09_12B)` call `EndOKEvent(12)`,
  which calls `SetResearchFlag(139)`.
- `RESEARCH.DEF` `Item(135)` is the first mountain key:
  `Flags(Info)`, `Group(Global)`, `Time(48)`, `Level(9)`,
  `ORconnections(138)`.
- `Item(136)` is the second key and similarly requires
  `ORconnections(139)`.
- `Item(138)` and `Item(139)` are `Flags(Special)`,
  **not research projects**; they unlock the projects.
- `LEVEL_09.DEF` `Event(13)` has `WaitForResearch(135,136)`,
  `SetPlayersTerritory(4)` and `SetResearchFlag(115)`.
  The mission chain culminates at `End(13)`.
- **Follow-up (2026-11-19):** retail `LEVEL_09.DEF` also declares
  `SetSpecialTerritory(4)`. Its `M09_04A.DEF` is a 169-byte
  `MissionParameters`-only file without `MissionData`. Sector 4
  is *awarded by script*, never a tactical conquest or defence map.
  The original executable/COMMON.FS includes full `M09_13A.DEF`
  with `MissionData`, the genuine final battle.
- The supplied EN `COMMON.FS` contains `M09_13.DTA` and
  `M09_13A.DEF`. Missing-data warnings on the CZ installation
  were reported but that Czech archive was not supplied here.

Remake failures: it ignored `WaitForResearch` in the LevelLoader;
research `ORconnections` only checked completed research, never the
Special flags that scripted events grant; those flag mutations on
`m_level` were not persisted in JSON (triggered event IDs are).
An already-visible final territory was incorrectly attackable.
A subsequent user screenshot exposed a second bug: `SetSpecialTerritory(4)`
was ignored by the LevelLoader, so clicking Attack on sector 4 (before or
after research-awarded ownership) tried to open a MissionParameters-only
DEF, raising `MissionData section not found`. The follow-up patch now
parses Special territory IDs (regardless of statement order), disables
manual attacks there via the shared launch gate (native UI and handler),
and excludes them from scripted/random enemy counterattacks, retiring
invalid legacy attack records without surrendering a scripted sector.

Fix: preserve `WaitForResearch` as an ALL-completed condition; calculate
effective Special flags from *persisted* triggered event IDs; distinguish
Special flags from completed research; process research-triggered events
immediately when research finishes; and gate the finale until its
research-to-territory event is satisfied. Show an in-game lock notice
instead of exposing a missing-map diagnostic prematurely.

## #172 Commander/formation stats

The supplied `BONUSES.DEF` grants experience bonuses, e.g. level 2
+1 attack/+1 defence, level 5 +2/+3. Retail `FORMACIE.DEF` adds
battalion +1/+1, regiment +2/+1, brigade +4/+3, using the *highest*
active formation, not the sum of lower formations. A lower-level MLRS
in an active brigade can therefore display a higher total attack than a
higher-level unit fighting alone. This is expected retail behavior and
must not be 'fixed' by forcing level-monotonic stats.

Before this patch the strategic and tactical code duplicated the retail
bonus numbers. Both now use one shared table and tactical Info names the
active formation modifier separately. This guards against numerical
drift between screens. A player-provided `slot_09.json` snapshot (2026-11-16 13:49:10,
turn 11, Czech ISO) has since been analyzed **locally**. This is the
late chapter-9 state for #173/#174, not necessarily the earlier
snapshot originally attached to #172. To respect the reporter, the
complete save is not checked into this public repository.

The save has owned territories `[1,2,3,9,11,8,12]`, triggered
events `[0,1,2,4,11,12,15,16]`, neither research 135 nor 136
completed, and Event(13) absent. The retail research prerequisites
therefore both evaluate available, but the final research gate remains
closed. At research output 66/turn and cost 48 per key, each key can
complete in one allocated turn when selected (barring other in-game
constraints). After both complete, Event(13) should open territory 4.

The same save contains **13 MLRS** (retail unit ID 28), with levels
5–8. All have identical `Upgrade(2)`, an engine-only upgrade
(`Move(1)`, no Attack/Defence change), and are placed in fully
populated battalions/regiments/brigades with living assigned officers.
Retail `JEDNOTKY.DEF` (EN) gives MLRS base armored attack 15 and
defence 8. For a *fully participating* brigade, expected values are:

| MLRS XP level | Armored attack | Defence |
|---|---:|---:|
| 5 | 21 | 14 |
| 6 | 22 | 15 |
| 7 | 23 | 15 |
| 8 | 23 | 16 |

This snapshot exhibits **no XP-inversion among MLRS at equal formation
and upgrade status**. Notably, every MLRS is on cooldown (mostly 1
turn, one on 2 turns); the saved strategic hierarchy does **not**
guarantee a brigade bonus in the next battle unless its constituent
units are actually deployed. This is a comparison with EN retail
values; the full Czech retail binary/content and separate #172 save
were not available for direct cross-verification.

## Regression coverage

- Standalone C++ tests: scripted flag restoration, OR vs AND research
  dependencies, final gate, formation bonus values/non-monotonic example.
- LevelLoader C++ fixture: `WaitForResearch(135,136)` and side effects.
- Source integration Python tests: UI/launch gate, save recovery,
  production use of the shared formation table.
- No original game binary or copyright-protected data is committed.
