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

## Strategic save lifecycle follow-up (M1)

- Loading `mission_flow.counter_attacks` now replaces the prior in-memory vector
  instead of appending records. This makes repeated loads of the same strategic
  snapshot idempotent. It fixes a Reloaded persistence bug, not an asserted DOS rule.
- New `Army` attacks exclude territories with *any* unresolved defence, including
  a scheduled but not yet triggered counter-attack. This prevents the later
  scheduled timer from overriding the already active Army mission token.
- On recapture, completed and untriggered obsolete attack records are removed
  before scheduling a fresh defence, while active attacks are retained for
  explicit mission resolution.

These safeguards do **not** yet provide persisted unique battle identities or
resolve ambiguity in pre-existing saves containing multiple active attacks for
one sector. No in-game strategic regression or Windows build was run in this
follow-up; verify repeated load, an Army event during a scheduled defence,
and the recapture/defence cycle before merging into `main`.

### Tactical launch and roster reconciliation safeguards

The strategic launch path previously retried a failed DEF load with an **empty**
player deployment while retaining the pending list of sent roster indices/UIDs.
That could let a tactical battle proceed with a different force from the one
whose health, deaths and commanders would subsequently be reconciled. The
fallback has been removed: a failed tactical load now reports failure and
clears the pending mission. This is a remake integrity fix, not recovered DOS
behavior.

The post-battle type/order compatibility matcher previously allowed a survivor
with a nonzero strategic UID to be consumed by a different company of the same
type when its own UID lookup failed. Legacy type matching now considers only
tactical companies **without** a strategic UID. New saves must match exact UIDs;
legacy battle saves without tactical UID metadata can still use type/order.

Verification still required at the M1 release candidate: load-failure path,
multiple companies with the same type and different casualties, commanders
attached to a killed company, an old UID-less tactical save, and mission
recovery after a failed load. Static review and GitHub diffs only; no Windows
build or interactive tactical test was performed in this session.

### M1: persisted counter-attack identity (2026-10-30)

Each strategic `CounterAttackState` now has an `attack_id`. The strategic save
stores it under `mission_flow.counter_attacks[].attack_id` and persists
`mission_flow.next_counter_attack_id`, so allocating a new attack after
recapturing/pruning old records does not reuse an earlier ID. Older strategic
saves without IDs are migrated deterministically in file order. The next-ID
allocator is reset when importing an original DOS campaign snapshot.

The pending tactical mission records the active attack ID, and tactical
`.scsave` campaign-context version 4 appends that ID after its existing
v3 fields. The reader still accepts v1-v3 contexts and sets their ID to zero
(legacy latest-first fallback). Modern returns resolve only the recorded
attack ID; a missing/mismatched attack is rejected *before* tactical losses,
EndOK rewards or territorial changes can be applied. This blocks accidental
attribution of one defence to a later attack on the same sector.

This is a **Reloaded consistency invariant**, not a claim that DOS stored
monotonic attack IDs. Windows compilation and in-game coverage remain pending.
Required milestone scenarios include: (1) launch an Army defence, save in
battle, reload and finish; (2) refuse an attack, recapture, trigger a new
attack, save/reload; (3) load a pre-ID strategic save; (4) load a v3 or older
tactical context; (5) verify an orphaned new-format battle cannot award the
conquest mission's EndOK; (6) verify multiple historic attacks for one sector.

### Legacy tactical save UID reconciliation

The pre-context tactical recovery path previously assembled
`sent_unit_indices` in roster order but collected `sent_unit_uids` in tactical
map iteration order. Since post-battle damage/death reconciliation indexes
these two arrays together, mixed company types or a different tactical
iteration order could attribute losses to the wrong roster company.

Recovery now binds each surviving tactical `strategic_uid` to the matching
expanded strategic roster row *and its unit type*, appending the index and UID
as one pair. UID-less legacy companies retain type-based best-effort matching
with explicit zero UID slots. Exact matches are counted first to avoid
allocating the same roster capacity twice to UID-less survivors. Missing
companies in old saves remain fundamentally ambiguous and cannot be perfectly
reconstructed from survivor-only data.

This was inspected statically only. Include a legacy mixed-type save with
interleaved roster/tactical order in milestone regression testing, alongside a
battle with duplicate companies of the same type and different commanders.

### M1 legacy overlapping defences and battle restoration

When recovering a battle from older `.scsave` files with no embedded campaign
context, the reconstructed pending mission now adopts the newest active
strategic attack ID for the recovered territory, if one exists. It is still a
best-effort recovery; the original save does not identify which historical
attack launched the mission.

The scheduled defence trigger now refuses to activate another pending defence
on a territory that already has an active one. This protects imported/legacy
saves containing overlapping records from duplicate prompts and mission-token
overwrites. It leaves the waiting defence pending until the active defence is
resolved, rather than silently consuming a scripted event. Both changes are
Reloaded lifecycle safeguards; no claim of original DOS parity or in-game
validation is made.

### M1 Army event preflight

The earlier retry-only-when-Army-only workaround was insufficient: mixed events
with `Army` alongside `AddUnitToPlayer`, `RunEvents`, `ChangeMission` or
other actions were consumed when they had no valid attack target, losing their
Army action permanently. Retrying such events after executing their other
actions would duplicate units or event triggers.

Target discovery and the random target draw now happen *before* marking the
whole event consumed and before executing any side effects. If no sector is
eligible, the entire event remains pending for a later turn. Once a target is
found, all actions execute once and the selected territory is used for the
Army attack. This also handles a chapter with no owned eligible territory.

This is a safe campaign lifecycle correction in Reloaded; exact DOS event
ordering and RNG consumption remain unverified. Verify a timed Army-only event
and a mixed Army/AddUnit/RunEvents event with initially no eligible sector,
then territory conquest and save/reload before eventual activation.
