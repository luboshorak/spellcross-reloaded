# Original mechanics investigation

Local investigation, 2026-10-30. Game executables were inspected statically;
they were not executed. Decompiled game code is deliberately kept outside this repository.

## Reproducible inputs

- EN SPELCROS.EXE SHA-256: `ff6b6554a2a8c532a2a2ed2c6d437ce63b7fc6b480efb29e9f7e14f441837623`.
- CZ SPELCROS.EXE SHA-256: `9125f7023688387c0078b8841e6f000e0aece49fc2a3e60062738a83e1764ff5`.
- Ghidra 12.1.4; retrokit 0.2.1 commit `7625c6b6a1f29d4793bc528c31621cd9ef784b1d`;
  LinearExecutableLoader, x86 32-bit, Watcom stack convention (heuristically selected).
- REA 6.8.0 is connected, but its target admission rejects LE on all hosts.
  The native decompilation below therefore comes from Ghidra through retrokit, not REA.
- Local workspaces: `C:\Users\nixantora\REAWork\Spellcross\AnalysisEN` and `AnalysisCZ`.

## Reloaded comparison: experience

`SpellUnitRec::CalcExperiencePts` explicitly approximates the original thresholds.
`MapUnit::InitExperience` also approximates the initial points within a level.
Neither formula has yet been validated against an identified original function.
Do not replace those formulas on the strength of string matches or compiler heuristics.

A separate, directly demonstrable Reloaded defect exists in `MapUnit::AddExperience`:
XP is capped at the level-12 threshold, but the level-increment loop previously had
no level cap. `GetNextExperiencePts` clamps every index above 11 to that same final
threshold, so reaching maximum XP caused an infinite loop. The branch limits the
loop to level 12. This is a termination fix, not a claim about recovered original code.

Boundary evaluation with monotonic thresholds covered zero XP, immediately below
and at the first threshold, immediately below the maximum, and the maximum.
The maximum now terminates at level 12; earlier transitions are unchanged.
Release x64 application compilation passed with MSBuild 18 and the installed
wxWidgets/vcpkg dependencies. In-game verification remains pending.

## Original experience routines located

### Instruction follow-up: thresholds implemented

The rank lookup's zero-threshold exception is now preserved in
`MapUnit::AddExperience`: EN `0x881fd` and CZ `0x7fa70` both immediately
return zero-based rank zero when the first threshold is zero, independently
of XP. Reloaded previously advanced to level 12 for an all-zero table.
The one-based level now remains 1 for these types. Positive-threshold types
retain the existing bounded level progression. Release x64 rebuild passed.

Read-only Ghidra disassembly of EN `0x88146` and CZ `0x7f9b9` resolves
the missing x87 calculation. Both multiply `(original_level - 2)` by a
single-precision 0.125, square it, multiply by `(exp_max - exp_min)`, storing
each intermediate to float. Levels below 2 return zero; level 2 returns
`exp_min`; higher levels add `2 * exp_min` before integer conversion.
The constants at EN `0x103ea6` and CZ `0xf39c6` both contain `00 00 00 3e`.
Reloaded now implements this using its zero-based threshold-table index.
The old logarithmic approximation and its division by exp_min are removed.

EN random helper `0x575dc` explicitly returns zero for a zero limit and
otherwise uses the remainder of unsigned 16-bit division by the limit.
Initialization now samples `[0, half_interval)` instead of 10..20 percent.
Reloaded retains its own RNG; this does not reproduce the DOS random sequence.
The initialization caller flags and maximum-rank special cases still merit
additional tracing. Release x64 rebuild passed after these changes.

EN `0x88146` and CZ `0x7f9b9` calculate thresholds. Their floating-point x87
expressions were not recovered completely in the initial C export; inspect
instructions before substituting a formula.

EN `0x881fd` and CZ `0x7fa70` convert points to a zero-based rank. Both read
the first threshold from the unit definition (EN offset 0xc7 with record stride
0xcf; CZ offset 0xc6 with stride 0xce), return rank zero below that threshold,
and search levels 3 through 12 with a bounded loop. This independently supports
the termination boundary used by Reloaded's one-based level representation.

