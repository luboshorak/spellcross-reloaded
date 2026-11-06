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
- The routine iterates `y` over the entire DTA height; it does **not** prove
  a generic `y=5..height-15` limit. The draft PR's vertical rectangle is only
  derived from retail mission positions and must not be called an original rule.
  Further investigation should trace the movement-mask tile class and
  neighbor expansion, especially on northern/southern grey `PLA00_*` rows.
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
- **No evidence was found for the remake's distance triggers 1.5/4.5**
  or its 3.5-tile takeoff hysteresis. These are project heuristics
  currently in `tactical_navigation_rules.h`, not recovered DOS values.
  Do **not** claim full Magotar AI parity or merge PR #126 without
  implementing/testing the group strength + landing-site decisions.

## Outstanding validation

1. Trace the original vertical and terrain-bit restrictions all the way
   through the enemy/human path-search consumers of `0x746FF`.
2. Recover pairwise effectiveness helper `0x9B6C1` and exact meanings
   of result 1 vs 2 in `0x9C867`; then port the exact decision flow.
3. Test against the user-installed Czech ISO (including M04_03A convoy)
   before treating the restoration as finished.

This note distinguishes **direct executable evidence** from remaining
hypotheses, and records where to continue without re-guessing the constants.
