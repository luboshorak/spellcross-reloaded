# Optional Enhanced defensive fire

The Options menu now contains **Defensive fire → Original / Enhanced** in
the main menu, strategic view, and tactical view (CZ and EN).

- **Original** (optional retail-compatible mode):
  identical to the original English DOS game. All units on a faction share a
  first-contact visibility trigger. If a Hummer or radar spots a Wolf rider
  before any Commando is in firing range, no later reaction is triggered merely
  by entering that Commando's range.
- **Enhanced** (default for new installations): scouting, radar, fog and side-wide shared visibility are
  unchanged. Once an enemy moving unit is spotted, each defending company
  gets its **one** reaction-fire initiative contest at the first movement
  step where its weapon can legally fire on that enemy (provided it has AP
  and is active/idle). The opportunity is consumed on the *contest*, whether
  the defender wins or loses; it will not roll on every following tile.
  Separate defenders can each react, and a successful defender uses the same
  volley queue, shot count and damage processing as Original.
- A new faction phase renews opportunities for that phase. The same Enhanced
  rules apply to Alliance and Other Side; this is not a player-only advantage.

**Why no exception list for Hummer, Radar R1/R2, UDES, etc.?** The bug-like
behaviour is a property of the gap between *shared sight* and *legal weapon
range*, not a special property of named scout classes. Hard-coding exceptions
would break new custom units and make radar less useful for indirect fire.
Enhanced keeps reconnaissance intact and fixes the per-shooter opportunity
rather than excluding spotters.

The preference is stored in `config.ini` under
`[STATE] reaction_fire_mode=0|1` and saved at normal game shutdown, just like
the difficulty setting. If the key is absent, **Enhanced** (1) is selected.
An explicit saved **Original** (0) is respected: an update never silently
changes a player's previously selected mode. Players who want Enhanced on
an existing installation with `reaction_fire_mode=0` can select it in Options
once, then exit normally to save the choice. It is intentionally **not serialized into tactical or
strategic saves**, so an old save can be used to compare both modes. Switching
modes resets only the temporary per-phase attempt ledger, not AP, tactical
units, visibility or save data.

## Repro for Divac1 issue #197

1. Build the latest game (including this mode) with the English retail data.
2. Load the original Divac1 save, kill the injured Magotar with the distant
   Commando, end turn, heal the wounded Commando in the subsequent quiet player
   phase, end turn. Keep the Hummer at its original position.
3. In **Original**, expect the observed no-fire behavior: Hummer sees the Wolf
   rider (unit 50, rank 6) at distance 7.21; competent direct-fire defenders
   are still out of weapon range, and early artillery initiative attempts fail.
4. Load that same save in **Enhanced** and replay the same steps. The Hummer
   should still spot the rider early, but later *first* entry into a Commando's
   firing range should give that Commando an initiative contest and potentially
   a shot. Random outcomes vary; a given defender can still lose its sole roll.
5. Verify that attempts do not repeat on every tile; repeat with several
   moving enemies, multiple defenders, indirect fire, full/spent AP, and
   first visibility loss/re-entry.

Checks: `test_enhanced_reaction_fire_mode.cpp`,
`test_enhanced_reaction_fire_wiring.py` and existing original-side-sight
regressions. Full native Windows gameplay with the retail save still requires
player QA. The setting does not alter the **Original** path, which matches the
real DOSBox observation reported for #197.
