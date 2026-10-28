# Strategic campaign regressions — GitHub issues #51–#54

These checks must be completed in a native Windows game build with the original
Czech game data **before** the pull request is merged. Code review and inspection
of the original English `COMMON.FS` alone do not prove the gameplay is fixed.

## #51 — attacks, defences and recaptures

The original `COMMON.FS` has `M05_02.DTA` and `M05_03.DTA`, but no
`R0502.DTA` or `R0502.DEF`. The original saved campaign contains
`R0502.MIS` and `R0503.MIS`: these are generated missions, not map assets.

- Start a new campaign at LEVEL_02. End turns until a scripted `Army(...)`
  event triggers. The first `Start(1)` command territory must not be attacked.
- Progress to LEVEL_05, with original `Start(1), Start(2), Start(3)`.
  First `Start(1)` is protected; eligible other sectors can be attacked.
- Accept an attack on territory 2 (the generated `R0502` family). It should
  use `M05_02.DTA` and the scripted `Army(...)` force, not fail searching
  for `R0502.DEF/DTA`.
- Decline the defence, verify the sector becomes enemy-held, and recapture
  it on a subsequent strategic turn. The same occupying force must be used.
- Repeat with an actual defeated defence and with a strategic save/load
  between loss and recapture.
- Confirm the crossed-swords `LASTTERT` sprite appears only on the
  terminal `End(n)` sector, not on ordinary attacks or recaptures.
- Confirm attacks select a player-held frontier sector (excluding the initial
  command territory), rather than blindly choosing the last owned sector.
- Regression: `Rxxxx` tactical save/load must reconnect to the correct
  `Mxx_xx.DTA` battlefield and campaign context.
- Also exercise a custom level lacking a CLK border map: it must use the
  documented eligible-sector fallback, not crash.

Caveat: the exact DOS weighted target-selection algorithm has not yet been
recovered; the current implementation uses eligible player-held frontier
territories. Do not claim byte-for-byte equivalence to original RNG without
a DOS trace. Legacy save families that point to territories absent from the
loaded game data cannot be reconstructed from missing source assets.

## #52 — tactical casualty snapshot

- Save a tactical game *before* a company dies, lose that company, then load
  the in-game tactical save. Win the mission without losses.
- Both tactical result statistics and campaign statistics must count **zero**
  losses for the discarded timeline.
- Save *after* a company dies and reload: the loss must remain counted.
- Repeat with casualties that have already been physically removed from the
  tactical map (the accumulated loss ledger) and with commander casualties.
- Check that `.scsave` resume and the four-use tactical save limit still work.

## #53 — sequential movies

- Finish a mission with an ending movie, then advance to a new chapter that
  has an intro. See both videos in order with **picture and sound**, and only
  then see the strategic map.
- In particular check end of LEVEL_02 and the LEVEL_03 opening, and the
  talking-head outro paths.
- Press Escape during a movie. The next queued movie should still play;
  closing the final movie must show the correct strategic chapter.
- Test using a released package that passes the 37-MP4 SHA-256 manifest
  verification. A local build missing the video fallbacks cannot verify
  Czech CAN/SMK picture playback.

## #54 — campaign-wide slots

- Save on LEVEL_02 in slot 1, advance to LEVEL_03, and verify slot 1 remains
  visible on the Options page.
- Reload slot 1 from LEVEL_03 and verify LEVEL_02 is opened with the saved
  territory ownership, units, money, research, commanders and statistics.
- Save LEVEL_03 in slot 1; it should overwrite the globally shared slot, and
  other global slots should remain unchanged.
- Confirm that autosaves continue to use `level_XX/autosave.json` for
  inter-level player-state transfer.
- Regression: start with only legacy
  `save/strategic/level_XX/slot_YY.json` files. The newest legacy file of
  each slot number is copied to global `slot_YY.json`; the originals are
  NOT deleted or modified. All colliding older copies remain manually
  accessible via the file picker.
- Verify exactly **nine** native strategy Options slots and no per-chapter
  player-slot namespace.

## Release gate

No claim of a verified gameplay fix until MSVC/wxWidgets compile succeeds,
a Czech ISO playthrough passes the checks above, and a fresh portable build
with all video fallback files is tested. Keep this PR as draft until then.
