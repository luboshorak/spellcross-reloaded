# Legacy Spellcross Python research utilities

Historical independent scripts and GUIs for inspecting the original file formats and artwork:

- `unfs.py` — FS unpacking; `unlz.py` — `Bits`/`LZFile` decompression primitives; `unsave.py` — save data investigation.
- `spell_rawimg_tool_v4.py` and related GUI variants — indexed raw images, VGA 6-bit palettes and orientation transforms (Pillow).
- `spell_territory_mapper.py` — image-difference territory investigation (NumPy/OpenCV/Pillow).
- `rebuild_mainmenu.py` — interactive `ZoneEditor` that creates/loads hotspot JSON.
- `helper.py`, `mkimg.py`, `png_to_pal.py` and older versions — standalone experimental helpers; do not assume they are invoked by the compiled game.

The directory also contains historical binaries, `.idea` and `__pycache__` artefacts. Treat them as items for a later **evidence-based cleanup**, not as safely disposable by filename alone. See the [tool inventory](../docs/tools.md), [repository audit](../docs/repository-layout.md) and [developer docs index](../docs/README.md).
