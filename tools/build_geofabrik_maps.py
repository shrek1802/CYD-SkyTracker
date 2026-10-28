#!/usr/bin/env python3
"""Self-render Geofabrik OSM vector data to SkyTracker RGB565-LE map tiles."""
import argparse, json, math, urllib.request, zipfile
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import osmium

URL = 'https://download.geofabrik.de/europe/united-kingdom/england/nottinghamshire-latest.osm.pbf'
RANK = {'motorway':0,'motorway_link':0,'trunk':1,'trunk_link':1,'primary':2,
        'primary_link':2,'secondary':3,'secondary_link':3,'tertiary':4,
        'tertiary_link':4,'residential':5,'unclassified':5,'living_street':5,
        'service':6,'pedestrian':6,'footway':7,'cycleway':7,'path':7,'track':7}
PLACE = {'city':0,'town':1,'village':2,'suburb':3,'hamlet':4,'neighbourhood':4}

def project(lat,lon,z):
    lat=max(-85.05112878,min(85.05112878,lat))
    n=256*(1<<z)
    return (lon+180)/360*n,(1-math.asinh(math.tan(math.radians(lat)))/math.pi)/2*n

class Extract(osmium.SimpleHandler):
    def __init__(self,bounds):
        super().__init__();self.lines=[];self.places=[];self.bounds=bounds
    def node(self,n):
        p=n.tags.get('place'); name=n.tags.get('name')
        if p in PLACE and name and n.location.valid():
            a,b=n.location.lat,n.location.lon
            if self.bounds[0]<=a<=self.bounds[2] and self.bounds[1]<=b<=self.bounds[3]:
                self.places.append((PLACE[p],str(name),a,b))
    def way(self,w):
        road=w.tags.get('highway'); water=w.tags.get('waterway')
        if road not in RANK and water not in ('river','stream','canal','drain'):return
        try: pts=[(n.location.lat,n.location.lon) for n in w.nodes if n.location.valid()]
        except Exception:return
        if len(pts)<2:return
        a,b,c,d=self.bounds
        if max(x for x,_ in pts)<a or min(x for x,_ in pts)>c or max(x for _,x in pts)<b or min(x for _,x in pts)>d:return
        self.lines.append((0 if road is None else 1,RANK.get(road,0),pts))

def font(sz,bold=False):
    p='/usr/share/fonts/truetype/dejavu/DejaVuSans'+('-Bold' if bold else '')+'.ttf'
    try:return ImageFont.truetype(p,sz)
    except OSError:return ImageFont.load_default()

def as565(im):
    rgb=im.tobytes();out=bytearray(131072); j=0
    for i in range(0,len(rgb),3):
        r,g,b=rgb[i:i+3];v=((r&248)<<8)|((g&252)<<3)|(b>>3)
        out[j]=v&255;out[j+1]=v>>8;j+=2
    return out

