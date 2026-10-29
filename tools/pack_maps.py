#!/usr/bin/env python3
"""Prepare *already legally obtained* MBTiles for CYD SkyTracker microSD.

No network access, no tile downloads, no public OSM tile scraping.
Reads raster PNG/JPEG/WEBP MBTiles in the standard TMS tile_row scheme.
Outputs raw little-endian RGB565: /maps/<z>/<x>/<y>.rgb565 (XYZ y).
"""
import argparse
import io
import json
import math
import sqlite3
import struct
import sys
from pathlib import Path
from PIL import Image

TILE_SIZE = 256
MAX_LAT = 85.05112878

def xy(latitude, longitude, zoom):
    lat = max(-MAX_LAT, min(MAX_LAT, latitude))
    n = 1 << zoom
    lon_x = (longitude + 180.0) / 360.0 * n
    lat_r = math.radians(lat)
    lat_y = (1.0 - math.asinh(math.tan(lat_r)) / math.pi) / 2.0 * n
    return lon_x, lat_y

def coverage(latitude, longitude, radius_km, zoom):
    # Conservative bounding rectangle; not a precise radius-shaped cutout.
    lat_delta = radius_km / 110.574
    coslat = max(0.08, math.cos(math.radians(latitude)))
    lon_delta = radius_km / (111.320 * coslat)
    if longitude - lon_delta < -180.0 or longitude + lon_delta >= 180.0:
        raise ValueError("Map pack crossing the 180th meridian is not supported yet")
    min_lat = max(-MAX_LAT, latitude - lat_delta)
    max_lat = min(MAX_LAT, latitude + lat_delta)
    x0, y0 = xy(max_lat, longitude - lon_delta, zoom)
    x1, y1 = xy(min_lat, longitude + lon_delta, zoom)
    n = 1 << zoom
    return (
        max(0, math.floor(x0)), min(n-1, math.floor(x1)),
        max(0, math.floor(y0)), min(n-1, math.floor(y1))
    )

def rgb565(image_data):
    with Image.open(io.BytesIO(image_data)) as source:
        rgb = source.convert("RGB")
    if rgb.size != (TILE_SIZE, TILE_SIZE):
        raise ValueError(f"Expected 256x256 raster tile, got {rgb.size}")
    src = rgb.tobytes()
    data = bytearray(TILE_SIZE * TILE_SIZE * 2)
    offset = 0
    for p in range(0, len(src), 3):
        r, g, b = src[p], src[p+1], src[p+2]
        struct.pack_into("<H", data, offset,
                         ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
        offset += 2
    return data

def export(mbtiles, output, lat, lon, radius_km, minzoom, maxzoom, max_tiles):
    if not (-MAX_LAT <= lat <= MAX_LAT and -180 <= lon < 180):
        raise ValueError("Home coordinates must be valid Web Mercator coordinates")
    if radius_km <= 0 or not (0 <= minzoom <= maxzoom <= 16):
        raise ValueError("Radius and zoom levels are invalid")
    if max_tiles < 1:
        raise ValueError("max-tiles must be positive")
    uri = f"file:{mbtiles.resolve()}?mode=ro"
    conn = sqlite3.connect(uri, uri=True)
    try:
        meta = dict(conn.execute("SELECT name, value FROM metadata"))
        fmt = str(meta.get("format", "")).lower()
        if fmt in ("pbf", "mvt"):
            raise ValueError("Vector MBTiles cannot be decoded. Export raster PNG/JPEG MBTiles.")
        if fmt and fmt not in ("png", "jpg", "jpeg", "webp"):
            raise ValueError(f"Unsupported raster format {fmt}")
        queries = []
        total = 0
        for z in range(minzoom, maxzoom + 1):
            x0, x1, y0, y1 = coverage(lat, lon, radius_km, z)
            n = 1 << z
            tms_min = n - 1 - y1
            tms_max = n - 1 - y0
            params = (z, x0, x1, tms_min, tms_max)
            count = conn.execute(
                "SELECT count(*) FROM tiles WHERE zoom_level=? "
                "AND tile_column BETWEEN ? AND ? AND tile_row BETWEEN ? AND ?",
                params).fetchone()[0]
            total += count
            queries.append((z, params))
        if total == 0:
            raise ValueError("No raster tiles in MBTiles for the selected area and zoom levels")
        if total > max_tiles:
            raise ValueError(f"Area requires {total:,} tiles (limit {max_tiles:,}). "
                             "Reduce radius/zoom or increase --max-tiles knowingly.")

        output.mkdir(parents=True, exist_ok=True)
        for z, params in queries:
            n = 1 << z
            rows = conn.execute(
                "SELECT tile_column, tile_row, tile_data FROM tiles "
                "WHERE zoom_level=? AND tile_column BETWEEN ? AND ? "
                "AND tile_row BETWEEN ? AND ?",
                params)
            for x, tms_y, image_data in rows:
                xyz_y = n - 1 - tms_y
                out_file = output / "maps" / str(z) / str(x) / f"{xyz_y}.rgb565"
                out_file.parent.mkdir(parents=True, exist_ok=True)
                out_file.write_bytes(rgb565(image_data))
        attribution = str(meta.get("attribution", meta.get("name", "Map attribution: see source licence")))
        (output / "maps" / "attribution.txt").write_text(attribution + "\n", encoding="utf-8")
        manifest = {
            "format": "RGB565-LE-XYZ-256",
            "tileBytes": 131072,
            "minzoom": minzoom,
            "maxzoom": maxzoom,
            "center": [lat, lon],
            "radiusKm": radius_km,
            "tileCount": total,
            "source": {k: str(meta.get(k, "")) for k in ("name", "attribution", "format")},
        }
        (output / "maps" / "map-pack.json").write_text(json.dumps(manifest, indent=2) + "\n")
        return manifest
    finally:
        conn.close()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mbtiles", type=Path, required=True, help="Existing legally obtained raster .mbtiles")
    parser.add_argument("--output", type=Path, required=True, help="FAT32 SD card root / output folder")
    parser.add_argument("--lat", type=float, required=True)
    parser.add_argument("--lon", type=float, required=True)
    parser.add_argument("--radius-km", type=float, default=60.0)
    parser.add_argument("--minzoom", type=int, default=9)
    parser.add_argument("--maxzoom", type=int, default=12)
    parser.add_argument("--max-tiles", type=int, default=5000)
    args = parser.parse_args()
    try:
        info = export(args.mbtiles, args.output, args.lat, args.lon,
                      args.radius_km, args.minzoom, args.maxzoom, args.max_tiles)
    except (OSError, ValueError, sqlite3.Error) as error:
        parser.exit(2, f"Map pack error: {error}\n")
    print(f"Created {info['tileCount']:,} tiles under {args.output / 'maps'}")
    print("Copy the maps folder to the root of the CYD's FAT32 microSD card.")

if __name__ == "__main__":
    main()
