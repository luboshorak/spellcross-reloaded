# Issue #57 — defensive battle deployment from original DTA

Issue: https://github.com/luboshorak/spellcross-reloaded/issues/57

## Root cause
The campaign's generated DTA-only defence/recapture path used to select up to
`max(12, chosen companies)` positions from a synthetic lower-map terrain band.
It checked for *zero* strictly passable positions, not whether there were
enough for the roster. A band with seven strict candidates could therefore
expose seven START crosses for nine companies, making `FinishDeployment()`
impossible. The scattered layout also ignored the original game data.

Ordinary tactical DEF missions are different: their authored
`AddStartSquare(...)` directives are loaded by `SpellMap::Load()` and
are intentionally unchanged.

## Data provenance
Each tactical `Mxx_xx.DTA` has two original authored counter-attack layers:
- L5 `counter_attack_post_enemy`: Other Side posts, also used by enemy group AI.
- L6 `counter_attack_post_player`: Alliance counter-attack positions.

Generated DTA-only battles now use **every original L6 coordinate** as the
Alliance START set. Do not invent, add, spread, or truncate positions to
fit the roster. A missing L6 is a data error, not permission to manufacture
a deployment area.

Direct validation against the project's user-provided original English
`COMMON.FS` (all counted coordinates are unique and inside the battlefield):
- `M02_02.DTA`: 50 player positions
- `M03_01.DTA`: 50 player positions
- `M04_02.DTA`: 55 player positions
- `M05_02.DTA`: 75 player positions
- `M05_03.DTA`: 119 player positions

These data checks are not a substitute for verifying the Czech game files.

## Windows / Czech ISO regression checklist (not yet run)
- [ ] Load the reporter's `slot_08.json` from #57; reproduce the defence
      with nine companies, formerly seven crosses.
- [ ] Verify the visible deployment crosses match the original L6 positions
      in that territory's `Mxx_xx.DTA`; count unique, valid locations.
- [ ] Deploy all nine companies, start combat, and finish the mission.
- [ ] Move already deployed companies, including different terrain classes
      and valid air/ground stacking, before confirming.
- [ ] Decline the same defence, then recapture the lost territory using the
      authored L6 deployment positions.
- [ ] Validate tactical retreat still uses the same START squares.
- [ ] Launch regular authored DEF missions: `AddStartSquare()` positions
      must remain unchanged.
- [ ] Test loading the reporter's older strategic save without a new campaign;
      this is a generated tactical layout issue, not a save schema change.
- [ ] Validate unrelated #51 territory-selection problems separately.

Do not merge/release as a verified gameplay fix until MSVC/wxWidgets build
and these tactical runs with the actual CZ ISO succeed.
