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

from PIL import Image, ImageOps, ImageDraw, ImageStat

USER_AGENT = "CYD-SkyTracker/0.4 (https://github.com/shrek1802/CYD-SkyTracker)"
COMMONS = "https://commons.wikimedia.org/w/api.php"
MODELS = {
    "A318": ("Airbus A318", "Airbus A318"),
    "A319": ("Airbus A319", "Airbus A319"),
    "A320": ("Airbus A320", "Airbus A320"),
    "A20N": ("Airbus A320neo", "Airbus A320neo"),
    "A321": ("Airbus A321", "Airbus A321"),
    "A21N": ("Airbus A321neo", "Airbus A321neo"),
    "BCS1": ("Airbus A220-100", "Airbus A220-100"),
    "BCS3": ("Airbus A220-300", "Airbus A220-300"),
    "A332": ("Airbus A330-200", "Airbus A330-200"),
    "A333": ("Airbus A330-300", "Airbus A330-300"),
    "A339": ("Airbus A330-900neo", "Airbus A330-900neo"),
    "A359": ("Airbus A350-900", "Airbus A350-900"),
    "A35K": ("Airbus A350-1000", "Airbus A350-1000"),
    "A388": ("Airbus A380-800", "Airbus A380-800"),
    "B737": ("Boeing 737-700", "Boeing 737-700"),
    "B738": ("Boeing 737-800", "Boeing 737-800"),
    "B739": ("Boeing 737-900", "Boeing 737-900"),
    "B38M": ("Boeing 737 MAX 8", "Boeing 737 MAX 8"),
    "B39M": ("Boeing 737 MAX 9", "Boeing 737 MAX 9"),
    "B744": ("Boeing 747-400", "Boeing 747-400"),
    "B748": ("Boeing 747-8", "Boeing 747-8"),
    "B752": ("Boeing 757-200", "Boeing 757-200"),
    "B753": ("Boeing 757-300", "Boeing 757-300"),
    "B763": ("Boeing 767-300", "Boeing 767-300"),
    "B764": ("Boeing 767-400", "Boeing 767-400"),
    "B772": ("Boeing 777-200", "Boeing 777-200"),
    "B77W": ("Boeing 777-300ER", "Boeing 777-300ER"),
    "B788": ("Boeing 787-8", "Boeing 787-8"),
    "B789": ("Boeing 787-9", "Boeing 787-9"),
    "B78X": ("Boeing 787-10", "Boeing 787-10"),
    "E170": ("Embraer 170", "Embraer 170"),
    "E175": ("Embraer 175", "Embraer 175"),
    "E190": ("Embraer 190", "Embraer 190"),
    "E195": ("Embraer 195", "Embraer 195"),
    "E290": ("Embraer E190-E2", "Embraer E190-E2"),
    "E295": ("Embraer E195-E2", "Embraer E195-E2"),
    "AT72": ("ATR 72", "ATR 72"),
    "AT76": ("ATR 72-600", "ATR 72-600"),
    "DH8D": ("De Havilland Canada Dash 8-400", "De Havilland Canada Dash 8-400"),
    "CRJ2": ("Bombardier CRJ200", "Bombardier CRJ200"),
    "CRJ7": ("Bombardier CRJ700", "Bombardier CRJ700"),
    "CRJ9": ("Bombardier CRJ900", "Bombardier CRJ900"),
    "C172": ("Cessna 172", "Cessna 172"),
    "C182": ("Cessna 182", "Cessna 182"),
    "C208": ("Cessna 208 Caravan", "Cessna 208 Caravan"),
    "PA28": ("Piper PA-28", "Piper PA-28"),
    "PC12": ("Pilatus PC-12", "Pilatus PC-12"),
    "C25A": ("Cessna Citation CJ2", "Cessna Citation CJ2"),
    "C56X": ("Cessna Citation Excel", "Cessna Citation Excel"),
    "GLF5": ("Gulfstream V", "Gulfstream V"),
    "GLF6": ("Gulfstream G650", "Gulfstream G650"),
    "CL60": ("Bombardier Challenger 600", "Bombardier Challenger 600"),
    "LJ45": ("Learjet 45", "Learjet 45"),
    "B77F": ("Boeing 777F", "Boeing 777F"),
    "A400": ("Airbus A400M", "Airbus A400M"),
    "C130": ("Lockheed C-130 Hercules", "Lockheed C-130 Hercules"),
    "C17": ("Boeing C-17 Globemaster III", "Boeing C-17 Globemaster III"),
    "E145": ("Embraer ERJ 145", "Embraer ERJ 145"),
    "SF34": ("Saab 340", "Saab 340"),
    "C152": ("Cessna 152", "Cessna 152"),
    "DA40": ("Diamond DA40", "Diamond DA40"),
    "DA42": ("Diamond DA42", "Diamond DA42"),
    "SR22": ("Cirrus SR22", "Cirrus SR22"),
    "EC35": ("Eurocopter EC135", "Eurocopter EC135"),
    "A139": ("Leonardo AW139", "Leonardo AW139"),
    "R44": ("Robinson R44", "Robinson R44"),
    "S76": ("Sikorsky S-76", "Sikorsky S-76"),
    "CL35": ("Bombardier Challenger 350", "Bombardier Challenger 350"),
}
# Hand-picked EXTERIOR airframe images, verified as examples of each model.
# Commons licensing and author metadata are fetched afresh and validated.
# Do not replace these with arbitrary first-search hits.
PINNED = {
    "A319": "File:Airbus A319 easyJet.JPG",
    "A320": "File:EasyJet Airbus A320-200 G-EZRT.jpg",
    "A321": "File:EVA Air Airbus A321-211 B-16202 Taking off from Taipei Songshan Airport 20140917.jpg",
    "B738": "File:Ryanair UK Boeing 737-800.jpg",
    "B38M": "File:N8801Q - Boeing 737 MAX-8 - Southwest Airlines.jpg",
    "AT76": "File:ATR 72-600 in flight.jpg",
    "A21N": "File:Airbus A321neo EI-LRE MD1.jpg",
    # Visually reviewed alternatives to sky-only / cabin / engine images.
    "A35K": "File:Cathay A35K.jpg",
    "B739": "File:Boeing 737-900 (26889681761).jpg",
    "B753": "File:Boeing 757 300 plane aircraft.jpg",
    "CRJ9": "File:Bombardier CRJ900.JPG",
    "GLF6": "File:Gulfstream G650.JPG",
}

