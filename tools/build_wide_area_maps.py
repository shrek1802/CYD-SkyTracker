#!/usr/bin/env python3
"""Build extra zoom 5-8 SkyTracker SD tiles without rewriting the proven z9-13 pack.

Natural Earth land and population points are public domain.  Data is loaded
from its official GitHub repository during CI; no OSM public tile scraping.
This pack overlays /maps/{z}/{x}/{y}.rgb565 (day) and /maps/night/... (night)
onto the existing FAT32 SD card.  Both are 256x256 RGB565 little-endian.
"""
import argparse
import json
import math
from pathlib import Path
import urllib.request
from zipfile import ZipFile, ZIP_DEFLATED

from PIL import Image, ImageDraw, ImageFont

from build_geofabrik_maps import PALETTES, as565, project

BASE = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/"
SOURCE_FILES = {
    "land": "ne_50m_land.geojson",
    "cities": "ne_50m_populated_places.geojson",
    "cities_detail": "ne_10m_populated_places.geojson",
}
# Broad world view at z5, most of Britain / neighbouring regions at z6-7,
# and a generous UK regional view at z8. Each radius also leaves room to pan.
RADIUS = {5: 950, 6: 700, 7: 480, 8: 300}
OCEAN = {"day": "#b7d6e3", "night": "#103047"}
COAST = {"day": "#82a8b5", "night": "#3f778e"}


def font(size, bold=False):
    name = "/usr/share/fonts/truetype/dejavu/DejaVuSans" + ("-Bold" if bold else "") + ".ttf"
    try:
        return ImageFont.truetype(name, size)
    except OSError:
        return ImageFont.load_default()


def read_source(kind, cache: Path):
    cache.mkdir(parents=True, exist_ok=True)
    name = SOURCE_FILES[kind]
    path = cache / name
    if not path.exists():
        req = urllib.request.Request(BASE + name, headers={
            "User-Agent": "CYD-SkyTracker/1.0 offline small-scale Natural Earth maps"
        })
        with urllib.request.urlopen(req, timeout=90) as response:
            raw = response.read(35_000_001)
        if len(raw) > 35_000_000:
            raise RuntimeError("Natural Earth source exceeds safety limit")
        path.write_bytes(raw)
    doc = json.loads(path.read_text(encoding="utf8"))
    if doc.get("type") != "FeatureCollection":
        raise ValueError("Invalid Natural Earth feature collection: " + kind)
    if len(doc.get("features", [])) < 20:
        raise ValueError("Unexpectedly small Natural Earth dataset: " + kind)
    return doc["features"]


def coords_to_polygons(geometry):
    if not geometry:
        return []
    shape = geometry.get("type")
    points = geometry.get("coordinates", [])
    if shape == "Polygon":
        return [points]
    if shape == "MultiPolygon":
        return points
    return []


def bbox_tiles(lat, lon, z, radius_km):
    cx, cy = project(lat, lon, z)
    pixels = radius_km * (256 * (1 << z)) / (40075.016686 * math.cos(math.radians(lat)))
    x0 = math.floor((cx - pixels)/256)
    x1 = math.floor((cx + pixels)/256)
    y0 = math.floor((cy - pixels)/256)
    y1 = math.floor((cy + pixels)/256)
    n = 1 << z
    x0, x1 = max(0,x0), min(n-1,x1)
    y0, y1 = max(0,y0), min(n-1,y1)
    if (x1-x0+1)*(y1-y0+1) > 200:
        raise ValueError("Too many wide-area tiles")
    return x0,x1,y0,y1


def draw_land(draws, features, z, style):
    palette = PALETTES[style]
    coast = COAST[style]
    tiles = draws.keys()
    minx = min(x for x,y in tiles)*256
    maxx = (max(x for x,y in tiles)+1)*256
    miny = min(y for x,y in tiles)*256
    maxy = (max(y for x,y in tiles)+1)*256
    any_shape = 0
    for feature in features:
        for polygon in coords_to_polygons(feature.get("geometry")):
            for ring_index, ring in enumerate(polygon):
                if len(ring) < 3:
                    continue
                # NE 50m can contain large worldwide polygons.  Filter by
                # world-pixel bounds before painting intersecting SD tiles.
                world = [project(lat, lon, z) for lon,lat,*_ in ring]
                bx0 = min(u for u,v in world)
                bx1 = max(u for u,v in world)
                by0 = min(v for u,v in world)
                by1 = max(v for u,v in world)
                if bx1 < minx or bx0 > maxx or by1 < miny or by0 > maxy:
                    continue
                any_shape += 1
                x0 = max(int(math.floor(bx0/256)), min(x for x,y in tiles))
                x1 = min(int(math.floor(bx1/256)), max(x for x,y in tiles))
                y0 = max(int(math.floor(by0/256)), min(y for x,y in tiles))
                y1 = min(int(math.floor(by1/256)), max(y for x,y in tiles))
                for tx in range(x0,x1+1):
                    for ty in range(y0,y1+1):
                        dr = draws.get((tx,ty))
                        if dr is None:
                            continue
                        local = [(round(u-tx*256), round(v-ty*256)) for u,v in world]
                        if ring_index:
                            dr.polygon(local, fill=OCEAN[style])
                        else:
                            dr.polygon(local, fill=palette["background"])
                            dr.line(local+[local[0]], fill=coast, width=1)
    if any_shape < 1:
        raise RuntimeError("No land shapes intersect the wide map")


