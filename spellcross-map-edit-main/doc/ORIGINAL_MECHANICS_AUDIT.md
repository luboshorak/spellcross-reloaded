# Original Spellcross — sight, movement and firing-distance audit

**Source of truth:** user-supplied retail English `Spellcros_EN_Verze_Puvodni.zip`, specifically `SPELL/DATA/COMMON.FS`, its `JEDNOTKY.DEF` (87 records, 207 bytes each), `BONUSES.DEF`, `FORMACIE.DEF`, `UPGRADES.DEF`. Corroboration: original Czech printed manual supplied as `Spellcross_navod_text_a_obrazky.pdf`; it explicitly says hills improve *both* sight and firing distance by one per level of height over the surrounding terrain. The retail `SPELCROS.EXE` is a DOS/4GW **compiled executable, not released original C/C++ source code**. Where its exact algorithms are not proven, this audit explicitly does not assert bit-for-bit equivalence.

The independent unpacker under `tools/audit_original_ranges.py` reads the retail COMMON.FS and enumerates **every original CZ/EN unit**, including enemy/special forms. No original game assets are committed.

## Original data layout

| Property | EN JEDNOTKY.DEF record offset | CZ offset | Runtime consumer |
| --- | --- | --- | --- |
| Unit ID | zero-based array index | same | `SpellUnitRec::type_id` |
| Base action points | 0x3B | 0x3B | `MapUnit::GetMaxAP()` |
| Action points per ordinary movement step | 0x3D | 0x3D | `MapUnit::GetWalkAP()` |
| Base weapon range | 0x46 | 0x46 | `MapUnit::GetFireRange()` |
| Base sight | 0xB0 | 0xAF | `ViewRange::Worker()` |
| Radar active sight range | 0xAF (special action param 3) | 0xAE | `ViewRange::Worker()` |

