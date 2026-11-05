# Original health indicators, company status and slope firing

## Scope and implementation

This revision builds on the hierarchy mouse-control restoration. It retains
the formation model, commander-host metadata, temporary-company identity and
tactical save layout. No replacement artwork is added.

Strategic company health is restored in Hierarchy, Units (including support
companies) and Buy. The same live company health and availability state feeds
all three screens. The mission-selection list deliberately has no UF icons or
health-slot decorations: these did not appear there in the original game.

## Original evidence

- Supplied Czech manual, printed pages 10-11: company management, four activity
  symbols, purchase delay, infantry/mortar and artillery rules.
- Supplied original-game video `2026-11-05_08h10_12.mp4`: hierarchy layout and
  health indicators. The existing QH/LZ coordinate catalog supplies screen
  origins and semantic regions; it does not claim bitmap placement metadata
  where the asset format contains none.
- Original CZ LE image SHA-256:
  `1b12fddfa80785c8f518e67981befa76e8eae11549485ab9519799e5c585bffd`.
  Offline LE relocation and x86 instruction inspection establishes the
  indicator formulas and icon draw calls below. These are static observations,
  not a claim of tracing the DOS executable at runtime.
- `COMMON.FS`: four UF images and `_SHARED1.PAL` in the test fixture were
  byte-for-byte identical to the supplied Czech installation.

## Health graphics and exact geometry

The short strategic health bar is **not a standalone bitmap**. The original
draws a filled rectangle with palette index **122**, RGB **0,212,0** from
`_SHARED1.PAL`. Its height is **3 px** and its width is
`floor(27 * current_strength / maximum_strength)`.

The original screen buffer for these panels is 406 pixels wide, composited at
native screen `(6,8)`. Coordinates below use the final 640x480 screen:

| Screen / first company cell | Bar origin | Size / pitch |
| --- | --- | --- |
| Hierarchy `(12,25)` | `(38,24)` | up to 27x3; rows 23 px, battalion pairs 104 px |
| Units, permanent `(16,11)` | `(38,10)` | up to 27x3; rows 19 px, columns 152 px |
| Units, support `(16,321)` | `(38,320)` | up to 27x3; rows 19 px, columns 152 px |
| Buy `(16,11)` | `(38,10)` | up to 27x3; rows 19 px, columns 152 px |

Static code anchors: hierarchy `0x34AFF..0x34B54`; permanent Units
`0x3A7A3..0x3A7F4`; support row origin `0x3A9DD`; Buy
`0x421B3..0x42204`; original rectangle primitive `0x51CC4`.

The Units selection overlay remains the real `VMU_SLCT.LZ` (146x17).
Its hand-drawn fallback frame is removed. Hit cells follow the corrected native
origins, including the one-pixel correction to the support block.

## All original strategic activity icons

These are static sparse-scanline ICO images, not Windows icon files or PNM
animations. Each is drawn at `company_cell + (1,1)` plus its native ICO offset.

| Asset | Meaning | Header width x height | Native offset x,y | Model condition |
| --- | --- | --- | --- | --- |
| `UF_ATACK.ICO` | Returning from battle | 10x10 | 1,2 | battle-return lockout |
| `UF_RECRU.ICO` | Recruitment / repair | 10x12 | 1,1 | pending recruitment or recorded recruitment lockout |
| `UF_UPGRD.ICO` | Upgrade / re-arm | 10x12 | 1,1 | pending engine/weapon/armour upgrade, re-arm or legacy upgrade |
| `UF_BUY.ICO` | Newly purchased company | 7x12 | 2,1 | purchase lockout |

Original loader: `0x120CD..0x12162`. Draw sites:
`0x34B57..0x34C16`, `0x3A830..0x3A8C8`, support roster near
`0x3AB3D..0x3ABFE`, and `0x42207..0x4229F`. The low company byte selects battle `0x10`, recruitment `0x04` and upgrade
`0x08`; the purchase draw check separately inspects byte +39. The last
applicable draw check wins: purchase, upgrade, recruitment, battle.

