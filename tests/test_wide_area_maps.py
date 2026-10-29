import sys
from pathlib import Path
import tempfile
import unittest

from PIL import Image

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from build_wide_area_maps import bbox_tiles, generate_level, draw_land, draw_cities, OCEAN, PALETTES, project


class WideAreaMapTests(unittest.TestCase):
    def test_zoom_5_through_8_is_bounded(self):
        for z in (5,6,7,8):
            x0,x1,y0,y1=bbox_tiles(52.9548,-1.1581,z,{5:950,6:700,7:480,8:300}[z])
            self.assertLess(x0,x1)
            self.assertLess(y0,y1)
            self.assertLess((x1-x0+1)*(y1-y0+1),200)

    def test_land_and_cities_render_without_internet(self):
        z=8
        lon=-1.1581
        lat=52.9548
        u,v=project(lat,lon,z)
        x,y=int(u//256),int(v//256)
        features=[{
            "geometry":{
                "type":"Polygon",
                "coordinates":[[
                    [lon-0.2,lat-0.2], [lon+0.2,lat-0.2],
                    [lon+0.2,lat+0.2], [lon-0.2,lat+0.2],
                    [lon-0.2,lat-0.2]
                ]]
            }
        }]
        place=[{"geometry":{"type":"Point","coordinates":[lon,lat]},
                "properties":{"NAME":"TEST CITY","SCALERANK":1}}]
        from PIL import ImageDraw
        im=Image.new("RGB",(256,256),OCEAN["night"])
        draws={(x,y):ImageDraw.Draw(im)}
        draw_land(draws,features,z,"night")
        self.assertNotEqual(im.getpixel((int(u-x*256),int(v-y*256))),
                            Image.new("RGB",(1,1),OCEAN["night"]).getpixel((0,0)))
        draw_cities(draws,place,z,"night")
        self.assertEqual(im.size,(256,256))

    def test_day_and_night_tile_data_encoded_with_same_layout(self):
        lat,lon=52.9548,-1.1581
        land=[{"geometry":{"type":"Polygon","coordinates":[[
            [lon-1,lat-1],[lon+1,lat-1],[lon+1,lat+1],
            [lon-1,lat+1],[lon-1,lat-1]
        ]]}}]
        cities=[]
        with tempfile.TemporaryDirectory() as temp:
            base=Path(temp)
            count=generate_level(land,cities,lat,lon,5,base)
            self.assertGreater(count,0)
            day=sorted((base/"maps"/"5").rglob("*.rgb565"))
            self.assertEqual(len(day),count)
            for file in day:
                night=base/"maps"/"night"/"5"/file.parent.name/file.name
                self.assertTrue(night.is_file())
                self.assertEqual(file.stat().st_size,131072)
                self.assertEqual(night.stat().st_size,131072)
            self.assertNotEqual(day[0].read_bytes(),(base/"maps"/"night"/"5"/day[0].parent.name/day[0].name).read_bytes())


if __name__=="__main__":
    unittest.main()
