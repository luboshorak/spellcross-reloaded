# Strategic UI, ISO import and tactical tick regression — 2026-10-17

Scope: branch `fix/strategic-loader-mechanics-tick`. This checklist documents
source-level fixes and required **Windows / MSVC in-game verification**. It is
not a claim that the complete game has already passed runtime testing.

## Strategic original UI: native scrollbars

The original `SB_BG01..04.LZ` backgrounds include metal chrome/arrows;
`SB_BAR01..03.LZ` are full-length textured tracks, **not** draggable thumbs.
Keep them. Render a separate, proportional thumb on the native track using the
list's total items and visible rows. Use a common hit/drag handler for:

- [ ] Map unit roster (native track at x=556, y=33; window height 429)
- [ ] Hierarchy unit pool (height 472)
- [ ] Units upgrade/rearm list (height 287)
- [ ] Buy/Sell list (height 287)
- [ ] Research list (height 472); **must not** snap to the selected research
  item after a thumb drag/arrow/wheel scroll
- [ ] Info list (height 472)
- [ ] Arrows, mouse wheel, page clicks above/below thumb still work
- [ ] No scrolling content: thumb fills track, no out-of-bounds or invalid drag
- [ ] Drag under 1x / 2x / fractional original-screen scaling
- [ ] Clicking toolbar elsewhere during hover/release never leaves stuck capture

The obsolete strategic top-menu **Game mode** item is removed; game-mode
state is still managed internally by the campaign.

## Startup data-source wizard

When core Spellcross archives are not configured, wizard choices include
`Download CZ ISO` and `Download EN ISO` after existing four choices.

- [ ] CZ URL: `https://spellcross.czech-games.net/wp-content/uploads/ISO/Spellcross_CS.iso`
- [ ] EN URL: `https://spellcross.czech-games.net/wp-content/uploads/ISO/Spellcross_EN.iso`
- [ ] HTTPS download to `game_data/downloads/Spellcross_??.iso.part`,
  progress and Cancel, ISO9660 PVD `CD001` validation, atomic-ish
  `MoveFileExW` replacement only after validation
- [ ] If server declares Content-Length, reject an incomplete transfer
- [ ] Cached valid ISO can be reused, without network access
- [ ] Download and local ISO both use existing ISO9660 `INSTALL.DTA` importer
  and normal sidecar files (`INFO.FS`, `MOVIE.FS`, `SPEAKER.FS`)
- [ ] Offline/cancel/HTTP error/invalid ISO does not delete a working cached ISO
- [ ] Manual ISO, physical CD directory, DATA folder and per-file modes unchanged

## Original unit data audit and entrenchment

Reference: original **English retail** `SPELL/DATA/COMMON.FS/JEDNOTKY.DEF`,
87 records of 207 bytes. English has a one-byte extension after DEF offset
0x48, so **direct sight is at raw record 0xB0 and dig interval at raw 0xB2**.
Do not accidentally read offsets from the 206-byte Czech structure.

| ID | Unit | UTYPE | WALK | Base sight | Base fire range | Turns per dig level | Max dig after fix |
|----|------|-------|------|------------|-----------------|---------------------|-------------------|
| 0 | Light infantry | 0x52 | yes | 4 | 5 | 2 | 6 |
| 29 | Hummer | 0x10 | no | 9 | 5 | 2 | 2 |
| 30 | Radar 1R | 0x10 | no | 5 | 0 | 2 | 2 |
| 31 | Radar 2R | 0x10 | no | 5 | 0 | 2 | 2 |
| 32 | MI24 | 0x04 | no | 4 | 4 | 0 | 0 |

Previous implementation used `isLight()?6:2`, wrongly giving all light
vehicles max 6. The new predicate uses `isWalk()?6:2` after rejecting air
and units with zero dig interval. From the original 87 records, expected
dig caps are 38 at 6, 40 at 2, 9 at 0 based on these rules.

- [ ] Entrench light infantry through level 6
- [ ] Entrench Hummer, Radar 1R and Radar 2R only through level 2
- [ ] Radar special up/down action does not grant extra dig levels
- [ ] Air units and units with no dig interval cannot entrench
- [ ] Taking incoming fire decreases an earned level according to existing
  tactical rules; moving clears entrenchment as before
- [ ] Unit XP does not incorrectly modify the dig *cap*

Other fields in original `JEDNOTKY.DEF`: `ap`, `apw`, `fire_range`,
`sdir`. `BONUSES.DEF` contains XP level 1..12 Attack, AttackPT,
Defence, Move; `FORMACIE.DEF` contains Attack and Defence per formation
and Move(0). Per-unit upgrades are applied separately in runtime.
**No general range/movement algorithm changes were made**: the user
requested full DOS parity before modifying them, and exact executable
parity has not yet been demonstrated. The current `GetWalkAP` divisor
and visibility/elevation transitions warrant targeted comparison.

## Tactical tick / dead-unit cleanup

- [ ] Kill enemy unit, change selected unit while projectile/hit/death
  animation is running: former attacker finishes HIT/DIE/UPDATE and corpse
  is removed without waiting for another turn
- [ ] Selected friendly is busy with another action: deferred death finishes
  after it becomes idle, without interrupting its animation
- [ ] Kill units that have no hit/death animation: removal is immediate
- [ ] Kill a unit using a scripted special attack/kamikaze: no dangling
  target pointers, crash or infinite spin
- [ ] Killed unit cannot fire again, occupy a cell, appear in FOW, or be
  targeted after cleanup
- [ ] When cleanup changes unit list, `Tick()` returns repaint=true and
  rebuilds mask/fog; preview ranges stay associated with actual selection
- [ ] Enemy-turn/reaction-fire sequences and victory/failure still complete

Underlying issue: tactical `Tick()` advances only one active/selected unit.
If that selection changes during a lethal attack, `IsUnitProtectedFromCleanup`
correctly retains a dead target while an unselected attacker is mid-death
sequence; without advancing that owner, the corpse lingers until later turns.
The fix resumes a pending lethal owner's animation on idle ticks and forces
a repaint after generic corpse cleanup.

## Strategic saves

Two independent formats are supported:
- Original `BIG_MAP.SAV`: Spellcross LZW, save chapter at offset 0x4834,
  checks minimum expanded size and plausible chapter, territory and asset-set
  checks prior to import.
- Remake strategic JSON: older optional fields/default migrations, different
  structure and per-level checks.

The original distribution includes old `BIG_MAP.SAV` examples. Structural
decompression checks are not a full runtime import/roundtrip test.
A user-reported failed old save **cannot be conclusively diagnosed** until
that exact save/error is available. Do not silently force-load a wrong-chapter
or wrong-language save as if it were valid.

- [ ] Import original English BIG_MAP.SAV from chapter 2, 3, 5, 10
- [ ] Reject mismatched level and Czech-only missions with actionable errors
- [ ] Load older remake JSON, inspect resources/rosters/hierarchy/research
- [ ] Save -> reopen -> compare all visible strategic values and mission flags
- [ ] Invalid/truncated originals produce a handled error, not a crash

## Branch and merge gate

Keep this work isolated from `main` until MSVC Rebuild and the above
end-to-end tests are completed. No changes to Stanislav Maslan's upstream
editor repository.
