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
from PIL import Image, ImageOps, ImageDraw, ImageStat, ImageChops

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


# Manual photo-review framing profiles, based on the actual 136-image
# RGB565 contact sheet. zoom, horizontal centre, vertical centre.
# Only crop frames with clearly unnecessary surrounding sky/runway. Keep
# actual wing tips / rotor blades intact and never alter the model identity.
FOCUS = {
    "A109": (1.12, .47, .53),
    "A169": (1.14, .52, .58),
    "A310": (1.18, .52, .56),
    "A332": (1.12, .51, .55),
    "A343": (1.08, .53, .51),
    "A345": (1.10, .50, .47),
    "AT46": (1.16, .52, .56),
    "B06":  (1.13, .44, .54),
    "B407": (1.15, .51, .54),
    "B743": (1.10, .51, .57),
    "B763": (1.14, .50, .54),
    "F406": (1.13, .50, .53),
    "B764": (1.18, .49, .65),
    "B773": (1.17, .54, .55),
    "C152": (1.43, .49, .57),
    "C182": (1.38, .53, .55),
    "C510": (1.46, .47, .48),
    "C525": (1.14, .50, .54),
    "CRJ9": (1.39, .49, .57),
    "DH8A": (1.11, .52, .56),
    "E35L": (1.16, .55, .56),
    "E170": (1.18, .55, .61),
    "GLF6": (1.23, .51, .61),
    "GLF7": (1.13, .52, .56),
    "LJ45": (1.12, .54, .58),
    "PC24": (1.13, .55, .57),
    "R66": (1.32, .49, .62),
    "S76": (1.55, .54, .58),
    "SB20": (1.13, .53, .56)
}


