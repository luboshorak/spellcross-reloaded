# Native PAK runtime experiment

**This is an isolated, long-lived experiment, not a release branch.**

Owner-approved policy (2026-11-27):

- Work only on `experiment/native-pak-runtime` or temporary topic branches targeting it.
- Merge `main` into the experiment when stable fixes are needed. Never merge, rebase or cherry-pick experimental work into `main`, never open a PR targeting main from this experiment, and never force-push/delete the experiment.
- Do not change main's source, workflows, release packaging, tags or rulesets as part of this experiment. No automatic release from this branch.
- The owner must configure GitHub deletion/force-push protection separately. A branch rule alone does not enforce source-to-target merge direction; do not claim this is enforced by GitHub until a main-side gate is explicitly authorized and configured.
- Preserve the original game data path and behavior until a separately selected native path is proven. Never implement modern mode by extracting original FS/FSU resources from PAKs or silently falling back to legacy data.
- The standalone Asset Converter 1.0.0 PAK v1 specification is the contract. Keep schema 1; runtime_ready=false is expected. Known missing original resources, unknown palette values, auxiliary FSA context and game timing are not magically solved by a successful decode. Original archives are archival only in the native path.

## Stage 1: native resource access (implemented, acceptance pending)

`spellcross-remaster-main/source/native_pak/` contains a bounded JSON reader, a C++ read-only dataset provider and an indexed atlas pixel decoder using existing wxWidgets components. The provider indexes the catalog, checks package ownership/version/record counts, rejects ambiguous names, opens ZIP64 entries by central-directory location on demand, validates lengths/CRC, and reads only modern payloads. Graphics are cropped from both indexed PNG planes, retain exact opacity and signed origin and use the package's palette table. The diagnostic PNG export applies the explicit opaque-zero display mapping; shadow compositing and unknown palette contexts remain engine responsibilities.

The legacy `SpellData` constructor currently loads and extracts COMMON.FS, builds sound/font/unit/terrain resources and resolves many legacy names. Do not construct a dummy FSarchive as an adapter. Introduce native consumers incrementally against typed data and keep legacy consumers isolated. There is deliberately no in-game PAK option yet; enabling an incomplete startup path would misleadingly imply a playable game.

### Build and test the independent C++ probe

With CMake, Ninja, C++20 and wxWidgets 3.2 core/base available (e.g. the same MSYS2 UCRT64 environment used for the converter), from the repository root:

```sh
cmake -S spellcross-remaster-main/tools/native-pak-probe -B build/native-pak -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native-pak
ctest --test-dir build/native-pak --output-on-failure
```

On Linux CI runs under Xvfb. The synthetic ZIP64 test contains no original archives. It exercises resource lookup, ambiguity, failed resources, mixed version rejection, random access to entries, indexed pixels, alpha, palette and signed origins. The fixture is not retail gameplay validation.

```powershell
.\build\native-pak\native_pak_probe.exe "C:\Games\Spellcross-PAK-1" inspect
.\build\native-pak\native_pak_probe.exe "C:\Games\Spellcross-PAK-1" resource JEDNOTKY.DEF
.\build\native-pak\native_pak_probe.exe "C:\Games\Spellcross-PAK-1" image MAINMENU.LZ mainmenu-native.png
```

Choose the complete export directory, not an individual .pak. Use an exact resource ID or full source name when aliases collide. Output image must not already exist. Unknown palette entries in visible pixels are errors rather than invented colors. These commands do not start Reloaded. The probe does not replace the converter's full dataset validation, and no commercial data is bundled.

## Next stages

1. Accept this provider using actual CZ and EN exported datasets; compare main-menu images against the original renderer and measure cold/random-read performance.
2. Introduce common data interfaces and a native menu-only test path (graphics, native fonts, texts, WAV/menu audio); no full SpellData construction, no legacy fallback. Explicit native selection and clear unsupported features.
3. Add native audio, video, strategic/tactical map and unit adapters in small tested increments, comparing with the legacy path at each boundary.
4. Only after complete native campaign runs, save/load, rendering and gameplay regression testing, review the experimental architecture with the owner. No merge to main is authorized by this milestone.

Sync stable fixes:

```sh
git fetch origin
git switch experiment/native-pak-runtime
git pull --ff-only origin experiment/native-pak-runtime
git merge origin/main
git push origin experiment/native-pak-runtime
```

## Native engine integration (in progress)

The native SpellData constructor now assembles typed core resources from a complete CZ/EN PAK dataset. It uses native palettes, indexed graphics, terrain and unit sprites, object/experience tables, sound classes, PCM samples, MIDI, fonts, language tables and narrative text. Native map construction and strategic level interpretation use the existing gameplay code. Project-authored target/gold filter supplements are preconverted into compiled-in indexed data; existing project-only FSA and auxiliary fonts remain explicit application resources, not retail archive fallback.

This constructor is not yet exposed by the normal startup wizard: UI consumers still need native resource routing, native MKV playback, INFO art, strategic save/load and mission dispatch wiring. The v1 atlas does not carry the original terrain editor land_type byte; native terrain currently sets it to zero while tactical flags come from the existing project context. Do not claim editor parity or full campaign acceptance. Windows engine-source CI compiles individual translation units but does not link or play a complete game. No changes to main are authorized.

## Experimental game startup

Use the first item, **Use a native Spellcross PAK dataset (experimental)**, in the startup data wizard or launch with `Spellcross.exe --native-pak "C:\path\to\dataset" --no-update-check`. Choose the directory containing manifest.json and resources.json. To switch an existing legacy config, use `--select-game-data`. The PAK mode records its own configuration keys and never selects a legacy dataset after a native load failure. The common menu, campaign level/mission dispatch, tactical map loading, strategic masks and adjacency, research and upgrades now have native routes. MKV video playback uses FFmpeg (ffmpeg.exe beside the game or on PATH), with temporary modern RGB/PCM data only. Project-owned helper files are still required. This is an experimental route; full campaign and save compatibility require runtime acceptance using actual converted CZ and EN datasets.
