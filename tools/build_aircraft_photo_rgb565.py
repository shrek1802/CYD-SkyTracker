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
from PIL import Image, ImageOps, ImageEnhance, ImageFilter

W, H = 304, 128
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
        # Use a full-width landscape crop rather than letterboxing a tiny
        # image and wasting pixels on empty dark borders. LANCZOS resampling,
        # mild contrast and unsharp masking preserve aircraft details at 304px.
        im = ImageOps.fit(im, (W,H), method=Image.Resampling.LANCZOS,
                          centering=(0.5,0.5))
        im = ImageEnhance.Contrast(im).enhance(1.08)
        im = im.filter(ImageFilter.UnsharpMask(radius=1.0,percent=115,threshold=2))
        payload = encode_rgb565(im)
        assert len(payload) == PIXEL_BYTES
        return payload


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
        (destination/(code+".txt")).write_bytes(credit.read_bytes())
        imported.append(code)
        print("RGB565",code,len(raw),"bytes",flush=True)
    if not imported:
        raise RuntimeError("No credited JPEGs found: cannot build SD pack")
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