The EN routine immediately preceding `0x88146` calls the threshold calculator,
then, when randomization is enabled, adds the result of `FUN_000575dc` called
with half the difference to the next threshold. Reloaded's `InitExperience`
instead chooses 10..20 percent of that interval. This is a concrete candidate
for parity work, but the random helper's endpoint semantics and caller flags
still need checking before changing game balance.

CZ export completed: 1,694 functions, 25,216 fixups, 3,737,976 bytes of C.
Some functions have decompiler warnings; function counts measure discovery,
not full recovery of original source.

## Next evidence required

### Hit casualty XP award recovered

EN `FUN_0007cc81` at `0x7cc81` and CZ `FUN_00074be3` at `0x74be3`
are structurally identical casualty-XP routines.  The attack resolver passes
the total affected count and the killed subset before mutating the target.
The routine derives XP per member from the target's actual experience divided
by its maximum unit count; only an exactly zero target XP falls back to half
of the first positive threshold.

Killed casualties are weighted by 0.16 and wounded casualties by 0.06 in both
executables.  If the hit destroys the target, the wounded contribution is
discarded and all currently active plus previously wounded members are counted
as dead, matching the original rule that wounded members die with a destroyed
unit.  Intermediate values are stored as single precision and the positive
result is rounded by adding 0.5 before integer conversion.

Reloaded previously awarded one full XP-per-member for each killed member,
ignored wounded casualties, clamped small positive target XP up to half the
first threshold, and calculated the award after target health mutation.  The
`reverse` branch now follows the recovered EN/CZ routine.  In-game comparison
against the DOS release remains pending.

### Reaction queue lifecycle review

Reaction selection changes now invalidate the shared movement-range cache as
well as the attack map, matching ordinary `SelectUnit`. In particular the
restored mover must not reuse a pre-volley range after damage/AP/occupancy
changes. `GetUnitOptions` synchronously flushes this dirty range before using
it for the next click. Null restoration also clears the dirty owner.
Release x64 rebuild passed. The remaining manual regression is to interrupt
movement with a nonlethal volley and click immediately after control returns;
repeat with a killed mover, an exhausted defender, and stacked air/ground units.

`CheckReactionFire` admits only active, idle shooters, but `ProcessReactionFire`
previously did not repeat those checks after popping a delayed shot. It now
checks both unit definitions and the shooter's activity/busy state before
range calculation or `StartAttack_NoHUD`, which overwrites attack animation
state. Invalid entries are skipped using the existing queue-drain path.
Existing live-pointer, death, AP, status, direct-sight and range checks remain.
This is a defensive lifecycle correction, not a recovered DOS rule.
Release x64 compilation passed; an animated multi-shooter gameplay regression
has not yet been performed. No claim of complete reaction-fire parity is made.

This investigation is partial. One demonstrated bug does not establish parity
of the rest of Reloaded with either original release.

| Area | Current evidence | Remaining work |
| --- | --- | --- |
| Maximum XP termination | Reloaded defect demonstrated; original rank loop bounded | In-game regression check |
| Initial XP distribution | Original EN uses half-interval random helper; Reloaded uses 10..20% | Verify helper endpoints, caller flags and CZ counterpart |
| XP thresholds | Original functions located in EN and CZ; Reloaded explicitly approximates | Recover x87 arithmetic and compare per-unit boundaries |
| Kill/wound XP awards | EN 0x7cc81 / CZ 0x74be3 recovered and implemented | In-game comparison against original |
| Attack flags | Reloaded comments leave some meanings uncertain | Trace flag consumers in both originals |
| Spotting/reaction fire | Reloaded has a TODO for newly spotted enemies | Trace original trigger and eligibility rules |
| Morale, damage, movement and AI | No systematic comparison completed in this pass | Independent subsystem investigations |

Identify the original unit-record loader and experience threshold consumers in both
builds, using constants, relocations and caller context. Compare the recovered
threshold table and kill-XP arithmetic with `CalcExperiencePts`, `InitExperience`
and both `AddExperience` overloads before implementing numerical parity changes.
Library signature guesses and recovered prototypes require manual corroboration.
