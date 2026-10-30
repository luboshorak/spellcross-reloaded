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
