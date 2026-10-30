# Strategic counter-attacks: static investigation

2026-10-30. Inputs and tooling identities are recorded in
`original-mechanics-parity.md`. Original executables were not run.

## Original evidence

The EN export recognizes Army as command 0x0c (export line 34085).
At export lines 17951..17961 the strategic event flow obtains a territory
through FUN_0004d234, formats `R%02d%02d` into that territory's current
mission slot, writes two mission fields and invokes FUN_00085550 with
the territory, concrete army buffer DAT_007560a0 and count DAT_00756108.
This is an independent generated mission, not selection of the ordinary DEF
merely because that DEF exists. Earlier code populates the army buffer from
the event's retained unit list; a separate probabilistic path generates a force.
The precise target-selection and generated reward algorithms remain unresolved.

## Implemented corrections

- A retained scripted attacking/occupying force selects the generated DTA path
  even when an ordinary conquest DEF exists. Failure to resolve its DTA is
  reported rather than silently substituting an unrelated mission.
- Result handling chooses the newest active attack, matching launch selection.
- An active defending battle does not inherit the ordinary conquest mission's
  EndOK rewards, videos, successor, failure variant or chapter completion.
  Exact generated-battle rewards are still unknown; suppressing ordinary DEF
  rewards avoids attributing an unrelated mission record to the defence.
- New scripted attacks exclude territories with an unresolved active attack.
  This is a remake lifecycle invariant, not a claim about the DOS target algorithm.

## Validation and remaining work

### Generated enemy experience follow-up

The original LevelInit parser writes AttackFlags arguments 3 and 4 to offsets
0x69/0x6b. EN FUN_00085550 reads those exact offsets: base = max(min,
min + random(max-min+1) - 1), then company level = min(max, base+random(3)).
It initializes each company's XP through FUN_000880cc(type, level, 1).
Reloaded now passes these chapter bounds into the generated battle loader and
uses the recovered shared-base/per-company procedure rather than level zero.
LEVEL_05.DEF supplies AttackFlags(12,17,4,8,20,6), hence levels 4..8.
The legacy fallback also no longer treats arguments 3/4 as a special-unit
quantity. Original FUN_0004d3da selects at most one special type; fallback
uses one if the special pool exists. Fallback normal force size remains an
approximation, and retained scripted Army lists still take precedence.
Malformed/custom bounds are clamped to supported levels 1..12.
RNG sequence identity with DOS is not claimed. Release x64 builds passed.

EN FUN_0004d234 selects from a stored territory adjacency bitmask, preferring
frontier sectors adjacent to the territory index stored at chapter offset 3,
then falling back to all frontier sectors. The meaning of that preferred
index still needs tracing; the remake's CLK heuristic has not been changed.

Release x64 MSBuild compilation and diff whitespace checks passed.
Interactive tactical/strategic integration tests have not been performed.
Review scenarios: defend an owned sector with an existing DEF; refuse then
recapture with the retained force; defend the chapter's final sector without
advancing chapter; several Army events in one turn; save/load pending defence.

Not yet fixed: unique persisted battle/attack IDs, old-save army reconstruction
without territory attribution, deferred defence timing, event consumption when
no target exists, original enemy placement/experience and generated rewards.
Newest-first matching plus excluding new duplicates reduces ambiguity but does
not resolve duplicate attacks already stored in old saves. This is a partial
repair, not complete original strategic parity.
