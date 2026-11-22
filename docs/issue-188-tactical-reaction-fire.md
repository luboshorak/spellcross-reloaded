# Issue #188 — tactical reaction fire and initiative

Reported in [#188](https://github.com/luboshorak/spellcross-reloaded/issues/188):
which unit performs a counter-attack, whether a calculation decides it, why
artillery appears not to react, and why Commando appears to fire first.

## Short answer

There is a calculation, but the tactical mechanic is more accurately described
as **defensive or opportunity fire**, not as an automatic counter-attack after
being attacked. `SpellMap::CheckReactionFire` is evaluated after each movement
step for both factions. An ordinary attack by itself does not trigger it.

A defender can fire only when the moving enemy has just crossed from outside
to inside that defender's **direct sight**, is also in the defender's legal
weapon area, and the defender wins an initiative roll. Consequently artillery
is not hard-disabled, but its long indirect range does not help it qualify: it
still needs its own direct sight of the moving unit. Commando has high base
initiative and commonly has a strong multi-shot priority, so its observed
behavior follows the current rules rather than a special Commando exception.

## Trigger and eligibility

The current implementation in `source/map.cpp` follows this sequence:

1. A unit completes one movement tile. Teleport arrival is treated as entry
   from a previously unseen position.
2. The engine considers every live unit on the opposing side.
3. A prospective defender must be active, idle, able to attack the mover's
   unit class, and have enough action points for at least one shot.
4. The mover must be in the defender's direct sight at the new tile and must
   not have been in that same defender's direct sight at the previous tile.
   Remaining visible while moving therefore does not repeatedly retrigger
   defensive fire.
5. The ordinary attack-range calculation must accept the target tile. This
   preserves weapon range, line of fire, terrain/slope restrictions and the
   minimum range of indirect weapons.
6. The defender must win the initiative contest below. A tie is a failure.

The check is symmetric: Alliance units may interrupt Other Side movement and
Other Side units may interrupt Alliance movement. Once a volley starts, the
mover is paused, the queued shots are animated and paid from the defenders'
normal action points, and the interrupted movement resumes if the mover
survives.

## Initiative calculation

For each candidate the engine computes:

```text
initiative score = JEDNOTKY.DEF byte 0x45 + (experience level - 1)
roll             = initiative score + random integer from 0 through 5
```

The defender fires only when its roll is **strictly greater** than the mover's
roll. Each side receives its own independent random value. Experience levels
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

## Why artillery appears never to react

Indirect fire does not borrow sight from another unit for this trigger. Light
artillery and MLRS have direct sight 4 but weapon ranges 10 and 14. They can
therefore perform defensive fire only when all of the following coincide:

- the enemy has just entered the artillery unit's own direct sight;
- the enemy is at least three map-distance units away, because indirect fire
  cannot target ground units at distance 2 or less;
- the normal attack map permits the shot, including the no-slope restriction;
- action points for a shot remain;
- the artillery wins its initiative roll (base 4 plus experience rank).

For a ground target this leaves only a narrow contact band near the edge of
the artillery's direct sight, before initiative is even rolled. An enemy that
was already visible on the preceding tile does not create another chance.
The observed rarity is therefore expected. The code does **not** contain a
blanket `artillery cannot react` condition.

## Evidence boundary and remaining verification

The initiative field and formula are documented in the current source as
recovered from `SPELCROS.EXE`; the current trigger also models the recovered
movement-time contact behavior. The exact multi-candidate priority is expressly
a Reloaded close equivalent (`attack * shots`), not a demonstrated byte-for-byte
reconstruction of the DOS selection routine.

This page is a static review of current `main` plus the supplied CZ/EN unit
records. It does not claim a new DOSBox trace or interactive parity test.
Useful follow-up scenarios are:

1. Fix both random seeds and move identical units across a defender's sight
   boundary in retail and Reloaded.
2. Repeat with equal initiative, a +4 Commando advantage and a +6 advantage.
3. Place Light artillery and MLRS at distances 2, 3, 4 and 5, including a
   slope, and verify exactly where a candidate is created.
4. Provide two or more successful defenders with different attack/shot totals
   and recover the retail selection order rather than assuming Reloaded's
   priority formula is exact.
5. Repeat in both directions (Alliance moving and Other Side moving), with the
   mover surviving and being destroyed by the volley.

Primary implementation references are `SpellMap::GetReactionInitiative`,
`SpellMap::CheckReactionFire`, `SpellMap::ProcessReactionFire`,
`SpellMap::StartAttack_NoHUD`, `SpellMap::ViewRange::HasDirectSight` and the
`JEDNOTKY.DEF` loader in `source/spell_units.cpp`.
