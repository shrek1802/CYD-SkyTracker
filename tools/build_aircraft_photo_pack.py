#!/usr/bin/env python3
"""Build an optional self-contained, openly licensed model-photo pack for CYD.

Wikimedia Commons supplies a *representative model photo*, NEVER the actual
airframe/registration; full source and licence are recorded per JPEG.
Run in GitHub Actions (not in firmware) to make /aircraft/ files.
"""
from __future__ import annotations

import argparse
import html
import io
import json
import re
import time
import urllib.parse
import urllib.request
from html.parser import HTMLParser
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

from PIL import Image, ImageOps

USER_AGENT = "CYD-SkyTracker/0.4 (https://github.com/shrek1802/CYD-SkyTracker)"
COMMONS = "https://commons.wikimedia.org/w/api.php"
MODELS = {
    "A319": ("Airbus A319", "Airbus A319"),
    "A320": ("Airbus A320", "Airbus A320"),
    "A20N": ("Airbus A320neo", "Airbus A320neo"),
    "A321": ("Airbus A321", "Airbus A321"),
    "A21N": ("Airbus A321neo", "Airbus A321neo"),
    "B738": ("Boeing 737-800", "Boeing 737-800"),
    "B38M": ("Boeing 737 MAX 8", "Boeing 737 MAX 8"),
    "B789": ("Boeing 787-9", "Boeing 787-9"),
    "E190": ("Embraer 190", "Embraer 190"),
    "AT76": ("ATR 72-600", "ATR 72-600"),
}
# Hand-picked EXTERIOR airframe images, verified as examples of each model.
# Commons licensing and author metadata are fetched afresh and validated.
# Do not replace these with arbitrary first-search hits.
PINNED = {
    "A319": "File:Airbus A319 easyJet.JPG",
    "A320": "File:Airbus A320 (British Airways) (3338770065).jpg",
    "A321": "File:Airbus A321.jpg",
    "B738": "File:Ryanair UK Boeing 737-800.jpg",
}

BAD = ("cockpit", "interior", "cabin", "safety card", "seat map", "logo",
       "diagram", "simulation", "illustration", "engine detail", "scale model",
       "wing view", "seat", "floorplan")


class Plain(HTMLParser):
    def __init__(self):
        super().__init__(); self.parts = []
    def handle_data(self, data):
        self.parts.append(data)
    def get(self):
        return " ".join(" ".join(self.parts).split())


def request(url):
    req = urllib.request.Request(url, headers={
        "User-Agent": USER_AGENT, "Accept": "application/json, image/jpeg"})
    for attempt in range(3):
        try:
            with urllib.request.urlopen(req, timeout=25) as f:
                return f.read(2_000_000)
        except Exception as exc:
            if attempt >= 2:
                raise
            print(f"retrying after {exc}", flush=True)
            time.sleep(attempt + 2)


def api(params):
    p = {
        "action": "query", "prop": "imageinfo",
        "iiprop": "url|extmetadata|mime",
        "iiextmetadatafilter": "Artist|LicenseShortName",
        "iiurlwidth": "700", "format": "json", "formatversion": "2",
    }
    p.update(params)
    return json.loads(request(COMMONS + "?" + urllib.parse.urlencode(p)))


def acceptable(lic):
    upper = lic.strip().upper()
    return upper in ("CC0", "PUBLIC DOMAIN") or upper.startswith(
        ("CC0 ", "PUBLIC DOMAIN ", "CC BY ", "CC BY-SA ", "CC-BY-", "CC-BY-SA-"))


def good_filename(name, model):
    """Require a visible aircraft model in the title, not cabin/wing/sky pics."""
    low = name.lower()
    reject = BAD + ("hangar", "sunspot", "contrail", "sunset", "passagierbereich",
                    "innenraum", "cruising from", "view from", "winglet",
                    "underwing", "luggage", "tail of", "flight deck", "skyline",
                    "gear", "moon", "hold előtt", " & sun", "with sun",
                    "at sunset", "in the sun", "passenger area")
    if any(item in low for item in reject):
        return False
    letters = re.sub(r"[^a-z0-9]", "", low)
    m = re.sub(r"[^a-z0-9]", "", model.lower())
    if m.startswith("airbus"):
        needle = m[6:]
    elif m.startswith("boeing"):
        needle = m[6:]
    elif m.startswith("embraer"):
        needle = m[7:]
    else:
        needle = m
    if not (needle in letters or m in letters):
        return False
    # Airbus A320 != A320neo; Boeing 737-800 != 737 MAX 8.
    if needle in ("a319","a320","a321") and (needle+"neo") in letters:
        return False
    return True


