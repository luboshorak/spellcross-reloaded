# Issue #188 — tactical reaction fire and initiative

Reported in [#188](https://github.com/luboshorak/spellcross-reloaded/issues/188):
which unit performs a counter-attack, whether a calculation decides it, why
artillery appears not to react, and why Commando appears to fire first.

## Short answer

There is a calculation, but the tactical mechanic is more accurately described
as **defensive or opportunity fire**, not as an automatic counter-attack after
being attacked. `SpellMap::CheckReactionFire` is evaluated after each movement
step for both factions. An ordinary attack by itself does not trigger it.

Defensive fire is triggered when the mover **newly enters the combined
visibility of the defending side** (Alliance or Other Side). Individual
defenders then need a legal weapon shot, action points, and a successful
initiative contest. A spotter can provide contact for artillery even when the
gun itself cannot see the mover. Commando has high base initiative and often
wins the multi-shot priority, without any hard-coded special preference.

This distinction was corrected in Reloaded after disassembling the original
English `SPELCROS.EXE`: movement checks side-level visibility flags at
`0x1D690–0x1D6C9` / `0x28890–0x288B8`, while its reaction candidate
routine `0x9BE32` calls weapon-range test `0x62562`, not the separate
personal-sight test `0x625BE`.

## Trigger and eligibility

The current implementation in `source/map.cpp` follows this sequence:

1. A unit completes one movement tile. Teleport arrival is treated as entry
   from a previously unseen position.
2. The engine considers every live unit on the opposing side.
3. A prospective defender must be active, idle, able to attack the mover's
   unit class, and have enough action points for at least one shot.
4. Compare the previous and current tiles against **the defending side's
   combined visibility**. A previously unseen tile becoming visible is a
   contact; movement between already visible tiles does not repeatedly trigger
   another volley. A friendly scout or radar can supply that visibility.
5. The prospective shooter must have the target in its legal **weapon attack
   area**. Direct-fire weapons require their normal line of fire; indirect
   artillery can use another unit's observation while still respecting its
   maximum/minimum range and any terrain/slope restrictions.
6. The defender must win the initiative contest below. A tie is a failure.

The check is symmetric: Alliance units may interrupt Other Side movement and
Other Side units may interrupt Alliance movement. Team visibility is evaluated
for the corresponding defending faction, not from the player's fog-of-war
presentation buffer for both sides. Once a volley starts, the
mover is paused, the queued shots are animated and paid from the defenders'
normal action points, and the interrupted movement resumes if the mover
survives.

## Initiative calculation

For each candidate the engine computes:

```text
initiative score = JEDNOTKY.DEF byte 0x45 + (experience level - 1)
roll             = initiative score + random integer from 0 through 5
```

In the normal initiative branch, the defender fires only when its roll is
**strictly greater** than the mover's roll. The original EN executable also
contains a conditional path that skips these rolls, whose prerequisite flags
are not fully decoded; Reloaded currently uses the standard contest. Each
side receives its own independent random value. Experience levels
are stored as 1 through 12, so their initiative contribution is 0 through 11.

If `D` is the defender's initiative score minus the mover's score, the chance
to pass this contest is:

| D | -5 or less | -4 | -3 | -2 | -1 | 0 | +1 | +2 | +3 | +4 | +5 | +6 or more |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Chance | 0% | 2.8% | 8.3% | 16.7% | 27.8% | 41.7% | 58.3% | 72.2% | 83.3% | 91.7% | 97.2% | 100% |

The 41.7% value at equal scores is intentional: ties favor the moving unit.
This also explains apparently deterministic cases. A defender ahead by six or
more initiative points cannot lose the current 0..5 versus 0..5 contest.

## Why Commando so often fires first

The supplied Czech and English retail `JEDNOTKY.DEF` records agree on these
base values:

| Unit | Base initiative (`+0x45`) | Direct sight | Weapon range | Base shots before bonuses | Relevant fire flags |
| --- | ---: | ---: | ---: | ---: | --- |
| Commando | 8 | 5 | 6 | 3 | direct fire |
| Mortar infantry | 5 | 4 | 8 | 2 | indirect |
| Light artillery | 4 | 4 | 10 | 2 | indirect, cannot fire from slopes |
| MLRS | 4 | 4 | 14 | 2 | indirect/MLRS, cannot fire from slopes |

Commando therefore starts four initiative points above Light artillery and
MLRS at equal experience, which gives it a 91.7% initiative-win chance against
a mover four points below it. A two-level experience advantage raises that
difference to six and makes the contest certain under the current formula.

Initiative decides **whether a candidate qualifies**, not which successful
candidate fires first. If several defenders pass, Reloaded orders them by:

```text
priority = resolved attack against this target * remaining shot count
```

The highest priority is consumed first. Before experience or upgrade bonuses,
for example, a full-AP Commando has three shots and can outweigh a two-shot
artillery battery even when the artillery's single-shot attack value is
slightly higher. Equal priority is currently resolved by unit ID (the higher
ID is consumed first); that is an implementation detail, not a documented
retail rule.

## Artillery and friendly spotters

**Artillery does not need to personally spot the mover.** A scout, another
friendly unit, or appropriate radar visibility can establish *new* contact
for the whole defending side. Artillery may react if:

- the enemy has just entered the defending side's **combined current sight**;
- its own weapon can legally attack that tile (for indirect ground fire,
  beyond the two-tile minimum and within maximum range);
- its terrain/slope firing rules permit a shot;
- it has enough AP and wins the applicable initiative check.

The original contact check is shared across the faction; it is not performed
independently for each gun. If the enemy was already visible to *any* ally
at the previous position, simply approaching the artillery will not trigger
another volley. Artillery's relatively low initiative still reduces its
chances, but the previous Reloaded-only requirement for **personal direct
sight** was a real restriction and has now been removed.

Its sight value of 4 in the unit data is still relevant for *acting as a
spotter*, not as a hard cap on its reaction-fire range.

## Evidence boundary and remaining verification

The original EN executable's movement sites inspect side-level contact flags;
the reaction collector validates the gun's fire range rather than personal
sight. Reloaded now reconstructs combined visibility by checking living
observers of the defending side (including both factions separately), then
checks the individual shooter's weapon area and initiative. This is consistent
with the recovered original, though a per-tile DOSBox parity trace is still
outstanding. The exact multi-candidate priority remains a Reloaded close
equivalent (`attack * shots`) rather than a byte-for-byte reconstruction of
the DOS selection routine. See the [deep EN executable analysis](Spellcross_reaction_fire_original_vs_reloaded_2026-11-22.md).

This page is a static review of current `main` plus the supplied CZ/EN unit
records. It does not claim a new DOSBox trace or interactive parity test.
Useful follow-up scenarios are:

1. Fix both random seeds and move identical units across a defender's sight
   boundary in retail and Reloaded.
2. Repeat with equal initiative, a +4 Commando advantage and a +6 advantage.
3. Test Light artillery and MLRS with a **separate forward scout** revealing
   a mover beyond the gun's own sight, at distances 2, 3, 4, 5 and 10;
   repeat on slopes and when the target was already visible on the prior tile.
4. Provide two or more successful defenders with different attack/shot totals
   and recover the retail selection order rather than assuming Reloaded's
   priority formula is exact.
5. Repeat in both directions (Alliance moving and Other Side moving), with the
   mover surviving and being destroyed by the volley.

Primary implementation references are `SpellMap::GetReactionInitiative`,
`SpellMap::CheckReactionFire`, `SpellMap::ProcessReactionFire`,
`SpellMap::StartAttack_NoHUD`, `SpellMap::ViewRange::HasDirectSight` and the
`JEDNOTKY.DEF` loader in `source/spell_units.cpp`.
