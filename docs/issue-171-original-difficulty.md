# Issue #171 — original difficulty versus Reloaded

Reported in [#171](https://github.com/luboshorak/spellcross-reloaded/issues/171):
a Czech-ISO player switching from Normal to Hard noticed fewer available tactical
saves, but most Other Side companies were still at experience level 1.

## Source comparison

- Retail source available for this investigation: user-supplied English
  `SPELCROS.EXE` (DOS/4GW, LE; SHA-256
  `ff6b6554a2a8c532a2a2ed2c6d437ce63b7fc6b480efb29e9f7e14f441837623`).
  The Czech executable was **not** part of this supplied archive.
- The original Czech manual describes unit experience/level bonuses and
  mission-unit setup; it does not specify the difficulty-to-XP formula.
- The existing restoration of the DOS difficulty logic in
  `map.cpp::AdjustEnemyExperienceForDifficulty` uses engine values Easy=0,
  Normal=1, Hard=2. The English selector exposes only Normal/Hard (its
  0/1 selection is incremented to engine values 1/2).
- The recovered rule takes the zero-based *average rank* of live Alliance
  companies, subtracts one on Easy, adds two on Hard, clamps the base to 0..12
  and picks either this rank or rank+1 (random 0..2 capped at +1).
  Reloaded represents ranks as experience levels 1..12. A scripted enemy
  already above that computed level is **not** downgraded.
- Hence a battle with only experience-level-1 Alliance companies should
  produce **enemy level 1–2 on Normal** and **enemy level 3–4 on Hard**, unless
  the original mission explicitly assigned higher levels.
- Reloaded already mirrors other difficulty-dependent battle effects:
  accuracy, lethality, extra fatal casualties on Hard, and long-idle morale.
  These are separate from the enemy experience initialization.

The exact original XP formula is carried forward unchanged from prior
reverse-engineering work; the uploaded English binary and surviving manual
were reviewed as source material, but this change does not claim a new
independent DOSBox gameplay or Czech-executable verification.

## Root cause

`MainFrame::StartTacticalGame` enables game mode before the strategic
deployment queue has transferred its Alliance `MapUnit` objects onto the map.
`SpellMap::SetGameMode(1)` already calls `ApplyDifficultyEnemyExperience`,
but `AdjustEnemyExperienceForDifficulty` refuses to roll a level if it finds
no live Allied units. No one recalculated the already-placed enemy companies
after the player completed deployment. The save limit is set separately, so
that setting visibly changed while enemy levels did not.

## Fix

`MainFrame::FinishDeployment` now explicitly calls
`SpellMap::FinalizeEnemyDifficultyAfterDeployment` after placement is complete
but **before** `SaveInitial()` and `MissionStartEvent()`. This:

1. Keeps editor/cancelled deployments from accidentally changing enemy levels.
2. Restores the expected experience distribution on both regular and
   generated strategic missions that require player deployment.
3. Preserves preconfigured higher-level enemy veterans and the serialized
   `difficulty_adjusted` guard (no reroll of the same company).
4. Leaves stand-alone missions, scripted reinforcements, combat calculations,
   strategic state, and the existing tactical save counts unchanged.

## Regression

Run `python3 -m unittest discover -s spellcross-map-edit-main/tests -p
'test_issue_171_difficulty_deployment.py' -v`.

The test compiles the **actual three production methods** with small test
stand-ins and checks their effects on enemy levels. It also asserts that the
real `FinishDeployment` calls finalization after placement and before the
initial tactical save. Native interactive testing against Czech retail data
and resumed campaign saves is still recommended before release.