def select(pages, model, pinned=False):
    for page in pages:
        if not pinned and not good_filename(page.get("title", ""), model):
            continue
        for image in page.get("imageinfo", []):
            lic = image.get("extmetadata", {}).get(
                "LicenseShortName", {}).get("value", "")
            u = image.get("thumburl", "")
            if (image.get("mime") != "image/jpeg" or
                not acceptable(lic) or not
                u.startswith(("https://upload.wikimedia.org/",
                                "https://thumb.wikimedia.org/"))):
                continue
            artist_html = image.get("extmetadata", {}).get(
                "Artist", {}).get("value", "")
            cleaner = Plain()
            cleaner.feed(artist_html)
            artist = cleaner.get() or "Wikimedia contributor"
            source = image.get("descriptionurl", "")
            if not source.startswith("https://commons.wikimedia.org/"):
                print("  rejected source",repr(source[:120]),flush=True)
                continue
            return {
                "title": page.get("title", ""), "url": u, "licence": lic,
                "artist": artist[:95], "source": source}
    return None


def lookup(model, category, pinned_title=None):
    if pinned_title:
        q = api({"titles": pinned_title})
        match = select(q.get("query", {}).get("pages", []), model, pinned=True)
        if not match:
            raise ValueError(f"Curated Commons file invalid, missing, or not freely licensed: {pinned_title}")
        return match
    # Prefer a named model in the image filename: exact search ranks recognisable
    # outside aircraft photos ahead of generic category members (wings/cabins).
    q = api({"generator": "search", "gsrnamespace": "6", "gsrlimit": "40",
             "gsrsearch": 'intitle:"' + model + '"'})
    match = select(q.get("query", {}).get("pages", []), model)
    if match:
        return match
    q = api({"generator": "categorymembers",
             "gcmtitle": "Category:" + category,
             "gcmnamespace": "6", "gcmlimit": "50"})
    return select(q.get("query", {}).get("pages", []), model)


def make_photo(entry, dst):
    body = request(entry["url"])
    with Image.open(io.BytesIO(body)) as source:
        original = ImageOps.exif_transpose(source).convert("RGB")
        original.thumbnail((700, 500), Image.Resampling.LANCZOS)
        output = io.BytesIO()
        original.save(output, format="JPEG", quality=77, optimize=True,
                      progressive=False, subsampling="4:2:0")
        jpeg = output.getvalue()
    if not (jpeg.startswith(b"\xff\xd8") and 0 < len(jpeg) < 90_000):
        raise ValueError("JPEG exceeds device memory limit")
    dst.write_bytes(jpeg)
    return len(jpeg)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--output", type=Path, default=Path("dist"))
    args = p.parse_args()
    photos = args.output / "aircraft"
    photos.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for code, (model, category) in MODELS.items():
        try:
            print(f"Looking up {code}: {model} ...", flush=True)
            entry = lookup(model, category, PINNED.get(code))
            if not entry:
                print("  no clearly licensed JPEG, skipped", flush=True)
                continue
            size = make_photo(entry, photos / f"{code}.jpg")
            for key in ("artist", "licence", "source"):
                entry[key] = " ".join(entry[key].replace("\n", " ").split())
            (photos / f"{code}.txt").write_text(
                entry["artist"] + "\n" + entry["licence"] + "\n" +
                entry["source"] + "\n", encoding="utf8")
            manifest[code] = {"model": model, "bytes": size, **entry}
            print(f"  wrote {size} byte photo, licence {entry['licence']}", flush=True)
        except Exception as exc:
            (photos / f"{code}.jpg").unlink(missing_ok=True)
            (photos / f"{code}.txt").unlink(missing_ok=True)
            print(f"  unavailable: {exc}", flush=True)
    if len(manifest) < 4:
        raise SystemExit(f"Insufficient licensed photos ({len(manifest)})")
    (photos / "README.txt").write_text(
        "Representative model photos from Wikimedia Commons.\n"
        "NOT photographs of the particular aircraft being tracked.\n"
        "Each JPEG has matching photographer, licence and original source\n"
        "in its .txt file. Redistribute with these credits; obey licences.\n"
        "SkyTracker uses ICAO type names (e.g. B738) as filenames.\n")
    (photos / "attribution.json").write_text(json.dumps(
        manifest, indent=2, ensure_ascii=False), encoding="utf8")
    archive = args.output / "SkyTracker-Representative-Plane-Photos-SD.zip"
    with ZipFile(archive, "w", ZIP_DEFLATED, compresslevel=7) as z:
        for f in photos.rglob("*"):
            if f.is_file():
                z.write(f, f.relative_to(args.output))
    print("PACK CREATED:", archive, "photos:", len(manifest), flush=True)


if __name__ == "__main__":
    main()
