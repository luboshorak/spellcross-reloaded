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

## Shader-style CPU filters (post-AI experiment)

The neural experiment was removed. Optional native-battlefield RGB filters
now include nearest, sharp bilinear, Catmull-Rom and Lanczos-2 (four taps per
axis). These are native CPU implementations, not imported GLSL or an OpenGL
backend. A conservative checker-pattern dedither and bounded adaptive
sharpening (0-100%) work independently. New options default to legacy/off/0
and are persisted through the shared screen dialog. Explicit scaling filters
replace the old procedural edge reconstruction; Legacy retains that option.
Lighting and material effects remain separately selectable. Processing is
applied before the HUD, guarded at the HUD fringe and bypassed for movement,
attack range, editor and dimmed dialog modes. Reserved colours and water
are guarded. The original sprite pixels, save format and hitboxes are intact.
There is no claim of perfect semantic separation, new realistic geometry,
GPU acceleration or full anti-aliasing at reduced zoom. CPU cost of the
four-tap filters needs real-mission profiling, especially at 4K.

### Additional filters and CPU optimization

Five alternatives retain the existing numeric configuration IDs: Bilinear,
Hermite (smoothstep interpolation), Mitchell-Netravali, cubic B-spline and
Scale2x/EPX quadrant reconstruction. Four-tap kernels use an exact separable
four-native-row cache. Regression tests compare it byte-for-byte against
direct convolution with mixed and reserved palette indices. Axis weights are
reused until viewport/zoom/filter changes. RGB and edge lookup tables reuse
unchanged palettes; the edge cache ignores reserved water cycling. Native RGB
scratch buffers are reused and dedither no longer copies the RGB surface.
This is not a GPU backend, and FPS improvements require in-game measurement.

### Native ground optimization and review

World noise is memoized in a bounded cache; at native/enlarged zoom the
five-point compositing mask is validated once per native pixel. Material-only
mode avoids lighting and RGB surface work. Flat sprite areas exit the edge
reconstruction early. Added six additional CPU kernels (IDs 10-15).
The temporary Filter review dialog exports checked removal candidates by
stable ID, never changing or deleting a filter itself.

### Debug performance overlay

Ctrl+Shift+F10 toggles a tactical canvas overlay. It counts successful map
renders per elapsed second (not monitor refresh rate), shows last, average
and maximum renderer milliseconds and map/RGB conversion phase averages.
CPU is process usage normalized by logical CPU count, RAM is process
working set/RSS on Windows/Linux. Samples update at roughly one-second
intervals while painting; no additional redraw timer or polling thread.
The overlay defaults off and is not saved. GPU counters are not measured.

### Filter review and parallel CPU pass

Removed IDs 3, 4, 5, 6, 7, 8, 12 and 15 following the review export;
retired saved IDs resolve to Legacy without renumbering retained entries.
Added Scale3x and edge-preserving bilinear. Native lighting now reads cached
noise in contiguous spans. A bounded reusable pool (up to 8 logical workers,
including the UI thread) processes independent RGB rows; the GUI thread
acquires bitmap row addresses and waits before unlocking the bitmap. The
four-tap scanline-cache path remains serial. No worker calls wx APIs or
simulation functions. In-game throughput and frame pacing are not yet
verified, and 25 FPS is a target, not a guarantee.

### Legacy fast path after reported FPS regression

All inactive experimental modes now bypass the common RGB effect pipeline.
The dedicated original palette converter uses cached coordinate tables and
two horizontal scanlines for exact 125% bilinear interpolation; rounding
is identical to the V4 formula. Nearest output uses direct palette lookup.
No worker pool is invoked on this path. Tests compare output to reference
on multiple frames and both display scales. The FPS overlay additionally
shows wall-clock frame interval and approximate time outside map rendering,
which includes event scheduling and drawing. The reported 18 FPS regression
has not yet been reproduced or confirmed fixed on the player's machine.

### Performance logging

The graphics settings checkbox and Ctrl+Shift+F10 toggle the same session-only profiler. RENDER_PERF records are appended to Windows debug.log roughly once per second while enabled and rendering a stable profile. Configuration changes reset the measurement window. GPU counters are not measured.

### Second filter review

Retired Scale3x (ID 16). Added directional triangle interpolation, Soft EPX and soft CRT scanlines (visible at effective zoom >=200%). Existing IDs are preserved and the review dialog includes the new modes. CPU implementations, not imported GLSL. The legacy fast path remains unchanged.

### V6 first hardware compute backend (Windows)

An opt-in Direct3D 11 feature-level 11.0 hardware compute path handles the
three original visual effects, palette dedither, adaptive sharpening and
Legacy/Nearest/Sharp Bilinear/Cosine/Smootherstep/Robidoux Sharp/Sharp Bicubic.
The CPU still rasterizes indexed map and HUD, produces lighting tags and
generates the material atlas once. GPU output is synchronously read back
into the existing wxBitmap (not a swap chain yet). Other filters, Linux,
unsupported hardware and device/shader failures use the CPU path without
silently disabling selected effects. Shader float interpolation may differ
by a few RGB levels from CPU fixed-point interpolation. Settings default
off. RENDER_PERF records gpu_requested, gpu_active and gpu_readback_ms
(includes waiting for GPU completion and copying the bitmap rows, not GPU
execution time alone). No claim of 60 FPS until measured on real hardware.

