# Spellcross archive and UI research tools

This directory is a **standalone toolbox**, not a dependency automatically built into `Spellcross.exe`.

- `spell_extractfs_gui.py` and `spell_fs_gui.py` — Tkinter wrappers around the adjacent `spell_extractfs.exe`/`spell_makefs.exe` executables; extract and repack FS archives.
- `spellcross_level_tool_v5.py` — `decode_clk`, `composite_fog_level` and Pillow/NumPy strategic map inspection.
- `bin_inspector_ui.py` and variants — raw binary detectors/image inspectors; selected scripts use Pillow and openpyxl.
- `bin_out/spell_ui_builder.py` — manually compose extracted layers and export PNG/SVG from a research JSON layout.
- `data_sorter.py` — CLI directory organization after extraction.
- `bin_out/`, `level_out/`, `build/` and `*.spec` — generated/reference outputs and packaging metadata intermixed with source.

For all entry points, dependencies and caveats see [tools guide](../docs/tools.md). The game runtime uses its **own** `source/fs_archive.cpp` and `source/LZ_spell.cpp` under `../spellcross-map-edit-main/`. Do not remove packaged executables or reorganize adjacent-data assumptions until their tools are individually tested.
