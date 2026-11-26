"""Run a real C++ PAK compatibility import against generated ZIP fixtures."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import zipfile

ARCHIVES = ["COMMON.FS", "T11.FS", "PUST.FS", "DEVAST.FS",
            "UNITS.FSU", "TEXTS.FS", "INFO.FS"]

def fixture(root, *, partial=False, missing=False, duplicate=False,
            invalid_range=False, corrupt=False, bad_path=False,
            nested=False, wrong_count=False):
    root.mkdir()
    records = []
    expected = {}
    pak = root / "common.pak"
    with zipfile.ZipFile(pak, "w", compression=zipfile.ZIP_STORED) as z:
        for i, name in enumerate(ARCHIVES):
            if missing and name == "INFO.FS":
                continue
            data = ("original " + name).encode()
            expected[name] = data
            location = f"original/containers/{i}.bin"
            logical = ("DATA/INSTALL.DTA::DATA/" if nested and name != "INFO.FS" else "DATA/") + name
            z.writestr(location, data)
            records.append({
                "id": logical + "#0", "name": logical, "status": "container",
                "format": "FSU" if name.endswith(".FSU") else "FS",
                "original": {"entry": location, "offset": 0, "size": len(data)}
            })
        # A modern payload and an unrelated nested FS member must not be
        # materialized as a game archive.
        z.writestr("records/100/payload.bin", b"modern")
        records.append({
            "id": "member#0", "name": "DATA/COMMON.FS::MUSIC.FS",
            "status": "converted", "format": "FS",
            "original": {"entry": "original/containers/0.bin",
                         "offset": 0, "size": 1},
            "payload": "records/100/payload.bin"
        })
        if invalid_range:
            records[-1]["original"]["size"] = 9999
        if duplicate:
            record = dict(records[0], id="duplicate#0")
            records.append(record)
        if bad_path:
            z.writestr("../escaped.bin", b"must not be extracted")
        z.writestr("manifest.json", json.dumps({
            "schema": "spellcross-pak", "version": 1, "category": "common",
            "partial": partial, "records": records, "palettes": [], "graphics": []
        }))
    if corrupt:
        raw = pak.read_bytes()
        original = expected["COMMON.FS"]
        at = raw.index(original)
        pak.write_bytes(raw[:at] + bytes([raw[at] ^ 1]) + raw[at+1:])
    container_count = sum(r["status"] == "container" for r in records)
    (root / "manifest.json").write_text(json.dumps({
        "schema": "spellcross-dataset", "version": 1,
        "partial": partial, "runtime_ready": False,
        "packages": [{"file": "common.pak", "category": "common",
                      "records": len(records) + int(wrong_count),
                      "containers": container_count, "converted": 1,
                      "decoded": 0, "raw": 0, "failed": 0}]
    }), encoding="utf-8")
    return expected

def run(executable):
    with tempfile.TemporaryDirectory() as temp:
        base = Path(temp)
        for label, kwargs, ok in [
            ("installed", {}, True),
            ("iso", {"nested": True}, True),
            ("partial", {"partial": True}, False),
            ("missing", {"missing": True}, False),
            ("ambiguous", {"duplicate": True}, False),
            ("range", {"invalid_range": True}, False),
            ("crc", {"corrupt": True}, False),
            ("traversal", {"bad_path": True}, False),
            ("count", {"wrong_count": True}, False),
        ]:
            root = base / label
            expected = fixture(root, **kwargs)
            dest = base / (label + "-output")
            dest.mkdir()
            result = subprocess.run([executable, str(root), str(dest)],
                                    capture_output=True, text=True)
            assert result.returncode == (0 if ok else 1), (label, result.stderr)
            if ok:
                assert {p.name: p.read_bytes() for p in dest.iterdir()} == expected
            else:
                assert not list(dest.iterdir()), (label, "validation wrote output")
            assert not (base / "escaped.bin").exists()
            print(label, "passed")
        # Never overwrite an existing import directory.
        dest = base / "existing"
        dest.mkdir()
        (dest / "COMMON.FS").write_bytes(b"keep")
        result = subprocess.run([executable, str(base / "installed"), str(dest)])
        assert result.returncode == 1
        assert (dest / "COMMON.FS").read_bytes() == b"keep"

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=lambda p: str(Path(p).resolve()))
    run(parser.parse_args().executable)
