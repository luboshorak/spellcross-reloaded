#!/usr/bin/env python3
"""Build a local Hummer/Commando HD cache with the official Real-ESRGAN Vulkan CLI.
Requires Pillow and a separately installed Real-ESRGAN executable and models.
Never modifies source assets; no interpolation-only fallback is written.
"""
import argparse
import collections
import pathlib
import re
import struct
import subprocess
import tempfile
from PIL import Image

SOURCE_MAGIC = b"SCHDSRC1"
RGB_MAGIC = b"SCHDRGB1"
SCALE = 4
PAD = 16

def read_source(path):
    data = path.read_bytes()
    if len(data) < 784 or data[:8] != SOURCE_MAGIC:
        raise ValueError(f"Invalid source: {path}")
    w, h = struct.unpack_from("<II", data, 8)
    if not (1 <= w <= 256 and 1 <= h <= 256) or len(data) != 784 + w*h:
        raise ValueError(f"Invalid source dimensions: {path}")
    return w, h, data[16:784], data[784:]

def prepared_image(w, h, palette, indices):
    # Fill transparent/shadow regions by nearest opaque colour, with a
    # padded border, so the neural model does not learn a black halo.
    W, H = w+2*PAD, h+2*PAD
    pixels = [None]*(W*H)
    q = collections.deque()
    for y in range(h):
        for x in range(w):
            i = indices[y*w+x]
            if i not in (0, 253):
                p = (y+PAD)*W+x+PAD
                pixels[p] = tuple(palette[i*3:i*3+3])
                q.append(p)
    if not q:
        raise ValueError("Sprite contains no opaque artwork")
    while q:
        p = q.popleft()
        x, y = p%W, p//W
        for nx, ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)):
            if 0 <= nx < W and 0 <= ny < H:
                n = ny*W+nx
                if pixels[n] is None:
                    pixels[n] = pixels[p]
                    q.append(n)
    image = Image.new("RGB",(W,H))
    image.putdata(pixels)
    return image

def valid_cache(path,w,h):
    try:
        with path.open("rb") as f:
            return f.read(16)==RGB_MAGIC+struct.pack("<II",w*SCALE,h*SCALE) and path.stat().st_size==16+w*h*SCALE*SCALE*3
    except OSError:
        return False

def store_result(source, output, w, h):
    target = source.with_suffix(".scrgb")
    with Image.open(output) as result:
        if result.size != ((w+2*PAD)*SCALE, (h+2*PAD)*SCALE):
            raise ValueError(f"Unexpected neural output size for {source.name}: {result.size}")
        image = result.convert("RGB").crop((PAD*SCALE, PAD*SCALE,
                                          (PAD+w)*SCALE, (PAD+h)*SCALE))
        # The engine uses the original exact opaque pixel mask.
        payload = RGB_MAGIC + struct.pack("<II", w*SCALE, h*SCALE) + image.tobytes()
    temporary = target.with_suffix(".scrgb.tmp")
    temporary.write_bytes(payload)
    temporary.replace(target)

