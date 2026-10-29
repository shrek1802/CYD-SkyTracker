#!/usr/bin/env python3
"""Generate a deterministic SD RGB565-LE colour test and PNG reference.

Copy diagnostics/rgb565-check.rgb565 onto the FAT32 SD card, then
send COLOUR_TEST to the running firmware at 115200 baud.
"""
from pathlib import Path
import argparse
import hashlib
from zipfile import ZipFile, ZIP_DEFLATED
from PIL import Image

WIDTH, HEIGHT = 320, 96
COLOURS = (
    (255,0,0), (0,255,0), (0,0,255), (255,255,255),
    (255,255,0), (0,255,255), (255,0,255), (0,0,0)
)

def make_image():
    im=Image.new("RGB",(WIDTH,HEIGHT))
    pixels=im.load()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            pixels[x,y]=COLOURS[x//40]
    return im

def encode(im):
    if im.size!=(WIDTH,HEIGHT): raise ValueError("Incorrect diagnostic dimensions")
    buf=bytearray(WIDTH*HEIGHT*2)
    for i,(r,g,b) in enumerate(im.convert("RGB").getdata()):
        v=((r&248)<<8)|((g&252)<<3)|(b>>3)
        buf[2*i]=v&255
        buf[2*i+1]=v>>8
    return bytes(buf)

def build(out):
    folder=out/"diagnostics"
    folder.mkdir(parents=True,exist_ok=True)
    original=make_image()
    raw=encode(original)
    target=folder/"rgb565-check.rgb565"
    target.write_bytes(raw)
    preview=out/"rgb565-check-reference.png"
    original.save(preview)
    readme=out/"RGB565-TEST-INSTRUCTIONS.txt"
    readme.write_text(
        "CYD SkyTracker RGB565 colour test.\n"
        "Copy diagnostics/rgb565-check.rgb565 onto microSD root.\n"
        "Run the existing firmware and send COLOUR_TEST over 115200 baud.\n"
        "Compare the native TFT swatches on top to the SD swatches underneath.\n"
        "If only the lower colours differ, inspect byte swap and SD read path.\n"
        "If both differ from the PNG, check TFT panel RGB/BGR/inversion.\n"
        "This test does not erase settings, flash, or format SD.\n"
        "File SHA-256: "+hashlib.sha256(raw).hexdigest()+"\n",
        encoding="utf-8")
    archive=out/"SkyTracker-RGB565-Colour-Diagnostic-SD.zip"
    with ZipFile(archive,"w",ZIP_DEFLATED) as zipfile:
        for f in (target,preview,readme):
            zipfile.write(f,f.relative_to(out))
    return archive

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    print("Created",build(args.output))