Only an active lockout displays an activity icon. Purchase, battle return,
recruitment, tech upgrades and re-arm set the real reason when they begin.
Completion and cancellation remove the symbol when the existing shared timer
reaches zero; this revision does not introduce a second timer.

The optional strategic JSON field `cooldown_reason` preserves purchase versus
battle return across strategic saves and embedded battle contexts. Missing
fields default to unknown. Old saves with pending-operation metadata still
recover recruitment/upgrade icons; a bare old cooldown cannot reliably identify
purchase versus battle and deliberately receives no guessed symbol.

The DOS save importer is preserved. Its reference parser distinguishes the
company flag byte (+38), action-timeout byte (+39) and signed upgrade-timeout
byte (+44); these are not merged into a new two-byte company-flags field.
Reference: [original Mod Launcher save parser](https://github.com/smaslan/Spellcross-Mod-Launcher/blob/master/source/SpellSaves.cpp).
Native integration tests cover remake JSON and tactical saves. Changes to the
DOS importer's record format are outside this revision.

## Tactical single-member health

The HUD was reading `wounded`, an infantry counter, for single-member units.
Their combat model stores fractional damage in `damage_remainder` (0..999).
The corrected 112x7 HUD bar uses `(1000-damage_remainder)/1000`; death gives
zero. Over-unit and unit-information bars use the same representation.

For multi-member infantry and vehicle companies, green strength remains
active members divided by maximum members. The wounded portion remains a
separate segment. Member-count labels retain their count semantics.
No combat damage, healing or binary save representation is replaced.
The original single-member accumulator formula is independently visible in
the unit-information display at `0x85015..0x85090`.

## Terrain firing rule

`JEDNOTKY.DEF` record +0x47 bit `0x10` (`FIRE_NOT_SLOPES`) is authoritative.
Sprite slope `'A'` is flat. Its elevation is independent: a flat plateau is a
legal firing position. Slopes `'B'..'M'` reject firing only for flagged types.

Verified fixture examples: AMX30, Light artillery, M109 and MLRS carry the
restriction; Mortar infantry and M106 do not. A blanket indirect-fire ban would
therefore be wrong, including for the mortar carrier.

The existing attack-range worker retains this rule and shares its predicate
with player unit/object checks and the actual AI/reaction-fire attack entry.
A stale cached attack map can no longer bypass the terrain restriction.

## Validation

- Windows C++ compilation and link.
- Native retail integration: all four sparse icons and green bar pixels in
  Hierarchy, Units and Buy; seven health levels, including zero/full; empty
  remainder of the bar; support roster placement; state expiry and recruitment.
- Strategic JSON and embedded campaign-context round trips for every reason;
  legacy unknown and legacy pending-operation cases.
- Actual HUD pixels for the single-member officer type 79 at damage 0, 1, 250,
  500 and 999; healing, death, tactical disk reload; multi-member regression.
- Flagged artillery and unflagged mortar types on flat/sloped tiles, elevation
  0/5, both sides; shared AI/reaction attack entry, stale player range cache and
  the asynchronous attack-range worker.
- Existing native hierarchy/formation/save and temporary-company identity
  regressions; campaign defence/recapture lifecycle regression.
- Existing Python suite and a dependency-free C++ health test wired into CI.

The integration executable takes four absolute paths: retail DATA, bundled
auxiliary data (containing units.json), extracted COMMON, and a writable fixture
directory. It is linked against the same application objects as the game.
Its rendered PNGs and generated saves are test outputs, not repository assets.

## Explicit limits

- Pixel verification covers the restored bars and symbols, not every pixel of
  the already-reconstructed strategic screens or their unrelated text/chrome.
- ICO images have no intrinsic animation. The manual's flashing countdown
  involves a separate palette cycle (`_SHARED2.PAL` / `_SHARED3.PAL`). Its exact
  original cadence has not been established here and is not replaced with an
  invented timer.
- No symbols are added to the mission-selection list. The optional desktop wx
  layout retains its existing list presentation; exact native placement applies
  to the restored original-game screens.
- This is automated renderer/entry-point verification, not an interactive
  end-to-end replay of the DOS game and remake or a complete campaign playthrough.