**All 87 original EN movement costs verified:** 83 units cost 7 AP per normal step, Orcs (#37), Ka-orcs (#38) and Undead (#42) cost 6, bunker Destructor (#73) costs 0 (stationary). These values are properties of individual types; never substitute a global constant. Elevation, slope, road, forest, distance/diagonal movement and obstacles still apply separately.

Key retail reference values:
- Light infantry (#0): 56 AP, 7 AP/step, sight 4, range 5.
- Rangers (#2): 70 AP, 7 AP/step, sight 6, range 6.
- Hummer (#29): 98 AP, 7 AP/step, sight 9, range 5.
- Radar 1R (#30): 74 AP, 7 AP/step, normal sight 5, deployed radar sight **11**, weapon range 0.
- Radar 2R (#31): 74 AP, 7 AP/step, normal sight 5, deployed radar sight **14**, weapon range 0.
- Destructor bunker (#73): base step cost **0**; stationary while in this form.

## Experience bonuses (BONUSES.DEF, exact)

| Experience level | Movement steps added | Weapon/sight range added |
| --- | --- | --- |
| 1–5 | 0 | 0 |
| 6–8 | +1 | 0 |
| 9–12 | +2 | 0 |

`AttackPT` increases the potential number of shots (levels 5–9 +1, 10–12 +2). This is *not* a range bonus. Other experience modifiers affect attack and defense; none modifies sight or weapon range.

**AP formula (normal movement):** `available_AP = base_AP + (XP_Move + sum(installed_Upgrade_Move)) × base_step_AP`. Normal step **cost** is always the raw `base_step_AP`; the old `GetMaxAP() / (apw + bonus - 1)` altered it incorrectly.

Worked examples on unobstructed level ground, counting only ordinary full-cost steps before any firing (actual pathfinding applies separate terrain/distance rules):
- Light infantry: level 1 = 56 AP / 7 = 8 steps; level 6 = 63 / 7 = 9; level 9 = 70 / 7 = 10.
- Hummer: level 1 = 98 / 7 = 14; level 6 = 105 / 7 = 15; level 9 = 112 / 7 = 16.
- TAM tank (#10) at level 1: 72 / 7 = 10; T2001 engine upgrade (+3 Move) => 93 / 7 = 13 (on level terrain with normal-cost steps).
- Orcs (#37): 66 / 6 = 11 steps at level 1.
- Destructor bunker (#73): movement disabled, not free.

## Hierarchy and strategic upgrades

`FORMACIE.DEF` has *no movement, sight, or range bonuses*:
- Battalion: attack +1, defense +1, Move(0).
- Regiment: attack +2, defense +1, Move(0).
- Brigade: attack +4, defense +3, Move(0).

`UPGRADES.DEF` contains movement upgrades with positive **and negative** deltas (e.g. +3 engines, -1 strengthened armor). Exactly three original upgrades change weapon range: Tyrian #15 **-1**, Fargun #17 **+2**, Terminator #19 **+1**. None contains a direct sight increase. The strategic layer loads the original UPGRADES.DEF and passes these modifiers into tactical `MapUnit` records; completed category upgrades supersede earlier upgrades of the same category.

## Terrain and combat geometry

- **Sight:** raw `sdir` / activated radar value; downhill elevation advantage `max(0, shooter_elevation - target_elevation)`; normal sight additionally requires the engine's occlusion/ray mask. Active radar ignores intervening terrain rays, but still benefits from the height advantage.
- **Weapon range:** raw `fire_range` + signed installed `Range` upgrades (clamped to zero). Actual maximum shooting distance against a particular tile gains the same documented downhill elevation bonus. Raw range in the unit info panel deliberately remains the **base** range, as stated in the manual.
- **Indirect fire:** retains the existing minimum-range rule and ignores normal direct line-of-sight rays. Terrain/slope restrictions for different weapon flags remain active.
- **AI:** planner, move-to-fire stop decision and mobile defense are updated to consider the same downhill bonus as the fire overlay.
- **Movement:** roads, slope penalties, forests, obstacles, the air/hover/foot movement flags and non-axis-aligned path cost remain processed by the existing pathfinder. They have not been claimed to be bit-for-bit identical with DOS.

## Code changes in this branch

1. `spell_units.cpp::MapUnit::GetWalkAP`: use original per-unit AP-per-step, not an artificial divisor. Zero denotes immobile.
2. `map.cpp::MoveRange::Worker`, `FindPathInternal`: reject zero-step-cost stationary forms, so no negative/free path-cost exploits.
3. `map.cpp::ViewRange::Worker` and `HasDirectSight`: apply the manual's downhill sight bonus consistently, including radar.
4. `map.cpp::GetEffectiveFireRange` and enemy AI: apply original documented downhill fire-range advantage consistently.
5. No changes to `BONUSES.DEF` XP values, hierarchy attack/defense rules, strategic upgrade compatibility, or the underlying 3D ray/terrain/movement-cost algorithms.

## Required Windows / DOS comparison before merge

- On flat unblocked ground: compare max move reach for light infantry, Hummer, Rangers, Orcs, radar; repeat at XP 1/6/9/12.
- Compare TAM with no upgrades, +1 and +3 engine, and strengthened armor -1 Move (on suitable units). Verify range upgrades Tyrian -1, Fargun +2, Terminator +1.
- Verify identical line-of-sight on same-height tiles and each 1/2/3-step elevation advantage; confirm downhill shots, blocked LOS and indirect fire minimum range.
- Deploy Radar 1R and 2R (5 -> 11/14 sight), repeat on raised terrain, with objects blocking line of sight. Check no direct radar weapon attacks.
- Verify mobile and stationary Destructor forms, teleport/special-move units and air units do not gain illegal movement.
- Repeat actual firing after partial movement and vice versa, including XP `AttackPT`, since the current separate `GetAPperFire` formula remains **unverified** against the original DOS executable. It is intentionally **not rewritten without proof**.
- Original exact ray sampling, terrain path-cost multipliers, defensive fire timing and `GetAPperFire` for higher levels remain *requires retail executable testing or reverse engineering*, not 100% verified by the available primary `.DEF` files.

**Verification state:** retail data decoded and independently checked; source paths reviewed. A Windows MSVC build and side-by-side original gameplay have **not** been run by the assistant. Do not merge into main until runtime checks pass.
