#!/usr/bin/env python3
"""Convert licensed, credited aircraft JPEGs into pre-rendered RGB565 tiles.

No web requests, no ESP32 decoder, no opaque source code dependencies.
Files are 304x128 pixels, 16-bit *little-endian* RGB565. The renderer can
send these rows directly to TFT_eSPI, as SkyTracker already does for maps.
"""
from __future__ import annotations
import argparse
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import shutil
import json
from PIL import Image, ImageOps

W, H = 304, 128
FULL_W, FULL_H = 320, 166
PIXEL_BYTES = W * H * 2


def encode_rgb565(image: Image.Image) -> bytes:
    rgb = image.convert("RGB").tobytes()
    out = bytearray(len(rgb)//3*2)
    j=0
    for i in range(0,len(rgb),3):
        r,g,b = rgb[i], rgb[i+1], rgb[i+2]
        p = ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3)
        out[j] = p & 255
        out[j+1] = (p >> 8) & 255
        j += 2
    return bytes(out)


def prepare(jpg:Path) -> bytes:
    with Image.open(jpg) as src:
        src.load()
        im = ImageOps.exif_transpose(src).convert("RGB")
        # Natural-colour LANCZOS resizing only. The earlier contrast boost and
        # unsharp mask amplified JPEG artefacts on the small TFT.
        im = ImageOps.fit(im, (W,H), method=Image.Resampling.LANCZOS,
                          centering=(0.5,0.5))
        payload = encode_rgb565(im)
        assert len(payload) == PIXEL_BYTES
        return payload


def prepare_full(jpg: Path) -> bytes:
    # A separate large image avoids stretching the 304x128 detail asset.
    # File is streamed one row at a time by ESP32; no large heap buffer.
    with Image.open(jpg) as src:
        src.load()
        im = ImageOps.exif_transpose(src).convert("RGB")
        im = ImageOps.fit(im, (FULL_W, FULL_H),
                          method=Image.Resampling.LANCZOS,
                          centering=(0.5, 0.5))
        pixels = encode_rgb565(im)
        assert len(pixels) == FULL_W*FULL_H*2
        return pixels


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--input",type=Path,required=True,
                   help="Directory containing aircraft/*.jpg and matching .txt")
    p.add_argument("--output",type=Path,required=True)
    args=p.parse_args()
    source=args.input/"aircraft" if (args.input/"aircraft").is_dir() else args.input
    destination=args.output/"aircraft"
    destination.mkdir(parents=True,exist_ok=True)
    imported=[]
    for photo in sorted(source.glob("*.jpg")):
        code=photo.stem
        credit=photo.with_suffix(".txt")
        if not (len(code)==4 and code.isalnum() and code.isupper() and credit.exists()):
            print("SKIP missing metadata / malformed type:",photo.name,flush=True)
            continue
        lines=credit.read_text(encoding="utf8").splitlines()
        if len(lines)<3 or not lines[0] or not lines[1] or not lines[2].startswith("https://commons.wikimedia.org/"):
            print("SKIP malformed photographer / licence / source:",code,flush=True)
            continue
        raw=prepare(photo)
        (destination/(code+".rgb565")).write_bytes(raw)
        full = prepare_full(photo)
        (destination/(code+".full.rgb565")).write_bytes(full)
        (destination/(code+".txt")).write_bytes(credit.read_bytes())
        imported.append(code)
        print("RGB565",code,len(raw),"detail bytes /",len(full),"full-screen bytes",flush=True)
    if not imported:
        raise RuntimeError("No credited JPEGs found: cannot build SD pack")
    for meta in ("attribution.json","coverage.json"):
        if (source/meta).is_file():
            shutil.copyfile(source/meta,destination/meta)
    (destination/"conversion-report.json").write_text(json.dumps({
        "converted_count":len(imported), "converted_codes":sorted(imported)
    },indent=2)+"\n",encoding="utf8")
    (destination/"README.txt").write_text(
        "CYD SkyTracker direct-render aircraft model photographs\n"
        "Each .rgb565 is exactly 304x128 pixels, little-endian RGB565.\n"
        "Copy aircraft/ to the root of FAT32 microSD, beside maps/.\n"
        "Photographs are REPRESENTATIVE aircraft TYPE images, not the\n"
        "specific live registration. Source and licence in every .txt.\n"
        "No JPEG decoding or internet needed once SD has these files.\n",
        encoding="utf8")
    output=args.output/"SkyTracker-Direct-RGB565-Aircraft-Photos.zip"
    with ZipFile(output,"w",ZIP_DEFLATED,compresslevel=6) as z:
        for item in destination.iterdir():
            if item.is_file():z.write(item,item.relative_to(args.output))
    print("PACK:",output,"count:",len(imported),flush=True)

if __name__=="__main__":
    main()