def frame_image(image: Image.Image, size: tuple[int, int],
                code: str = "") -> Image.Image:
    """Safe camera framing followed by a pixel-exact RGB565 image.

    Crop input only when a reviewed focal profile exists. On landscape
    photos, use a modest vertical crop to fill the target without making
    the aircraft tiny; extra-wide and portrait photos are preserved whole.
    The same focus is applied to detail and fullscreen images.
    """
    source = image.convert("RGB")
    if code=="F406":
        from isolated_f406 import isolate
        return isolate(source,size)
    if code in FOCUS:
        zoom, fx, fy = FOCUS[code]
        crop_w = max(1, round(source.width / zoom))
        crop_h = max(1, round(source.height / zoom))
        left = min(max(0, round(source.width * fx - crop_w / 2)),
                   source.width - crop_w)
        top = min(max(0, round(source.height * fy - crop_h / 2)),
                  source.height - crop_h)
        source = source.crop((left, top, left+crop_w, top+crop_h))

    aspect=source.width / source.height
    desired=size[0]/size[1]
    # Only shorten vertical scenery for ordinary landscape photographs.
    # Never cut a long wingspan off the left/right edges to fill a 320px box.
    if desired/1.27 <= aspect <= desired:
        return ImageOps.fit(source,size,method=Image.Resampling.LANCZOS,
                            centering=(0.5,0.52))
    fitted = ImageOps.contain(source,size,method=Image.Resampling.LANCZOS)
    framed = Image.new("RGB",size,(20,29,39))
    framed.paste(fitted,((size[0]-fitted.width)//2,(size[1]-fitted.height)//2))
    return framed


def decode_rgb565(raw: bytes, width: int, height: int) -> Image.Image:
    """Independent LE decoder to verify pixel ordering and RGB channels."""
    if len(raw) != width*height*2:
        raise ValueError("RGB565 byte count mismatch")
    pixels = []
    for i in range(0, len(raw), 2):
        word = raw[i] | (raw[i+1] << 8)
        pixels.append((((word >> 11) & 31) * 255 // 31,
                       ((word >> 5) & 63) * 255 // 63,
                       (word & 31) * 255 // 31))
    result = Image.new("RGB", (width, height))
    result.putdata(pixels)
    return result


def prepare(jpg:Path) -> bytes:
    with Image.open(jpg) as src:
        src.load()
        im = ImageOps.exif_transpose(src).convert("RGB")
        # Natural-colour LANCZOS resizing only. The earlier contrast boost and
        # unsharp mask amplified JPEG artefacts on the small TFT.
        im = frame_image(im, (W,H), jpg.stem)
        payload = encode_rgb565(im)
        assert len(payload) == PIXEL_BYTES
        return payload


def prepare_full(jpg: Path) -> bytes:
    # A separate large image avoids stretching the 304x128 detail asset.
    # File is streamed one row at a time by ESP32; no large heap buffer.
    with Image.open(jpg) as src:
        src.load()
        im = ImageOps.exif_transpose(src).convert("RGB")
        im = frame_image(im, (FULL_W, FULL_H), jpg.stem)
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
    preview_tiles=[]
    channel_errors={}
    visual=args.output/"visual-review"
    visual.mkdir(parents=True,exist_ok=True)
    for photo in sorted(source.glob("*.jpg")):
        code=photo.stem
        credit=photo.with_suffix(".txt")
        if not (2<=len(code)<=4 and code.isalnum() and code.isupper() and credit.exists()):
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
        with Image.open(photo) as img:
            expected = frame_image(ImageOps.exif_transpose(img).convert("RGB"),
                                   (FULL_W, FULL_H), code)
        decoded = decode_rgb565(full, FULL_W, FULL_H)
        delta = ImageChops.difference(expected, decoded)
        mean = max(ImageStat.Stat(delta).mean)
        max_error = max(channel_range[1] for channel_range in delta.getextrema())
        # RGB565 floors 5-bit R/B and 6-bit G. A source pixel can differ
        # by up to 7 channel levels after a CORRECT round-trip. Mean errors
        # above five are possible for low saturated content: they are not
        # evidence of byte-swapping. Any >7 pixel mismatch IS impossible
        # from correct RGB565 quantisation and catches swapped RGB/BGR/bytes.
        if max_error > 7:
            raise RuntimeError(f"RGB565 channels/bytes wrong for {code}: max pixel channel error {max_error}")
        channel_errors[code] = round(mean, 3)
        tile = Image.new("RGB", (FULL_W*2, FULL_H+18), (16, 26, 37))
        tile.paste(expected, (0, 18))
        tile.paste(decoded, (FULL_W, 18))
        ImageDraw.Draw(tile).text((6, 4), code+"  SOURCE FIT | DECODED SD RGB565", fill="white")
        tile.save(visual/(code+".png"))
        preview_tiles.append(tile)
        imported.append(code)
        print("RGB565",code,len(raw),"detail bytes /",len(full),"full-screen bytes",flush=True)
    if not imported:
        raise RuntimeError("No credited JPEGs found: cannot build SD pack")
    for meta in ("attribution.json","coverage.json"):
        if (source/meta).is_file():
            shutil.copyfile(source/meta,destination/meta)
    (destination/"conversion-report.json").write_text(json.dumps({
        "converted_count":len(imported), "converted_codes":sorted(imported),
        "max_mean_channel_error":max(channel_errors.values()),
        "mean_channel_errors":channel_errors,
        "byte_order":"little-endian RGB565"
    },indent=2)+"\n",encoding="utf8")
    columns = 2
    step_h = FULL_H + 18
    review_sheet = Image.new("RGB", (columns*FULL_W*2,
                        ((len(preview_tiles)+columns-1)//columns)*step_h), (16,26,37))
    for index,tile in enumerate(preview_tiles):
        review_sheet.paste(tile,((index%columns)*FULL_W*2,(index//columns)*step_h))
    review_sheet.save(args.output/"RGB565-original-vs-decoded-review.jpg", quality=90)
    print("PASS: RGB565 visual roundtrip, worst mean channel error",
          max(channel_errors.values()), flush=True)
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