### V6.1 GPU filter coverage and settings cleanup

All retained filter IDs now have D3D11 implementations; Scale2x, Soft EPX, directional triangle, edge-preserving bilinear and CRT soft scanlines complete coverage. CPU remains fallback. WARP smoke tests check all exposed IDs and compare mixed-color samples with CPU reference. Settings use grouped two-column controls; the temporary filter rejection/export dialog and its locale strings are removed. Diagnostics remain available. Sprite reconstruction is disabled in the UI when another filter is selected, preserving its stored preference.

Contact shadow profiles are precomputed for the eight caster combinations, eliminating repeated ellipse divisions in the terrain loop. Exhaustive tests compare cached values to the original calculation. Grouped settings use native static-box ownership for correct platform layout and the existing game theme.

### V7 first exact ground-ownership pass

The existing indexed compositing order remains authoritative. While terrain
lighting or materials are active, later opaque sprite/animation, FSU unit
(including baked shadows), unit status-bar background and projectile glyph
writes invalidate ground tags at the actual destination, even when the palette
index is unchanged. Transparent source pixels leave ground tags intact. The
observer is scoped to the tactical scratch buffer, with no second draw pass.
The existing five-point comparison remains a conservative fallback for other
writers such as debug fonts. Both CPU and D3D11 consume the resulting tags.
This is the first ground/foreground separation step, not yet independent
unit/object colour surfaces or sprite-only reconstruction. Real-mission visual
and performance acceptance is still required.

### V7.1 foreground ownership and overlay coverage

The viewport now retains a one-byte ownership classification for ground,
static scenery, units, baked unit shadows, effects and selection/text overlays
while enhanced ground is enabled. Font glyphs and solid font backgrounds
also invalidate ground tags at the actual write. Hidden unit sprites no longer
cast the experimental contact shade. The CPU/GPU output still uses the shared
ground tags; the new ownership classification is groundwork for layer-specific
reconstruction and lighting, not a new independent colour compositor yet.

### V8 adjustable lighting and directional shadows

The existing lighting switch has a 0–100% intensity (100% retains the previous
V2 look). An independent daylight directional-shadow switch and strength slider
(0–100%, initial 60%) project bounded soft ground-plane shadows from visible
static ground companies and scenery using sprite extents for approximate reach.
They do not reconstruct silhouettes or raytrace elevation. Aircraft keep baked
shadows, moving companies are omitted, and night/range/dimmed/editor modes retain
the established exclusions. The receiving mask rejects fog, protected palette
indices and foreground writes. Overlapping projections use maximum opacity.
Shadow-only mode leaves unshadowed colours neutral. Both CPU and D3D11 consume
the same packed tags and grade setting. All new choices persist in STATE and
Cancel restores the complete prior settings. The screen dialog scrolls on small
monitors while OK/Cancel remain visible. CPU, GPU and settings regressions cover
the new math and defaults; actual game visuals and performance need acceptance.

Original unit shadows are separately configurable (default on). Disabling them
skips only FSU FD shadow operations in the tactical framebuffer, even when all
RGB effects are off; those pixels preserve the underlying palette and ownership
mask. It does not erase shadows painted into terrain/scenery artwork, the unit
HUD background or the separate advanced-lighting contact shade. If original
shadows are disabled, aircraft and moving companies currently lose their baked
shadows without a projected replacement. Other previews and the editor retain
original rendering. Settings and mask tests cover the independent toggle.

### V8.1 silhouette-based directional shadows

Replaces V8's generic ellipses with projected opaque scanline runs from the
actual visible sprites. L2 scenery (buildings, trees, obstacles) and FSU units
(infantry members, hulls, turrets, animation frames and aircraft) participate
through the existing foreground raster observer. Each caster uses its ground
anchor, and height shifts the silhouette down/right. Transparent gaps and FSU
shadow-marker pixels do not cast; overlapping contributions use maximum opacity
with a one-pixel soft fringe. Shadows are applied to the final tagged ground,
not foreground or fog. No extra rendering pass or gameplay-state changes.
This remains an approximate planar projection of 2D assets, not a 3D terrain
raytrace; actual art, terrain slopes and performance require in-game checks.

### V9 terrain and water

Optional multi-scale V2 terrain sampling blends a second world-space atlas
sample with the existing micro-height material. Material intensity (0–100%)
is independent of lighting. Optional animated water uses only original water
palette-cycle indices 240–249 on water/ford tiles, protected by the same final
ownership and HUD masks. Two travelling ripple bands modulate the existing
water palette without replacing art, revealing fog or moving gameplay pixels.
CPU and D3D11 share integer math and phase. All new switches default off,
existing materials retain their default intensity and V1 sampling. New
settings persist and Cancel restores the original values. No new snow class
is assumed: the original flags do not expose one. Water is animated shading,
not geometric displacement, reflection or projectile-driven simulation.
