# Coordinate systems: map, render surface, UI and zoom

This game has **multiple spaces**. A coordinate in one is not interchangeable with another. No general-purpose physical world-unit (metres) is defined by the tactical projection code examined.

## 1. Map/tile space

`MapXY { int x, y; }` in [`source/map_types.h`](../spellcross-map-edit-main/source/map_types.h) holds integer **tile indices**. A default instance is invalid (`-1,-1`); `IsSelected()` tests both components for non-negativity. Validity against the *specific map bounds* still requires `x < x_size`, `y < y_size`. `SpellMap::ConvXY(x,y)` in `source/map.cpp` indexes the row-major tile array:

```cpp
int SpellMap::ConvXY(int x, int y) {
    if (x >= 0 && y >= 0 && x < x_size && y < y_size)
        return x + y * x_size;
    return -1;
}
```

A `MapSprite` stores L1 terrain, L2 object and elevation/flags. Animation positions (`MapLayer3` and `MapLayer4`), unit positions (`MapUnit::coor`) and trigger positions (`SpellMapEventRec`) use tile indices. `MapXY::RelativePos` computes a staggered-row distance proxy by first doubling x and offsetting alternate rows, then multiplying the differences by `0.707106`; do not replace it with plain Euclidean distance on raw tile indices.

## 2. Staggered projected pixels

The battlefield is projected onto a 2D **pixel surface**, not rendered from a free 3D camera. Positive x moves right and positive tile y generally moves down; higher elevation moves the drawing **up**. Exact origins from `SpellMap::GetSurfPos` (`source/map.cpp`) are:

```cpp
// local to current visible render region; xs_ofs/ys_ofs come from the scroller
int tileX = pos.x - xs_ofs;
int tileY = pos.y - ys_ofs * 2;
int screenX = tileX * 80 + ((tileY & 1) ? 0 : 40);
int screenY = tileY * 24 - elevation * 18 + MSYOFS + 50;
```

`SpellMap::GetPosOrigin` instead computes the map-surface origin using *absolute* tile indices and adds the current L1 sprite's internal origin (`x_ofs + x_size/2`, `y_ofs + y_size/2`). It is used for unit centers and visual direction calculations. `Sprite::ProjectVertex` and `GetTileProjY` in `source/sprites.cpp` further describe internal tile geometry.

Example: neighboring tile rows do **not** share the same x origin: even y rows have a 40-pixel horizontal offset. Elevation `elev = 2` shifts the projected y value upward by **36 pixels** relative to equal tiles at elevation zero. Never infer grid adjacency from a single x/y pixel delta.

## 3. Scroll/viewport space

`TScroll` (`source/map.h` and `source/map.cpp`) stores a camera/selection reference and scroll offsets in render pixels. `SpellMap::RenderPrepare` sets the visible surface extent and calculates `xs_ofs`, `ys_ofs`, `surf_x_origin` and `surf_y_origin`. The surface buffer is allocated for the entire projected map at approximately `x_size * 80 + 40` width and a height derived from `y_size * 24` plus margins. `SpellMap::ScrollToTile` aligns a computed tile origin to the viewport center.

**Important:** `GetSurfPos` returns an offset relative to visible tile ranges; `GetPosOrigin` returns a location tied to absolute tile indices. Using one in the other's scroll calculation can displace sprites, markers or selection.

## 4. Device pixels, logical pixels and tactical zoom

`spellcross_display::Settings` in [`source/display_settings.h`](../spellcross-map-edit-main/source/display_settings.h) normalizes global app scale to **100% or 125%** (plus brightness/gamma/temperature). `spellcross_display::ToLogical(devicePixel)` removes that display scale. Tactical zoom is an **independent** 50–300% setting in [`source/tactical_zoom.h`](../spellcross-map-edit-main/source/tactical_zoom.h).

`MainFrame::TacticalMousePoint` (`source/main.cpp`) composes these conversions:

```cpp
// device pixel -> global logical pixel -> tactical source pixel
int tacticalX = spellcross_tactical::MapPixel(
    spellcross_display::ToLogical(mouseX), tacticalZoomPercent);
```

`MainFrame::TacticalMapSize` maps physical canvas dimensions to logical source extents. `spellcross_tactical::MapExtent` rounds upward (ceiling); `MapPixel` uses signed integer arithmetic with defined behavior for negative coordinates. `AnchoredScroll` is used by `MainFrame::OnCanvasMouseWheel` to retain the tile under the cursor during zoom.

`SpellMap::Render` produces a logical indexed frame, optionally resamples, applies color correction and writes a physical wx bitmap. Use the same conversion functions for hit testing and drawing; tests in `tests/test_tactical_zoom.cpp` specifically cover odd viewport sizes, negative inputs and all zoom levels.

## 5. Original strategic UI space

`StrategicOriginalRenderer::kScreenW/H = 640×480` in `source/strategic_original_renderer.h`. Its map subrectangle is `(18,18,379,259)`. Native buttons, scrollbars and lists use **640×480 authored pixel coordinates**, including original list chrome at `(412,5)` and scrollbar shaft `(556,33)` (`StrategicOriginalRenderer::BlitListChrome`).

`StrategicLevelFrame::OnOriginalStrategicPaint` fits and centers that framebuffer into the wx panel, recording `m_originalStrategicDrawRect`. `OnOriginalStrategicLeftDown` converts a device click back to native coordinates by subtracting the draw rectangle origin and multiplying by `640/drawRect.width` and `480/drawRect.height`. Both scales and letterboxing matter:

```cpp
int lx = (mouseX - drawRect.x) * 640LL / drawRect.width;
int ly = (mouseY - drawRect.y) * 480LL / drawRect.height;
```

Do not use tactical viewport offsets in strategic hit boxes. Cross-check new original UI positions with `source/spellcross_ui_coordinate_catalog/` (QH rectangles and SVG overlays), and distinguish measured UI rectangles from bitmap source offsets.

## Common errors

- Using raw `x + y * width` without checking `ConvXY`'s `-1` invalid return.
- Treating staggered `MapXY` coordinates as square Cartesian coordinates.
- Ignoring the 40-pixel alternate-row shift, sprite-specific origin or the `18 px/elevation` shift.
- Confusing physical window pixels, display-scale logical pixels, tactical-zoom pixels and native 640×480 strategic pixels.
- Scaling native assets twice, or scaling the image but not its click rectangle.
- Updating zoom without `AnchoredScroll`, which makes the map jump under the pointer.
- Assuming screen `TileIsVisible` tests are identical to fog-of-war visibility; gameplay uses `unit_view->view` in `SpellMap::Render`/AI.

For final behavior not independently verified against the DOS renderer, see [verification](verification.md).