BAD = ("cockpit", "interior", "cabin", "safety card", "seat map", "logo",
       "diagram", "simulation", "illustration", "engine detail", "scale model",
       "wing view", "seat", "floorplan", "emergency oxygen",
       "oxygen mask", "engine", "turbine", "winglet", "close-up",
       "landing gear", "undercarriage")


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
        "iiextmetadatafilter": "Artist|LicenseShortName|LicenseUrl",
        "iiurlwidth": "700", "format": "json", "formatversion": "2",
    }
    p.update(params)
    return json.loads(request(COMMONS + "?" + urllib.parse.urlencode(p)))


def acceptable(lic):
    upper = lic.strip().upper()
    if upper in ("CC0", "PUBLIC DOMAIN") or upper.startswith(("CC0 ", "PUBLIC DOMAIN ")):
        return True
    # Full match is essential: CC-BY-NC and CC-BY-ND are not permitted.
    return re.fullmatch(r"CC[ -]BY(?:[ -]SA)?[ -](?:1\.0|2\.0|2\.5|3\.0|4\.0)", upper) is not None


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
            artist = cleaner.get()
            if not artist or artist.lower() in ("unknown", "anonymous"):
                continue
            source = image.get("descriptionurl", "")
            if not source.startswith("https://commons.wikimedia.org/"):
                print("  rejected source",repr(source[:120]),flush=True)
                continue
            return {
                "title": page.get("title", ""), "url": u, "licence": lic,
                "artist": artist[:95], "source": source,
                "licence_url": image.get("extmetadata", {}).get(
                    "LicenseUrl", {}).get("value", "")}
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
        if min(original.size)<250:
            raise ValueError("Image too small for 320px display")
        original.thumbnail((1000,700),Image.Resampling.LANCZOS)
        if ImageStat.Stat(original.convert("L")).stddev[0] < 12:
            raise ValueError("Low-detail image; manual review required")
        output = io.BytesIO()
        original.save(output, format="JPEG", quality=87, optimize=True,
                      progressive=False, subsampling="4:2:0")
        jpeg = output.getvalue()
    if not (jpeg.startswith(b"\xff\xd8") and 0 < len(jpeg) < 350_000):
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
    unavailable = {}
    for code, (model, category) in MODELS.items():
        try:
            print(f"Looking up {code}: {model} ...", flush=True)
            entry = lookup(model, category, PINNED.get(code))
            if not entry:
                print("  no clearly licensed JPEG, skipped", flush=True)
                unavailable[code] = "No suitable licensed exterior photograph"
                continue
            size = make_photo(entry, photos / f"{code}.jpg")
            for key in ("artist", "licence", "source"):
                entry[key] = " ".join(entry[key].replace("\n", " ").split())
            (photos / f"{code}.txt").write_text(
                entry["artist"] + "\n" + entry["licence"] + "\n" +
                entry["source"] + "\n" +
                (entry.get("licence_url") or "") + "\n", encoding="utf8")
            manifest[code] = {"model": model, "bytes": size,
                              "review_status": "curated" if code in PINNED else "automated_unreviewed",
                              **entry}
            print(f"  wrote {size} byte photo, licence {entry['licence']}", flush=True)
        except Exception as exc:
            (photos / f"{code}.jpg").unlink(missing_ok=True)
            (photos / f"{code}.txt").unlink(missing_ok=True)
            unavailable[code] = str(exc)[:180]
            print(f"  unavailable: {exc}", flush=True)
    if len(manifest) < 4:
        raise SystemExit(f"Insufficient licensed photos ({len(manifest)})")
    (photos/"coverage.json").write_text(json.dumps({
        "requested":len(MODELS), "downloaded":len(manifest),
        "available_codes":sorted(manifest), "missing":unavailable,
        "curated_codes":sorted(code for code in manifest if code in PINNED),
        "automated_unreviewed_codes":sorted(code for code in manifest if code not in PINNED)
    },indent=2,ensure_ascii=False)+"\n",encoding="utf8")
    if manifest:
        codes=sorted(manifest)
        cols,tw,th=4,240,130
        contact=Image.new("RGB",(cols*tw,th*((len(codes)+cols-1)//cols)),"#0c1726")
        draw=ImageDraw.Draw(contact)
        for i,code in enumerate(codes):
            with Image.open(photos/f"{code}.jpg") as im:
                thumb=ImageOps.fit(im.convert("RGB"),(232,102),method=Image.Resampling.LANCZOS)
            x,y=(i%cols)*tw+4,(i//cols)*th+4
            contact.paste(thumb,(x,y))
            draw.text((x+3,y+106),code+"  "+MODELS[code][0][:26],fill="#ffffff")
        contact.save(args.output/"aircraft-photo-contact-sheet.jpg",quality=88)
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
    print("PACK CREATED:", archive, "photos:", len(manifest), "missing:", len(unavailable),flush=True)


if __name__ == "__main__":
    main()
