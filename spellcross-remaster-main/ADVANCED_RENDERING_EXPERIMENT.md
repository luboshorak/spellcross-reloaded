# Advanced rendering experiment (opt-in)

This branch is an isolated first implementation, not a finished HD remaster.
Settings > Screen > Advanced rendering (experimental). Default off; persisted
in the existing STATE section as advanced_rendering. Cancel restores the prior
setting. No changes to retail assets or game/save formats.

The native ground pass uses palette-cached directional relief, subtle world-space
microtexture, restrained colour grading, and soft contact shadows beneath
visible static companies, aircraft and objects. It runs only in the normal
tactical ground pass; fog, deployment, editor, range overlays, animated water,
selection, sprites and HUD retain their existing render paths. No hidden tile
elevations or hidden companies are used. Night grading remains the existing
night palette. Moving companies do not receive a tile-centred shadow, to avoid
a stationary ghost shadow during interpolation.

Limitations: 8-bit palette quantisation, baked sprite lighting and tile geometry
remain. Shadows are local contact approximations, not true cast shadows. This
is deliberately not an AI upscaler, 3D renderer, replacement texture pack or
full-colour postprocessing. Visual tuning requires original game data and
screenshots in daylight, night, urban, desert, forest and snowy maps.

## Manual acceptance checks
* Compare off/on at identical positions and zoom, then toggle off; original
  terrain and overlays must return.
* Exercise MOVE/ATTACK, temporary M/A, deployment, showmap, pan, zoom, HUD,
  fullscreen, night and fog boundaries. No new information may leak.
* Check shadows on units/objects, moving companies, bridges and map edges.
* Restart and verify persisted setting; Cancel must restore previous mode.
* Compare frame times on large maps, especially 50% zoom and high-DPI screens.

The standalone advanced-rendering test checks bounded decoding, transparency,
reserved palette indices, deterministic output, relief and contact falloff.

## Version 2: true-colour terrain details

The first indexed experiment was too subtle. The ground pass now retains the
retail indexed pixels and records viewport-local lighting metadata. Final RGB
conversion uses 64 light levels, independent of the original palette; coherent
24/7-pixel material patches and individual grains, richer material saturation,
stronger elevation relief, warm highlights/cool shadows and stronger contact
shade. Roads/water/bridges use restrained texture. Visible deployment ground
is also supported; hidden enemy companies do not cast shadows there.
Range modes and dialog dimming bypass the RGB enhancement entirely. HUD pixels
are excluded using the native HUD boundary. Later compositing is detected by
indexed colour comparison with a one-pixel guard; identical-colour overlap
cannot be identified perfectly without changing every sprite writer. This can
tint an occasional matching sprite pixel. Original sprite resolution remains.
No asset replacement or full-map buffer is required.

## Version 3: procedural HD terrain materials

An additional physical-framebuffer texture pass now generates 512x512 periodic
material atlases on demand, sampled at four texels per original world pixel.
No game asset files are replaced or generated on disk. A fixed world-space
phase follows the camera and combined zoom/DPI, with microdetail fading at low
zoom. Seven material profiles derive height and chroma from deterministic
multiscale periodic noise: wind-rippled sand, clustered grass, angular rock and
sparse pebbles, compact road, directional wood, smoother wet ground and soil.
Directional finite-difference lighting simulates a small-scale normal map.
Only visible tagged terrain receives this layer. Smooth sampling requires all
four samples to be the same material; HUD and range overlays remain excluded.
Night output remains green only. The existing approximate same-colour
compositing limitation remains; material flags are tile-level, not a perfect
per-pixel semantic mask. Texture atlas repetition is possible over long areas,
and the underlying authored silhouettes and sprites remain native resolution.
The atlas cache is bounded by seven materials (~3.5 MiB for two signed channels);
the viewport tags grow to four bytes each. Visual and performance tests on
real missions remain necessary.

Tests cover material packing, deterministic wraparound, distinct generated
materials, subpixel variation, camera/zoom coordinates and night/black safety.

## Version 4: independent controls and procedural sprite reconstruction

Screen settings now expose three independent, persisted switches. Existing
advanced_rendering keeps the V2 colour/lighting behavior; procedural_materials
and procedural_upscaling default off even with an existing V3 config. Any of
the eight combinations is legal. The V2 material classification used for
lighting is restored. The material pass can use the original palette alone.
Upscaling is not an AI model and does not synthesize arbitrary unit features.
It reconstructs supported diagonal edges and gently interpolates compatible
colours using the native, composited battlefield before zoom and HUD. It is
active only when combined zoom and display scale exceed 100%, with conservative
HUD, reserved-colour, dark-colour and range/dialog guards. It can affect
units, buildings and ground, and retains optional V2 lighting as a colour
delta. Native sprites, game coordinates and assets remain unchanged.
Limitations: the composited native image is not a semantic sprite mask; some
thin details may be softened, and no new authentic features can be recovered
from the low-resolution source. Native 100% is intentionally not upscaled.

## Version 5: real neural Hummer pilot

Independent AI graphics: Hummer setting, off by default. Only retail unit
type 29 enters the HD cache; the original renderer and its shadow pass stay
authoritative. When a visible, unfiltered Hummer is rendered, all linked
FSU resource frames are exported to a local ai_hummer_cache directory in
the game's working directory (override with SPELLCROSS_AI_CACHE). No
original graphics or model weights are distributed. Source and palette
hashes isolate different editions/terrain palettes. Restart after generating.
The final RGB compositor replaces only original opaque, still-visible Hummer
pixels; later sprite and status bar draws clear ownership and final index
comparison rejects changed pixels. The original silhouette, transparency,
shadows, animation, anchors and game logic are unchanged. Cache is missing
or invalid: original graphics. Range modes, hidden units, HUD and editor
remain unmodified. Later direct pixel writers using the identical index
can still be indistinguishable. Neural output may vary between frames.

1. Enable AI graphics: Hummer. Enter a battle with a visible Hummer,
   then exit. Find ai_hummer_cache (or set SPELLCROSS_AI_CACHE to an absolute
   path before launching the game).
2. Download official Real-ESRGAN ncnn Vulkan binaries and models:
   https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan/releases
3. Install Pillow (python -m pip install Pillow). From repository root:
   python spellcross-remaster-main/tools/build_hummer_hd.py --cache PATH_TO_CACHE --esrgan PATH_TO_EXE
4. Restart the game, enable AI graphics and compare at 200-300% zoom.

Default model is realesrgan-x4plus, no synthetic placeholder or interpolation
fallback is written by the tool. A Vulkan-compatible GPU is required for
the external tool only. The game loads generated RGB data with no inference
dependency. Use --force to regenerate with an alternative supported model.
Cache files are local data; do not commit them. No full game or actual neural
inference is verified by the standalone C++ test.
