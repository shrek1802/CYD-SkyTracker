#!/usr/bin/env python3
"""Self-render Geofabrik OSM vector data to SkyTracker RGB565-LE map tiles."""
import argparse, json, math, urllib.request, zipfile
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import osmium

BASE_URL = 'https://download.geofabrik.de/europe/united-kingdom/england/'
# Neighbouring county extracts prevent blank rectangles beyond Nottinghamshire
# at low zoom, without needing an England-wide 1.6GB source.
COUNTIES = ('nottinghamshire','derbyshire','leicestershire',
            'lincolnshire','south-yorkshire','rutland')
RANK = {'motorway':0,'motorway_link':0,'trunk':1,'trunk_link':1,'primary':2,
        'primary_link':2,'secondary':3,'secondary_link':3,'tertiary':4,
        'tertiary_link':4,'residential':5,'unclassified':5,'living_street':5,
        'service':6,'pedestrian':6,'footway':7,'cycleway':7,'path':7,'track':7}
PLACE = {'city':0,'town':1,'village':2,'suburb':3,'hamlet':4,'neighbourhood':4}
# Use real OSM vector geometries, independently rasterized offline — never scrape
# a commercial or OpenStreetMap public tile server.
LAND_COLOURS = {
    'park':'#dce8d8', 'forest':'#cbdac6', 'grass':'#e0ecdd',
    'farmland':'#efece1', 'residential':'#edeff0',
    'industrial':'#e2e6e9', 'commercial':'#ece9e4',
    'water':'#b6d3df', 'building':'#d3d8dc'
}
LOW_ZOOM_RADIUS = {9:110,10:85,11:65,12:45}

def project(lat,lon,z):
    lat=max(-85.05112878,min(85.05112878,lat))
    n=256*(1<<z)
    return (lon+180)/360*n,(1-math.asinh(math.tan(math.radians(lat)))/math.pi)/2*n

