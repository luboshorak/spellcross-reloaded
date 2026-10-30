# Spellcross Reloaded — restoration and engineering principles

Agreed project direction, 2026-10-30. Apply to both `main` and the experimental
`reverse` branch; this document is initially introduced on `reverse`.

## Goal

Reconstruct as much of the original Spellcross's **observable gameplay,
mechanics, AI, strategic decisions, pacing, and campaign flow** as is useful,
while delivering a dependable, completable modern game. The original DOS game
is a sophisticated engineering reference, not a presumed fragile or poorly
designed program. Spellcross Reloaded has already been completable for many
versions; simple completable/not-completable status is not the restoration goal.

## Priorities and tradeoffs

1. Original EN/CZ executables, decompilation/disassembly, original game data
   and manuals carry substantial evidentiary weight. They are **not absolute
   dogma** when a justified modern implementation improves Reloaded.
2. Favor **fidelity of observable behavior and player feeling** over literal
   replication of implementation details. Reconstruct subsystems as needed:
   tactical AI, movement, LOS, attacks, morale, strategic campaigns,
   counter-attacks, research, economy, progression and related rules.
3. Apply **proportionality**: if a simpler implementation already reproduces
   approximately 90% of a behavior and the remaining gap adds negligible
   player value at disproportionate complexity or regression risk, document
   that tradeoff instead of automatically replicating every detail.
4. Fix demonstrated and plausible dangerous bugs proactively, especially
   crashes, corrupted saves, broken campaign transitions, duplication and
   invalid state. Preserve existing working behavior and old saves where
   reasonably possible.
5. Modernize implementation, diagnostics, persistence, safety, portability
   and engineering practices when useful. Do **not** invent significant game
   mechanics or silently change game balance/flow without evidence and
   rationale. Modern technical approaches need not mimic 1997 internals.
6. Maintain reliable campaign completion and reasonable playing experience as
   **non-negotiable baseline properties**, not as proof of overall fidelity.
7. Optimize the original *feeling and flow*: AI decisions, timing, strategic
   pressure, resource and mission progression, and audiovisual/gameplay
   behavior—not just individual numeric constants.

## Evidence levels for every substantive change

- **Proven original behavior:** show the exact EN/CZ original routine,
  disassembly, data record, or documented observed behavior. Note differences
  between editions when relevant.
- **Inferred behavior:** explain assumptions, limitations and competing
  interpretations; do not promote an inference into a fact.
- **Reloaded integrity / modernization:** clearly identify safeguards,
  migrations, compatibility code and modern engineering decisions as such.
- **Intentional deviation / approximation:** record why the simpler or
  different implementation yields comparable gameplay, the user-visible
  tradeoff and residual uncertainty.

## Implementation workflow

- Develop related mechanisms in coherent blocks on `reverse` with isolated,
  reviewable commits and documented original evidence.
- Preserve existing behavior unless explicitly corrected. Review integration
  points and backward compatibility, especially strategic/tactical state.
- Perform builds, static validation and automated regression checks where
  tools permit; do not claim a test ran when it did not.
- Avoid requiring manual gameplay tests after each small commit. At a useful
  milestone, consolidate changes into a testable candidate with focused
  scenarios, residual limitations and integration results.
- `main` remains separate until integration and acceptance are sufficiently
  verified. Do not treat the original or any guessed old Reloaded release
  number as a universal baseline without supporting evidence.
- Do not stop RE merely because a game can be completed; likewise, do not
  pursue tiny fidelity differences indefinitely when marginal value is low.

## Decision checklist

For any substantial proposed rewrite:
1. What did the original *actually* do, and how strongly is that supported?
2. What does Reloaded currently do, and what working behavior would change?
3. What is the player's observable gain in fidelity, flow or reliability?
4. What are the complexity, regression, save-compatibility and testing costs?
5. Is a small, robust, behaviorally faithful approximation preferable?

This document describes the project's direction, not a claim that all
existing code in either branch currently satisfies it.
