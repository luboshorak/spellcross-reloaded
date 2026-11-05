# Spellcross LZ utilities

`bulk_delz.py` runs the adjacent `spell_delz.exe` in batch. `spell_bulk_delz_gui.py` wraps the process in Tkinter. `spell_lz_tool_gui.py` provides compression and decompression jobs with `spell_delz.exe` and `spell_mklz.exe`, managed by `App._compute_jobs` and `_run_job`.

These are research tools, **not** the library used inside the running game: runtime LZ decoding lives in `../spellcross-map-edit-main/source/LZ_spell.cpp` and `fs_archive.cpp`. See [tools.md](../docs/tools.md) and [data-formats.md](../docs/data-formats.md). Keep adjacent EXE location expectations in mind before moving files.
