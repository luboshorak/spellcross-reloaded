# Historical investigations and editor screenshots

This directory retains historical project evidence without leaving orphan documentation folders inside the active C++ game tree. These files are **archived**, not current regression test results.

## Regression investigations

- [Hidden enemy movement](regressions/HIDDEN_ENEMY_SIMULATION_REGRESSION.md) — old experimental branch and manual acceptance plan.
- [Strategic issues #51–#54](regressions/STRATEGIC_ISSUES_51_54_REGRESSION.md) — counterattack, casualty, video and save test plans.
- [Issue #57 defence positions](regressions/STRATEGIC_ISSUE_57_DEFENCE_DEPLOYMENT.md) — documented original DTA L6 positions and unrun Windows/CZ test checklist.

## Map editor screenshot history

The three original screenshots are retained as versioned reference evidence, not shipped runtime assets: [v1](editor-screenshots/spell_map_edit_v1.png), [v2](editor-screenshots/spell_map_edit_v2.png), [v3](editor-screenshots/spell_map_edit_v3.png).

The original paths were under `spellcross-map-edit-main/doc/`; no game source, test or release binary uses this archive as a runtime dependency. See [current developer documentation](../README.md) and [verification register](../verification.md) for the present implementation and open tests.