def level(source,lat,lon,radius,z,output):
    cx,cy=project(lat,lon,z)
    px=radius*(256*(1<<z))/(40075.016686*math.cos(math.radians(lat)))
    x0,x1=math.floor((cx-px)/256),math.floor((cx+px)/256)
    y0,y1=math.floor((cy-px)/256),math.floor((cy+px)/256)
    count=(x1-x0+1)*(y1-y0+1)
    if count>1600:raise RuntimeError('Too many tiles; reduce radius or maxzoom')
    print(f'Zoom {z}: {count} geographic tiles',flush=True)
    images={(x,y):Image.new('RGB',(256,256),'#e9eee8') for x in range(x0,x1+1) for y in range(y0,y1+1)}
    draws={key:ImageDraw.Draw(img) for key,img in images.items()}
    paints=['#f0a15b','#f7bd75','#f8cf8c','#f8e8ae','#fff4d3','#fffdf8','#e1e4df','#b7bab4']
    for layer,rank,way in sorted(source.lines,key=lambda e:(e[0],-e[1])):
        if layer==1 and (z<=9 and rank>2 or z==10 and rank>4 or z==11 and rank>5 or z==12 and rank>6):continue
        width=(3 if rank==0 else 2) if layer==0 else max(1,6-rank+(z-12))
        pts=[project(a,b,z) for a,b in way]
        a=max(x0,math.floor((min(p[0] for p in pts)-5)/256));b=min(x1,math.floor((max(p[0] for p in pts)+5)/256))
        c=max(y0,math.floor((min(p[1] for p in pts)-5)/256));d=min(y1,math.floor((max(p[1] for p in pts)+5)/256))
        for x in range(a,b+1):
            for y in range(c,d+1):
                dr=draws[(x,y)];line=[(round(u-256*x),round(v-256*y)) for u,v in pts]
                if layer==0:dr.line(line,fill='#81bfe0',width=width+2,joint='curve')
                else:
                    dr.line(line,fill='#b6bfba',width=width+2,joint='curve')
                    dr.line(line,fill=paints[rank],width=width,joint='curve')
    used={key:[] for key in images}
    for rank,name,plat,plon in sorted(source.places):
        if rank>max(1,z-9):continue
        u,v=project(plat,plon,z);x=int(u//256);y=int(v//256)
        if (x,y) not in draws:continue
        dr=draws[(x,y)];f=font(13 if rank<2 else 11,rank<2)
        bb=dr.textbbox((0,0),name,font=f);w=bb[2]-bb[0];h=bb[3]-bb[1]
        tx=round(u-x*256-w/2);ty=round(v-y*256-h/2)
        box=(tx-3,ty-3,tx+w+3,ty+h+3)
        if any(not(box[2]<b[0] or box[0]>b[2] or box[3]<b[1] or box[1]>b[3]) for b in used[(x,y)]):continue
        dr.text((tx,ty),name,font=f,fill='#17374a',stroke_width=2,stroke_fill='#f8fcf6')
        used[(x,y)].append(box)
    for (x,y),img in images.items():
        dest=output/str(z)/str(x%(1<<z));dest.mkdir(parents=True,exist_ok=True)
        (dest/f'{y}.rgb565').write_bytes(as565(img))
    return count

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--lat',type=float,default=52.9548)
    ap.add_argument('--lon',type=float,default=-1.1581)
    ap.add_argument('--radius-km',type=float,default=30)
    ap.add_argument('--minzoom',type=int,default=9)
    ap.add_argument('--maxzoom',type=int,default=13)
    ap.add_argument('--source',type=Path,default=None)
    ap.add_argument('--output',type=Path,default=Path('SkyTracker-SD'))
    args=ap.parse_args()
    if not(-85<args.lat<85 and -180<args.lon<180 and 1<=args.radius_km<=80 and 3<=args.minzoom<=args.maxzoom<=15):ap.error('Invalid map bounds or zoom')
    args.output.mkdir(parents=True,exist_ok=True)
    source=args.source or args.output/'nottinghamshire-latest.osm.pbf'
    if not source.is_file():
        print('Downloading Nottinghamshire OSM extract from Geofabrik...',flush=True)
        req=urllib.request.Request(URL,headers={'User-Agent':'CYD-SkyTracker offline map builder (https://github.com/shrek1802/CYD-SkyTracker)'})
        with urllib.request.urlopen(req,timeout=90) as response,source.open('wb') as f:
            import shutil;shutil.copyfileobj(response,f)
    dlat=(args.radius_km+15)/111.2;dlon=dlat/math.cos(math.radians(args.lat))
    handler=Extract((args.lat-dlat,args.lon-dlon,args.lat+dlat,args.lon+dlon))
    print('Reading real road and place data...',flush=True)
    handler.apply_file(str(source),locations=True)
    print(len(handler.lines),'features;',len(handler.places),'named places',flush=True)
    if not handler.lines:raise RuntimeError('No road data found in requested area')
    maps=args.output/'maps';maps.mkdir(parents=True,exist_ok=True)
    counts={}
    for z in range(args.minzoom,args.maxzoom+1):counts[z]=level(handler,args.lat,args.lon,args.radius_km,z,maps)
    (maps/'attribution.txt').write_text('(c) OpenStreetMap contributors | ODbL 1.0\n')
    (maps/'map-pack.json').write_text(json.dumps({'format':'CYD-SkyTracker-rgb565-le-v1','map_centre':[args.lat,args.lon],
        'radius_km':args.radius_km,'zoom_tile_counts':counts,'data_source':URL,'licence':'OpenStreetMap ODbL 1.0'},indent=2))
    out=args.output/'SkyTracker-Nottinghamshire-Offline-Maps.zip'
    with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as zf:
        for p in maps.rglob('*'):
            if p.is_file():zf.write(p,p.relative_to(args.output))
    print('DONE:',out,'— copy /maps to SD root.',flush=True)

if __name__=='__main__':main()
