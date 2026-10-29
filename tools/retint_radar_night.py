#!/usr/bin/env python3
"""Post-render cool-navy SkyTracker night tiles from an offline OSM/NE pack.

For reproducible styling, this never uses a raster-provider server, does
not modify daytime tiles, and leaves licence attribution intact.
Requires numpy. Run after generating the day/night vector-derived pack.
"""
import argparse, json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import numpy as np
# RGB colours from the SkyTracker independent dark v4 palette => cool-navy v5.
COLOURS={
 "#101d2a":"#101a27","#234138":"#204338","#1b3932":"#17362f",
 "#2a4038":"#254138","#25342e":"#23362f","#283a48":"#233747",
 "#303e4b":"#2b3c4c","#354550":"#304553","#164b68":"#164b68",
 "#3a4958":"#34485a","#2b4a54":"#25434f",
 "#edbe77":"#8fcee5","#dfb377":"#78b1d0",
 "#c5a683":"#bac4c6","#aeb5c4":"#899db3",
 "#8c9aab":"#6b8298","#70899e":"#465c72",
 "#4f6a7c":"#364d60","#3d5265":"#2e4050",
 "#1a2939":"#172838","#347d9a":"#307b9b",
 "#63798a":"#5a7187","#b6c8cb":"#b8c9d3",
 "#78929e":"#668d9d","#e9f2f7":"#dbeaf2",
 "#0f3249":"#0e304b","#4d90a7":"#4b819c"
}
def rgb565(color):
    r,g,b=bytes.fromhex(color.lstrip("#"))
    return (r&248)<<8 | (g&252)<<3 | b>>3
def apply_to_zip(source:Path,output:Path):
    lookup=np.arange(65536,dtype=np.uint16)
    for old,new in COLOURS.items():lookup[rgb565(old)]=rgb565(new)
    counts={}
    changed=0
    with ZipFile(source) as incoming, ZipFile(output,"w",ZIP_DEFLATED,compresslevel=6) as result:
        if incoming.testzip() is not None:raise ValueError("Corrupt input zip")
        for name in incoming.namelist():
            if not name.startswith("maps/night/") or not name.endswith(".rgb565"):continue
            pixels=np.frombuffer(incoming.read(name),dtype="<u2")
            if len(pixels)!=65536:raise ValueError("Tile must be 256x256 RGB565: "+name)
            modified=lookup[pixels]
            changed+=int(np.count_nonzero(pixels!=modified))
            result.writestr(name,modified.astype("<u2").tobytes())
            level=name.split("/")[2]
            counts[level]=counts.get(level,0)+1
        if not counts:raise ValueError("No night map tiles found")
        meta={"format":"CYD-SkyTracker-rgb565-le-v1",
              "style":"radar-pro-cool-navy-v5",
              "night_zoom_counts":counts,
              "recoloured_pixels":changed,
              "sources":"Geofabrik OSM vectors and Natural Earth",
              "note":"Keeps the exact input tile geometry and credit obligations"}
        result.writestr("maps/radar-pro-night-pack.json",json.dumps(meta,indent=2)+"\n")
        result.writestr("maps/RADAR-PRO-NIGHT-CREDITS.txt",
            "(c) OpenStreetMap contributors / ODbL 1.0 for z9-z13. "+
            "Natural Earth public domain for z5-z8. Keep original maps/attribution.txt.\n")
    return counts,changed
if __name__=="__main__":
    ap=argparse.ArgumentParser()
    ap.add_argument("--input",type=Path,required=True,
        help="Already converted day/night RGB565 ZIP with maps/night/* tiles")
    ap.add_argument("--output",type=Path,required=True)
    args=ap.parse_args()
    counts,changed=apply_to_zip(args.input,args.output)
    print("Done:",args.output,"levels",counts,"changed pixels",changed)