def process_batch(batch, executable, models, model, jobs, gpu=None):
    """One model initialization per batch, not per animation frame.

    Import valid completed PNGs even if the native process fails or is
    interrupted. Atomic cache writes preserve any previously finished frame.
    """
    with tempfile.TemporaryDirectory(prefix="spellcross-hd-") as work:
        work = pathlib.Path(work)
        inputs, outputs = work/"input", work/"output"
        inputs.mkdir()
        outputs.mkdir()
        for source, w, h, palette, indices in batch:
            prepared_image(w, h, palette, indices).save(inputs/(source.stem+".png"))
        command = [str(executable), "-i", str(inputs), "-o", str(outputs),
                   "-m", str(models), "-n", model, "-s", "4", "-f", "png",
                   "-j", jobs]
        if gpu is not None:
            command.extend(["-g", str(gpu)])
        print(f"Running Real-ESRGAN for {len(batch)} frames in one process...", flush=True)
        error = None
        try:
            subprocess.run(command, check=True, cwd=executable.parent)
        except (subprocess.CalledProcessError, KeyboardInterrupt) as exc:
            error = exc

        generated, missing = 0, []
        for source, w, h, _, _ in batch:
            output = outputs/(source.stem+".png")
            try:
                if not output.is_file():
                    raise FileNotFoundError(output)
                store_result(source, output, w, h)
                generated += 1
            except (OSError, ValueError) as exc:
                missing.append(source.name)
                print(f"Not imported: {source.name}: {exc}", flush=True)
        print(f"Imported {generated}/{len(batch)} frames into HD cache.", flush=True)
        if error is not None:
            raise error
        if missing:
            raise RuntimeError(f"Real-ESRGAN did not produce {len(missing)} valid frames. "
                               "Completed frames were saved; rerun to resume.")
        return generated

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache",type=pathlib.Path,default=pathlib.Path("ai_hummer_cache"))
    parser.add_argument("--esrgan",type=pathlib.Path,required=True,help="Real-ESRGAN ncnn Vulkan executable")
    parser.add_argument("--models",type=pathlib.Path,help="Model directory; default executable/models")
    parser.add_argument("--model",choices=("realesrgan-x4plus","realesrnet-x4plus","realesrgan-x4plus-anime"),
                        default="realesrgan-x4plus")
    parser.add_argument("--force",action="store_true")
    parser.add_argument("--batch-size",type=int,default=128,
                        help="Frames per Real-ESRGAN invocation (default 128; 0 = all)")
    parser.add_argument("--jobs",default="2:2:2",
                        help="NCNN load:process:save worker counts (default 2:2:2)")
    parser.add_argument("--gpu",type=int,default=None,
                        help="Optional Vulkan GPU index; default is automatic")
    args=parser.parse_args()
    if args.batch_size < 0:
        parser.error("--batch-size must be non-negative.")
    if not re.fullmatch(r"[1-9][0-9]*:[1-9][0-9]*:[1-9][0-9]*", args.jobs):
        parser.error("--jobs must use positive load:process:save counts.")
    if args.gpu is not None and args.gpu < 0:
        parser.error("--gpu must be non-negative.")
    exe=args.esrgan.resolve()
    models=args.models.resolve() if args.models else exe.parent/"models"
    if not exe.is_file() or not (models/(args.model+".param")).is_file() or not (models/(args.model+".bin")).is_file():
        parser.error("Real-ESRGAN executable or selected model files not found.")
    sources=sorted(args.cache.glob("*.scsrc"))
    if not sources:
        parser.error("No pilot unit sources. Enable AI graphics and enter a mission with a visible Hummer or Commando first.")
    pending, cached, transparent = [], 0, 0
    for source in sources:
        w, h, palette, indices = read_source(source)
        if not args.force and valid_cache(source.with_suffix(".scrgb"), w, h):
            cached += 1
        elif not any(i not in (0, 253) for i in indices):
            transparent += 1
        else:
            pending.append((source, w, h, palette, indices))
    print(f"Sources: {len(sources)} | cached: {cached} | transparent: {transparent} | pending: {len(pending)}", flush=True)
    generated = 0
    batch_size = args.batch_size or max(1, len(pending))
    try:
        for start in range(0, len(pending), batch_size):
            batch = pending[start:start+batch_size]
            print(f"Batch {start//batch_size+1}/{(len(pending)+batch_size-1)//batch_size}", flush=True)
            generated += process_batch(batch, exe, models, args.model, args.jobs, args.gpu)
            print(f"Progress: {generated}/{len(pending)} new frames", flush=True)
    except KeyboardInterrupt:
        print("Interrupted. Valid completed frames are saved; rerun to resume.", flush=True)
        raise SystemExit(130)
    print(f"Generated {generated} new frames; reused {cached}. Restart Spellcross.")
if __name__=="__main__":
    main()