def draw_cities(draws, features, z, style):
    palette = PALETTES[style]
    city_font = font(12 if z <= 6 else 13, True)
    small_font = font(11, False)
    # Show metropolitan labels at very broad zooms; add smaller cities as
    # the user zooms towards the Midlands. Cross-tile labels are continuous.
    maxrank = {5:2,6:4,7:6,8:8}[z]
    visible = []
    for feature in features:
        geometry = feature.get("geometry") or {}
        if geometry.get("type") != "Point":
            continue
        name = str(feature.get("properties",{}).get("NAME") or
                   feature.get("properties",{}).get("name") or "")
        rank = feature.get("properties",{}).get("SCALERANK",
               feature.get("properties",{}).get("scalerank",10))
        try:
            rank = float(rank)
            lon, lat = geometry["coordinates"][:2]
            lon, lat = float(lon), float(lat)
        except (ValueError,TypeError,KeyError,IndexError):
            continue
        if not name or rank > maxrank:
            continue
        u,v = project(lat,lon,z)
        tx,ty = math.floor(u/256),math.floor(v/256)
        if (tx,ty) in draws:
            visible.append((rank, name, u, v))
    used = []
    for rank,name,u,v in sorted(visible):
        f = city_font if rank <= 3 else small_font
        t = next(iter(draws.values()))
        bbox=t.textbbox((0,0),name,font=f)
        width,height=bbox[2]-bbox[0],bbox[3]-bbox[1]
        x=round(u-width/2)
        y=round(v-height-4)
        box=(x-3,y-3,x+width+3,y+height+3)
        if any(not (box[2]<b[0] or box[0]>b[2] or
                    box[3]<b[1] or box[1]>b[3]) for b in used):
            continue
        used.append(box)
        for tx in range(x//256,(x+width)//256+1):
            for ty in range(y//256,(y+height)//256+1):
                dr=draws.get((tx,ty))
                if dr:
                    dr.text((x-256*tx,y-256*ty),name,font=f,
                            fill=palette["label"],stroke_width=1,
                            stroke_fill=palette["label_halo"])
                    if rank <= 3:
                        dr.ellipse((round(u)-256*tx-2,round(v)-256*ty-2,
                                    round(u)-256*tx+2,round(v)-256*ty+2),
                                   fill=palette["label"])


def generate_level(land,cities,lat,lon,z,root):
    x0,x1,y0,y1=bbox_tiles(lat,lon,z,RADIUS[z])
    for style in ("day","night"):
        base=root/"maps" if style=="day" else root/"maps"/"night"
        images={(x,y):Image.new("RGB",(256,256),OCEAN[style])
                for x in range(x0,x1+1) for y in range(y0,y1+1)}
        draws={key:ImageDraw.Draw(img) for key,img in images.items()}
        draw_land(draws,land,z,style)
        draw_cities(draws,cities,z,style)
        for (x,y),im in images.items():
            dst=base/str(z)/str(x)
            dst.mkdir(parents=True,exist_ok=True)
            (dst/(str(y)+".rgb565")).write_bytes(as565(im))
    count=(x1-x0+1)*(y1-y0+1)
    print(f"Zoom {z}: {count} day + {count} night maps, range x{x0}-{x1} y{y0}-{y1}",flush=True)
    return count


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--lat",type=float,default=52.9548)
    parser.add_argument("--lon",type=float,default=-1.1581)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--cache",type=Path,default=Path("/tmp/skytracker-natural-earth"))
    args=parser.parse_args()
    if not (45 < args.lat < 60 and -12 < args.lon < 5):
        parser.error("This pack targets the United Kingdom and neighbouring areas")
    land=read_source("land",args.cache)
    cities=read_source("cities",args.cache)
    # The 1:10m places layer adds Nottingham, Derby, Sheffield, Leicester,
    # and other regional settlements missing from the 1:50m world overview.
    cities_detail=read_source("cities_detail",args.cache)
    output=args.output
    output.mkdir(parents=True,exist_ok=True)
    counts={z:generate_level(land,cities if z<=6 else cities_detail,
                             args.lat,args.lon,z,output) for z in RADIUS}
    maps=output/"maps"
    (maps/"wide-map-pack.json").write_text(json.dumps({
        "format":"CYD-SkyTracker-rgb565-le-v1",
        "purpose":"Add zoom levels 5-8 to existing day/night zoom 9-13 SD pack",
        "zoom_tile_counts":counts,"zoom_radius_km":RADIUS,
        "day_pattern":"/maps/z/x/y.rgb565",
        "night_pattern":"/maps/night/z/x/y.rgb565",
        "sources":SOURCE_FILES,
        "attribution":"Natural Earth (public domain); preserve OpenStreetMap credits for z9-13",
    },indent=2)+"\n")
    (maps/"wide-map-attribution.txt").write_text(
        "Natural Earth vector data, public domain. https://naturalearthdata.com\n"
        "Existing zoom 9-13 OpenStreetMap attribution must remain in maps/attribution.txt\n")
    archive=output/"SkyTracker-WIDE-AREA-ZOOM-5-8-SD.zip"
    with ZipFile(archive,"w",ZIP_DEFLATED,compresslevel=6) as zipped:
        for p in maps.rglob("*"):
            if p.is_file():
                zipped.write(p,p.relative_to(output))
    print("PACK:",archive,"day/night tiles:",sum(counts.values())*2,flush=True)


if __name__=="__main__":
    main()
