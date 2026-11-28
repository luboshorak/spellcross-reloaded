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
