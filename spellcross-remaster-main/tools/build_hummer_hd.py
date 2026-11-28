#!/usr/bin/env python3
"""Build a local Hummer HD cache with the official Real-ESRGAN Vulkan CLI.
Requires Pillow and a separately installed Real-ESRGAN executable and models.
Never modifies source assets; no interpolation-only fallback is written.
"""
import argparse
import collections
import pathlib
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

def process(source, executable, models, model, force):
    w,h,palette,indices=read_source(source)
    target=source.with_suffix(".scrgb")
    if not force and valid_cache(target,w,h):
        return False
    if not any(i not in (0,253) for i in indices):
        return False
    with tempfile.TemporaryDirectory(prefix="spellcross-hd-") as work:
        work=pathlib.Path(work)
        inp,out=work/"input.png",work/"output.png"
        prepared_image(w,h,palette,indices).save(inp)
        subprocess.run([str(executable),"-i",str(inp),"-o",str(out),
                        "-m",str(models),"-n",model,"-s","4","-f","png"],
                       check=True,cwd=executable.parent)
        with Image.open(out) as result:
            if result.size!=((w+2*PAD)*SCALE,(h+2*PAD)*SCALE):
                raise ValueError(f"Unexpected neural output size: {result.size}")
            image=result.convert("RGB").crop((PAD*SCALE,PAD*SCALE,
                (PAD+w)*SCALE,(PAD+h)*SCALE))
            # The engine uses the original exact opaque pixel mask, not
            # model-generated alpha. No new silhouette or shadow can leak.
            payload=RGB_MAGIC+struct.pack("<II",w*SCALE,h*SCALE)+image.tobytes()
        temporary=target.with_suffix(".scrgb.tmp")
        temporary.write_bytes(payload)
        temporary.replace(target)
    return True

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache",type=pathlib.Path,default=pathlib.Path("ai_hummer_cache"))
    parser.add_argument("--esrgan",type=pathlib.Path,required=True,help="Real-ESRGAN ncnn Vulkan executable")
    parser.add_argument("--models",type=pathlib.Path,help="Model directory; default executable/models")
    parser.add_argument("--model",choices=("realesrgan-x4plus","realesrnet-x4plus","realesrgan-x4plus-anime"),
                        default="realesrgan-x4plus")
    parser.add_argument("--force",action="store_true")
    args=parser.parse_args()
    exe=args.esrgan.resolve()
    models=args.models.resolve() if args.models else exe.parent/"models"
    if not exe.is_file() or not (models/(args.model+".param")).is_file() or not (models/(args.model+".bin")).is_file():
        parser.error("Real-ESRGAN executable or selected model files not found.")
    sources=sorted(args.cache.glob("*.scsrc"))
    if not sources:
        parser.error("No Hummer sources. Enable AI graphics and enter a mission with a visible Hummer first.")
    generated=0
    for i,source in enumerate(sources,1):
        if process(source,exe,models,args.model,args.force):
            generated+=1
            print(f"[{i}/{len(sources)}] {source.stem}: neural HD cache written",flush=True)
    print(f"Generated {generated} of {len(sources)} source frames. Restart Spellcross.")
if __name__=="__main__":
    main()
