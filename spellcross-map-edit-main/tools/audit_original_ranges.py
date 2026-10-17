#!/usr/bin/env python3
"""Read *original* Spellcross COMMON.FS and audit unit sight, movement and fire.

No proprietary game assets are bundled; point this at an installed original
COMMON.FS, or supply the extracted .DEF files through a supported archive.
The original retail JEDNOTKY.DEF is a binary array, not a textual .DEF file.

    python tools/audit_original_ranges.py game_data/DATA/COMMON.FS
    python tools/audit_original_ranges.py game_data/DATA/COMMON.FS --level 9 --upgrade-move 3 --csv audit.csv
"""
import argparse
import collections
import csv
import pathlib
import re
import struct


def read_common(path):
    data = path.read_bytes()
    if len(data) < 4:
        raise ValueError("COMMON.FS header missing")
    count, = struct.unpack_from("<I", data)
    if not (0 < count < 100000) or 4 + count * 21 > len(data):
        raise ValueError("Not a valid COMMON.FS directory")
    records = {}
    for index in range(count):
        off = 4 + index * 21
        name = data[off:off + 13].split(b"\0")[0].decode("ascii", "replace")
        start, length = struct.unpack_from("<II", data, off + 13)
        if start + length > len(data):
            raise ValueError(f"Out of bounds: {name}")
        records[name.upper()] = data[start:start + length]
    return records


def blocks(source, heading):
    text = source.decode("cp852", "replace")
    pattern = re.compile(rf"{heading}\s*\(\s*(\d+)\s*\)\s*\{{([^}}]*)\}}", re.I)
    values = {}
    for match in pattern.finditer(text):
        fields = {}
        for m in re.finditer(r"([A-Za-z]+)\s*\(\s*([-]?\d+)\s*\)", match.group(2)):
            fields[m.group(1).lower()] = int(m.group(2))
        values[int(match.group(1))] = fields
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("common_fs", type=pathlib.Path)
    parser.add_argument("--level", type=int, default=1, choices=range(1, 13))
    parser.add_argument("--upgrade-move", type=int, default=0,
                        help="Combined Move(...) bonuses of installed upgrades")
    parser.add_argument("--upgrade-range", type=int, default=0,
                        help="Combined Range(...) bonuses of installed upgrades")
    parser.add_argument("--csv", type=pathlib.Path, help="Save every original unit as CSV")
    args = parser.parse_args()

    records = read_common(args.common_fs)
    unit_data = records["JEDNOTKY.DEF"]
    if len(unit_data) % 207 == 0 and len(unit_data) % 206 != 0:
        stride, sight_offset, version = 207, 0xB0, "EN"
    elif len(unit_data) % 206 == 0 and len(unit_data) % 207 != 0:
        stride, sight_offset, version = 206, 0xAF, "CZ"
    else:
        raise ValueError("Unrecognized Czech/English JEDNOTKY.DEF record size")

    bonuses = blocks(records["BONUSES.DEF"], "UnitLevel")
    formations = blocks(records["FORMACIE.DEF"], "Formation")
    upgrades = blocks(records["UPGRADES.DEF"], "Upgrade")
    for level in range(1, 13):
        if level not in bonuses:
            raise ValueError(f"Missing original XP level {level}")
    xp_move = bonuses[args.level].get("move", 0)

    results = []
    for index in range(len(unit_data) // stride):
        unit = unit_data[index * stride:(index + 1) * stride]
        name = unit[:28].split(b"\0")[0].decode("cp852", "replace")
        basic_ap = unit[0x3B]
        per_step = unit[0x3D]
        max_ap = basic_ap + (xp_move + args.upgrade_move) * per_step
        results.append({
            "id": index,
            "name": name,
            "base_action_points": basic_ap,
            "action_points_at_level": max_ap,
            "normal_step_ap_cost": per_step,
            "steps_on_flat_ground": max_ap // per_step if per_step else 0,
            "basic_sight": unit[sight_offset],
            "basic_weapon_range": unit[0x46],
            "range_with_upgrade": max(0, unit[0x46] + args.upgrade_range),
            "immobile": per_step == 0,
        })

    print(f"Source: {args.common_fs} ({version}), {len(results)} units")
    print(f"XP {args.level}: Move {xp_move:+d}, optional upgrade Move {args.upgrade_move:+d}")
    print("Formation Move bonuses:",
          {k: v.get("move", 0) for k, v in sorted(formations.items())})
    print("Original range-modifying upgrade IDs:",
          {k: v["range"] for k, v in sorted(upgrades.items()) if v.get("range", 0)})
    print("Normal step AP costs:", dict(collections.Counter(
        u["normal_step_ap_cost"] for u in results)))
    print("All values are raw original assets or directly derived. Terrain, LOS,")
    print("indirect-fire minimum range and radar deployment are NOT simulated.")

    cols = tuple(results[0])
    if args.csv:
        with args.csv.open("w", newline="", encoding="utf-8-sig") as f:
            out = csv.DictWriter(f, fieldnames=cols)
            out.writeheader()
            out.writerows(results)
        print("CSV:", args.csv)
    else:
        for u in results:
            print(f'{u["id"]:02d} {u["name"][:25]:25} AP {u["action_points_at_level"]:3} '
                  f'step {u["normal_step_ap_cost"]:2} flat steps {u["steps_on_flat_ground"]:2} '
                  f'sight {u["basic_sight"]:2} range {u["range_with_upgrade"]:2}')


if __name__ == "__main__":
    main()
