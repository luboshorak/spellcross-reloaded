# Experimental original EN projectile geometry

This branch is for in-game A/B comparison, not a claim of full DOS parity. It defaults to recovered English retail geometry and does not load editor `units.fsa` in that mode. The FSA file and legacy path remain intact. Set the process environment variable `SPELLCROSS_PROJECTILE_MODEL=fsa` before launch to run the legacy comparison; unset it and restart to return to the experiment. No save format, campaign data, combat damage formulas, AP rules, or build-script changes are intended.

## Reproducible source evidence

User-supplied English retail `Spellcros_EN_Verze_Puvodni.zip`, `SPELL/SPELCROS.EXE`, LE DOS/4GW, 1,632,112 bytes, SHA-256:

`ff6b6554a2a8c532a2a2ed2c6d437ce63b7fc6b480efb29e9f7e14f441837623`

Code addresses below use LE object 1, base 0x10000; data operands are the disassembler's pre-relocation offsets. The executable itself is not distributed.

| Evidence | Address/offset | Observed behavior |
|---|---|---|
| Map slope | 0x839d3–0x839e7, 0x83aa1 | Third terrain-name character minus 0x40; A=1 to M=13. |
| Ground aim vector | 0x76faa–0x77126 | Staggered tile X spacing 80/40, row spacing 24, elevation correction 18, slope height/2. |
| Direction sectors | 0x7712b–0x774ef | Projected Y-up multiplied by 1.66666666; single-precision ratio and thresholds ±0.1989 and ±0.6681. Directions 0 north, 4 east, 8 south, 12 west. |
| Terrain slope heights | data object 3: 0xcef3 + index*4 | For A–M: 48,66,66,66,48,49,30,30,30,31,48,49,66. |
| Airborne adjustment | 0x774ef–0x7750f, 0x775ba, 0x77645 | 120 pixel Y correction, after ordinary aiming sector selection. |
| Unit drawing anchor | 0x5522e–0x5523f; movement 0x1d280–0x1d29b | Airborne sprite anchor subtracts 120*screen pitch. |
| Flight origin table | EN file 0x1894b8, data object 3: 0x728b8 | 13 rows × 16 signed int32 LE X/Y pairs, 1664 bytes. |
| Air/ground table access | 0x77904/18, 0x77936/52, 0x779a1/b5, 0x779d3/ef | Air uses row A; ground uses terrain slope. Add offsets to computed projected start. |
| Equivalent object-attack branch | 0x7a91d/31, 0x7a94f/6b | Same coordinate table. |
| MLRS | 0x77a80–0x77aa0; 0x7aa4d–0x7aa6d | Fire flag 0x20 rotates direction +4 modulo 16 AFTER origin calculation. |
| Turn direction | 0x77b5b–0x77ba8 | Shortest circular turn; exact half-turn follows sign of target-current. |
| Projectile glyph | 0x78225–0x78273, 0x7b080–0x7b0c5 | Uses direction after the MLRS adjustment. |
| Travel slope and length | 0x778b9–0x77a0f | Dominant axis and slope from adjusted center delta BEFORE the launch offset; remaining distance from adjusted launch to target. |
| Flight step | 0x78382–0x784ec | 10 pixels per dominant-axis step; test (step+1)*10 against remaining distance. |
| Render | 0x23eb1–0x23f4a | Position used for projectile rendering. |

The header `original_projectile_geometry.h` contains all 208 coordinate pairs. FNV-1a of the original 1664 bytes is 0xbe15a065; both the header's table and the extracted bytes independently yield this value. The local C++23 test used files whose Git blob hashes matched the branch files and passed with both default and legacy environment values.

## Integration choices and remaining differences

The original FSU directory and compressed frame payloads are not changed. The found table is indexed by slope and direction, not by unit type. Maslan's FSA describes editor-authored per-graphic-group origins and centers; it is not the same coordinate system or data source. We do not synthesize per-unit DOS FSA records or infer original turret pivots. The actual current game aiming path used the regular FSU azimuth functions rather than the FSA-based angular helpers; image assembly remains unchanged.

The experimental attack path uses the DOS ordinary ground aiming vector, then selects the recovered offset with the unrotated attack direction. Turret/body and glyph receive the rotated MLRS direction. The engine's existing azimuth mapping handles resources not containing exactly 16 directions. Air sprite/target/muzzle anchors use 120 rather than 150; target height comes from the recovered slope table. The legacy path still uses 150 and the original FSA-based logic.

The DOS routines also use precomputed tile pixel pointers in a special state branch and clip the start to viewport boundaries before adding the offset. We have not ported these state/viewport details. The experiment instead uses the present engine's rendered tile centers (including signed terrain sprite Y offsets). This can differ when source and target tile sprite offsets differ, or when either is offscreen. The original applies x87 rounding to absolute coordinates; the experiment uses float ratios and truncates relative movement, so a one-pixel difference is possible. Invalid mod slope letters fall back to A, coincident targets keep a fallback direction, and a flight with no positive remaining distance lasts at least one Reloaded tick. These are explicit compatibility choices.

Visible projectiles use the recovered 10-pixel step and stop condition adapted to the existing state machine. Non-projectile attacks keep existing delay rules. DOS timer cadence, all firing-animation frame counts, special state behavior, actual turret pivot geometry, full PNM/hit timing, and Czech executable parity are not established. No claim is made that the entire visual attack sequence is byte-exact. No damage/AP/reaction-fire decision rules are replaced. Flight timing changes may affect presentation and event scheduling, so runtime tests are required.

## Suggested acceptance test

Use the same save/mission and compare two separate process launches. Start with flat terrain, then opposite facings and sloped tiles; check Abrams body and turret, infantry, artillery/MLRS, aircraft against ground/air, shots at objects, defensive fire, and nearby targets. Pan/zoom during flight and check fog/visibility and that attacks always complete and AP/damage are applied once. Check muzzle origin, direction, projectile glyph, hit placement, duration and aircraft height. Run default mode with `units.fsa` absent (optional) to confirm independence. Restart in legacy mode for a baseline; startup log reports selected model. Retain the FSA file for legacy comparison. Run both retail languages, but regard CZ as an unverified use of the EN geometry until the original CZ executable has been separately disassembled. Do not merge this branch before gameplay and build validation.
