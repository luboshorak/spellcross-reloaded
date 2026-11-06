# Original tactical traversal and Magotar AI – EN executable evidence

Extracted directly from the user-provided English retail `SPELL/SPELCROS.EXE`,
SHA-256 `ff6b6554a2a8c532a2a2ed2c6d437ce63b7fc6b480efb29e9f7e14f441837623`.
This is an LE/DOS4GW image, with code object mapped to virtual address `0x10000`.
Addresses below refer to **mapped 32-bit addresses**, not ZIP or file offsets.
The game uses Watcom x86 and original `JEDNOTKY.DEF` records of 207 bytes.

## Confirmed navigation rule

- Original `0x746FF` constructs a per-unit movement mask. It selects movement class by
  `(unitFlags & 0x0E)`: ground, air, hover, etc. Multiple original enemy movement
  call sites invoke it, including `0x909FE`, `0x91D78` and `0x94A09`.
- At **`0x74852`**, a map-column loop compares current `x` against 2;
  at `0x74858-0x74863`, it compares against `mapWidth - 3`, **strictly less than**.
  If either condition fails, `0x74865` sets the corresponding 16-bit movement
  mask entry to `0xFFFF`. Thus ordinary original traversal permits only
  **`2 <= x < width - 3`** in this branch.
- The check is repeated at `0x74911`, `0x74A1E`, `0x74B20` and `0x74C22`
  for other movement classes. The routine also checks tile terrain/occupancy
  bits, e.g. `[tile+0x0F] & 0x80`, and categorical terrain values.
- The movement-mask routine iterates over the entire DTA height, but the
  original **map initialization** explicitly marks top and bottom margins
  with the per-tile flag `0x80`. The loop at `0x169a3` covers
  `y=0..7`; `0x169ef` ORs `0x80` into `tile+0x0f`. At
  `0x16a02` it computes `mapHeight-14`, and `0x16a70..0x16a7f`
  marks every cell from that row through the bottom. The movement-mask
  routine at `0x74880` rejects those flagged tiles (writes `0xffff`).
  Therefore **the actual vertical movement limits are**
  `8 <= y < mapHeight - 14`. This is a recovered DOS rule, not the
  draft's earlier guessed `5..height-15` inset. The PR now uses the
  original `8/14` margins.
  Further investigation should trace the movement-mask tile class and
  neighbor expansion, especially on northern/southern grey `PLA00_*` rows.
- The original loader marks these outer rows even when the L1 tiles have
  **non-PLA00 scenery**. I verified all 75 original English DTA map filenames:
  four of them (M06_06, M06_12, M06_13, M09_13) have under 80% PLA00
  filler on top/bottom. The PR's former grey-tile-ratio detection failed
  for those real maps. Original-map navigation now uses the canonical
  `MNN_NN` / `MNN_NNX` naming convention; custom DLC/editor maps
  (including `M00_00`) retain full-grid movement by default.
- Retail scenario scripts can place START and objective markers on otherwise
  restricted border columns. Those positions should not automatically turn
  the entire surrounding border into a walkable bypass. Keep rendering all
  DTA tiles independently of movement permission.

### Minimal original evidence (disassembled)

```asm
00074852  cmp  dword ptr [ebp-0x48], 2
00074856  jl   0x74865
00074858  mov  eax, dword ptr [0x64f9b4]  ; width read from DTA loader 0x83979
0007485d  sub  eax, 3
00074860  cmp  eax, dword ptr [ebp-0x48]
00074863  ja   0x7487d
00074865  mov  eax, dword ptr [ebp-0xc]
00074868  mov  word ptr [eax], 0xffff
```

## Confirmed Magotar decision structure

- Retail unit type **47** is ground Magotar, action 7 (take-off to 48).
  Type **48** is flying Magotar, action 8 (land into 47).
- The original enemy AI **explicitly** checks `action_id == 8` at
  **`0x918F7`** and **`0x92DBE`** (the value is read from
  `[0x26AFC1 + unitType*0xCF]`, the EN unit's action field).
- In both paths it invokes **`0x9C867`**, a group combat effectiveness
  comparison that forms separate air/ground unit lists, combines
  pairwise effects via `0x9B6C1`, and returns either **1 or 2**.
  In the flight special-action branch the result is compared to **2**
  before considering a landing.
- It then invokes **`0x9CDAE`**, which chooses a landing tile.
  A candidate must be unoccupied and have high terrain-class bits
  `tileFlags & 0xF800` equal to **0 or 0x9000**. If the current tile is
  suitable it is selected; otherwise it searches alternative reachable
  candidates and chooses one by distance/cost.
- At `0x919B9` and `0x92E61`, remaining AP is compared against the
  original `action_ap` from the unit record before scheduling the
  action. Otherwise the AI can issue a movement command toward the
  selected landing square first.
- **The guessed distance triggers (1.5/3.5/4.5) were removed from the PR**.
  The new helper implements the observed return-1 vs return-2
  group-form decision; the assembly's 0x7c9b8 no-RNG attack-vs-defence
  quadratic is ported for pair-effect estimation, and landing site search
  replaces a forced same-tile action. Dynamic bonuses and the exact
  original list-building/route-cost calculation remain **unverified**.
  Do **not** claim byte-exact Magotar AI parity until those are traced.

## Outstanding validation

1. The original `0x746FF` movement mask and the separate `0x169a3` /
   `0x16a02` map-edge flagging routines establish both X and Y limits.
   Verify exact parity against CZ original and per-tile classes near the
   scripted exceptions.
2. The `0x9c867` result comparison and zero-random `0x7c9b8`
   hit-projection curve are now ported; reproduce the remaining dynamic
   AP/experience/terrain/difficulty adjustments of `0x9b6c1` and original
   group-list choice, then verify candidate route costs from `0x9cdae`.
3. Test against the user-installed Czech ISO (including M04_03A convoy)
   before treating the restoration as finished.

This note distinguishes **direct executable evidence** from remaining
hypotheses, and records where to continue without re-guessing the constants.