class Extract(osmium.SimpleHandler):
    def __init__(self,bounds):
        super().__init__()
        self.lines=[]; self.places=[]; self.areas=[]; self.bounds=bounds
    def node(self,n):
        p=n.tags.get('place'); name=n.tags.get('name')
        if p in PLACE and name and n.location.valid():
            a,b=n.location.lat,n.location.lon
            if self.bounds[0]<=a<=self.bounds[2] and self.bounds[1]<=b<=self.bounds[3]:
                self.places.append((PLACE[p],str(name),a,b))
    def way(self,w):
        road=w.tags.get('highway')
        water=w.tags.get('waterway')
        railway=w.tags.get('railway')
        land=w.tags.get('landuse')
        natural=w.tags.get('natural')
        leisure=w.tags.get('leisure')
        building=w.tags.get('building')
        try:
            pts=[(n.location.lat,n.location.lon) for n in w.nodes if n.location.valid()]
        except Exception:
            return
        if len(pts)<2:return
        a,b,c,d=self.bounds
        if max(x for x,_ in pts)<a or min(x for x,_ in pts)>c or max(x for _,x in pts)<b or min(x for _,x in pts)>d:return

        # Land-use/water polygons establish a proper cartographic base layer.
        # Draw broad areas first, buildings last at detailed zoom only.
        if len(pts)>=4 and pts[0]==pts[-1]:
            kind=None
            if natural=='water' or land in ('reservoir','basin'):kind='water'
            elif leisure in ('park','garden','nature_reserve','golf_course'):kind='park'
            elif natural in ('wood','scrub','heath'):kind='forest'
            elif natural in ('grassland','meadow') or land in ('grass','meadow','recreation_ground'):kind='grass'
            elif land in ('forest','wood'):kind='forest'
            elif land in ('farmland','farmyard','orchard'):kind='farmland'
            elif land in ('residential','industrial','commercial','retail'):kind='commercial' if land=='retail' else land
            elif building:kind='building'
            if kind:
                self.areas.append((kind,pts))
        if road in RANK:
            self.lines.append(('road',RANK[road],pts))
        elif water in ('river','stream','canal','drain'):
            self.lines.append(('water',{'river':0,'canal':1,'stream':2,'drain':3}[water],pts))
        elif railway in ('rail','light_rail','tram'):
            self.lines.append(('rail',0,pts))

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
    # Low zoom needs broader area coverage than the high-zoom 30 km radius;
    # this fixes missing z9/x255/y166 and /167 tile errors on the actual CYD.
    radius=max(radius,LOW_ZOOM_RADIUS.get(z,0))
    cx,cy=project(lat,lon,z)
    px=radius*(256*(1<<z))/(40075.016686*math.cos(math.radians(lat)))
    x0,x1=math.floor((cx-px)/256),math.floor((cx+px)/256)
    y0,y1=math.floor((cy-px)/256),math.floor((cy+px)/256)
    count=(x1-x0+1)*(y1-y0+1)
    if count>1600:raise RuntimeError('Too many tiles; reduce radius or maxzoom')
    print(f'Zoom {z}: {count} tiles within {radius}km',flush=True)
    images={(x,y):Image.new('RGB',(256,256),'#f0f2ed') for x in range(x0,x1+1) for y in range(y0,y1+1)}
    draws={key:ImageDraw.Draw(img) for key,img in images.items()}

    def apply_to_tiles(pts,call):
        screen=[project(a,b,z) for a,b in pts]
        a=max(x0,math.floor((min(p[0] for p in screen)-7)/256))
        b=min(x1,math.floor((max(p[0] for p in screen)+7)/256))
        c=max(y0,math.floor((min(p[1] for p in screen)-7)/256))
        d=min(y1,math.floor((max(p[1] for p in screen)+7)/256))
        for x in range(a,b+1):
            for y in range(c,d+1):
                local=[(round(u-256*x),round(v-256*y)) for u,v in screen]
                call(draws[(x,y)],local)

    # Soft green parks/forests, blue waterways and distinct landuse/buildings.
    priority={'residential':0,'commercial':1,'industrial':2,'farmland':3,
              'forest':4,'grass':5,'park':6,'water':7,'building':8}
    for kind,pts in sorted(source.areas,key=lambda p:priority[p[0]]):
        if kind=='building' and z<13:continue
        if kind in ('commercial','industrial','residential') and z<11:continue
        if kind in ('grass','farmland') and z<10:continue
        fill=LAND_COLOURS[kind]
        apply_to_tiles(pts,lambda dr,line: dr.polygon(line,fill=fill))

    # Water/rail/roads overlay their landuse base with clear hierarchy.
    paints=['#e9bd91','#eecaa6','#ecd8ba','#eee2cf','#f5eee0','#f8f7f4','#e3e7e5','#dae0dd']
    def line_width(rank):
        return max(1,(6-rank)+(z-12))
    for layer,rank,way in sorted(source.lines,key=lambda e: (0 if e[0]=='water' else 1 if e[0]=='rail' else 2, -e[1])):
        if layer=='road' and ((z<=9 and rank>2) or (z==10 and rank>4) or (z==11 and rank>5) or (z==12 and rank>6) or (z>=13 and rank>6)):
            continue
        if layer=='water':
            # Tiny streams and drains at zoom 9 create blue spaghetti.
            # Preserve navigable rivers/canals at broad zoom; add detail later.
            if (z<=10 and rank>1) or (z==11 and rank>2) or (z==12 and rank>2):
                continue
            apply_to_tiles(way,lambda dr,line:dr.line(line,fill='#91b8cd',
                                    width=(4 if rank==0 else 2) if z>=12 else 2,joint='curve'))
        elif layer=='rail' and z>=11:
            apply_to_tiles(way,lambda dr,line:dr.line(line,fill='#a8adb3',width=2 if z>=13 else 1,joint='curve'))
        elif layer=='road':
            w=line_width(rank)
            def draw_road(dr,line):
                dr.line(line,fill='#bdc5c7',width=w+2,joint='curve')
                dr.line(line,fill=paints[rank],width=w,joint='curve')
            apply_to_tiles(way,draw_road)

    # Dark legible town names with a light text halo.
    used={key:[] for key in images}
    for rank,name,plat,plon in sorted(source.places):
        if rank>max(1,z-9):continue
        u,v=project(plat,plon,z);x=int(u//256);y=int(v//256)
        if (x,y) not in draws:continue
        dr=draws[(x,y)];f=font(14 if rank<2 else 11,rank<2)
        bb=dr.textbbox((0,0),name,font=f);w=bb[2]-bb[0];h=bb[3]-bb[1]
        tx=round(u-x*256-w/2);ty=round(v-y*256-h/2)
        box=(tx-3,ty-3,tx+w+3,ty+h+3)
        if any(not(box[2]<other[0] or box[0]>other[2] or box[3]<other[1] or box[1]>other[3])
               for other in used[(x,y)]):continue
        dr.text((tx,ty),name,font=f,fill='#213c48',stroke_width=2,stroke_fill='#f5f7f2')
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
    dlat=(max(args.radius_km,110)+15)/111.2
    dlon=dlat/math.cos(math.radians(args.lat))
    handler=Extract((args.lat-dlat,args.lon-dlon,args.lat+dlat,args.lon+dlon))
    counties=('nottinghamshire',) if args.source is not None else COUNTIES
    for county in counties:
        source=args.source or args.output/(county+'-latest.osm.pbf')
        if not source.is_file():
            url=BASE_URL+county+'-latest.osm.pbf'
            print('Downloading permitted OSM vector extract:',county,flush=True)
            req=urllib.request.Request(url,headers={
                'User-Agent':'CYD-SkyTracker offline OSM tile builder (github.com/shrek1802/CYD-SkyTracker)'})
            with urllib.request.urlopen(req,timeout=120) as response,source.open('wb') as dest:
                import shutil
                shutil.copyfileobj(response,dest)
        print('Parsing '+county+' OSM roads/areas...',flush=True)
        handler.apply_file(str(source),locations=True)
    print(len(handler.lines),'roads/rivers/rails;',len(handler.areas),'land/water areas;',
          len(handler.places),'named places',flush=True)
    if not handler.lines:raise RuntimeError('No road data found in requested area')
    maps=args.output/'maps';maps.mkdir(parents=True,exist_ok=True)
    counts={}
    for z in range(args.minzoom,args.maxzoom+1):counts[z]=level(handler,args.lat,args.lon,args.radius_km,z,maps)
    (maps/'attribution.txt').write_text('(c) OpenStreetMap contributors | ODbL 1.0\n')
    (maps/'map-pack.json').write_text(json.dumps({'format':'CYD-SkyTracker-rgb565-le-v1','map_centre':[args.lat,args.lon],
        'radius_km':args.radius_km,'low_zoom_radius_km':LOW_ZOOM_RADIUS,
        'zoom_tile_counts':counts,'cartography':'muted-aviation-v2','data_source':BASE_URL,'source_counties':counties,'licence':'OpenStreetMap ODbL 1.0'},indent=2))
    out=args.output/'SkyTracker-Nottinghamshire-Offline-Maps.zip'
    with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as zf:
        for p in maps.rglob('*'):
            if p.is_file():zf.write(p,p.relative_to(args.output))
    print('DONE:',out,'— copy /maps to SD root.',flush=True)

if __name__=='__main__':main()
