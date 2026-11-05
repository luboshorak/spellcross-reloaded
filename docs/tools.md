# Research and extraction utilities

These directories are separate research programs, **not modules imported by `Spellcross.exe`**. Their Python entry points and dependencies were inspected, but packaged binaries and generated research outputs were not executed during this review.

## `spell_extractfs/` — archive extraction and image inspection

| Script | Observed role / code anchors |
| --- | --- |
| `spell_extractfs_gui.py` | Tkinter front-end to `spell_extractfs.exe`; `App._start_worker` invokes subprocess extraction; `guess_exe_path` locates the companion tool. |
| `spell_fs_gui.py` | Tkinter GUI for both `spell_extractfs.exe` and `spell_makefs.exe`; `App._start_worker_extract`, `_start_worker_pack` and `_validate_pack`. |
| `spellcross_level_tool_v5.py` | `decode_clk`, `clk_outline_overlay`, `composite_fog_level` inspect and render strategic map/mask resources using Pillow and NumPy. |
| `bin_inspector_ui.py` and variants | Binary file detectors, entropy/hex previews, raw indexed-image analysis; `det_*` helpers. Uses Pillow and openpyxl for output. |
| `bin_out/spell_ui_builder.py` | `compose_png`, `export_svg` and `App` arrange manually selected decoded UI layers into images; the JSON project is research data, **not** a native game scene. |
| `data_sorter.py` | `sort_dir` groups extracted files by extension with an argparse CLI. |

The checked-in `bin_out/` and `level_out/` contain exports and build artifacts alongside scripts; do not mistake them for build inputs. Before moving or pruning, test tool default paths, GUI relative references and data provenance separately.

## `spell_decomp/` — compression tools

`bulk_delz.py` dispatches `spell_delz.exe` in batch. `spell_bulk_delz_gui.py` is a Tk front-end with `App._start_worker`; `spell_lz_tool_gui.py` provides compress/decompress jobs via `App._compute_jobs` / `_run_job` and uses `spell_delz.exe` and `spell_mklz.exe`. These are external tool processes, not the runtime's `LZWexpand` implementation (`spellcross-map-edit-main/source/LZ_spell.cpp`).

## `spellcross-master-pytools/` — earlier format investigations

`unfs.py` extracts FS records; `unlz.py` defines `Bits` and `LZFile` for original LZ streams; `unsave.py` examines saved data; `spell_rawimg_tool_v4.py` converts indexed raw images and VGA 6-bit palettes with Pillow. `spell_territory_mapper.py` analyzes differences between map images with NumPy/OpenCV/Pillow. `rebuild_mainmenu.py` defines interactive `ZoneEditor` to inspect/create menu hotspot JSON. Scripts such as `helper.py`, `mkimg.py` and the older GUI versions are separate utilities and need their own validation before being consolidated.

## Native in-repository audit and coordinate research

`spellcross-map-edit-main/tools/audit_original_ranges.py` parses original ranges. `spellcross-map-edit-main/source/spellcross_ui_coordinate_catalog/extract_spellcross_layout.py` inspects `.QH` and `.LST` sources; `qh_rectangles_*.csv`, `lst_links_all.csv`, SVG overlays and `spellcross_ui_coordinates_master.json` are reverse-engineering evidence for native UI hitboxes. The runtime code, not these generated tables alone, determines current behavior (`StrategicLevelFrame::OnOriginalStrategicLeftDown` and `StrategicOriginalRenderer`).

## Dependency caution

Tkinter is used throughout the GUI scripts. **Specific** scripts import Pillow, NumPy, OpenCV and openpyxl; there is no observed single repository-wide `requirements.txt` covering every script. Install dependencies for the chosen script rather than asserting every Python file needs all of them. Original third-party EXE tools may have their own platform or license constraints.

See [data-formats](data-formats.md), [repository-layout](repository-layout.md) and [verification](verification.md).
