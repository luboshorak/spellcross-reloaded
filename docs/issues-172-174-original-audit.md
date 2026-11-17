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
- The supplied EN `COMMON.FS` contains `M09_13.DTA` and
  `M09_13A.DEF`. Missing-data warnings on the CZ installation
  were reported but that Czech archive was not supplied here.

Remake failures: it ignored `WaitForResearch` in the LevelLoader;
research `ORconnections` only checked completed research, never the
Special flags that scripted events grant; those flag mutations on
`m_level` were not persisted in JSON (triggered event IDs are).
An already-visible final territory was incorrectly attackable.

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
drift between screens. An exact comparison of Divac1's saved units
still requires downloading the three issue-attached JSON saves, which
were unavailable through the connected GitHub file interface.

## Regression coverage

- Standalone C++ tests: scripted flag restoration, OR vs AND research
  dependencies, final gate, formation bonus values/non-monotonic example.
- LevelLoader C++ fixture: `WaitForResearch(135,136)` and side effects.
- Source integration Python tests: UI/launch gate, save recovery,
  production use of the shared formation table.
- No original game binary or copyright-protected data is committed.
