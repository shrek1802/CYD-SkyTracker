import io
import json
import sqlite3
import tempfile
import unittest
from pathlib import Path
import sys

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from pack_maps import export, coverage, rgb565

class MapPackTests(unittest.TestCase):
    def test_rgb565_red(self):
        im = Image.new("RGB", (256, 256), (255, 0, 0))
        buffer = io.BytesIO()
        im.save(buffer, format="PNG")
        raw = rgb565(buffer.getvalue())
        self.assertEqual(len(raw), 256 * 256 * 2)
        self.assertEqual(raw[:2], b"\x00\xf8")

    def test_mbtiles_tms_to_xyz(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            db = root / "map.mbtiles"
            im = Image.new("RGB", (256, 256), (255, 0, 0))
            buf = io.BytesIO()
            im.save(buf, format="PNG")
            con = sqlite3.connect(db)
            con.execute("CREATE TABLE metadata (name text, value text)")
            con.execute("CREATE TABLE tiles (zoom_level integer, tile_column integer, "
                        "tile_row integer, tile_data blob)")
            con.execute("INSERT INTO metadata VALUES ('format','png')")
            con.execute("INSERT INTO metadata VALUES ('attribution','Demo test map')")
            # XYZ tile (2,2,2) corresponds to TMS tile_row=1 at z=2.
            con.execute("INSERT INTO tiles VALUES (2,2,1,?)", (buf.getvalue(),))
            con.commit()
            con.close()
            result = export(db, root / "out", 0, 0, 20, 2, 2, 100)
            tile = root / "out/maps/2/2/2.rgb565"
            self.assertTrue(tile.exists())
            self.assertEqual(tile.stat().st_size, 131072)
            self.assertEqual(tile.read_bytes()[:2], b"\x00\xf8")
            self.assertEqual(result["tileCount"], 1)
            meta = json.loads((root / "out/maps/map-pack.json").read_text())
            self.assertEqual(meta["format"], "RGB565-LE-XYZ-256")
            self.assertEqual((root / "out/maps/attribution.txt").read_text(),
                             "Demo test map\n")

    def test_reject_wrong_shape(self):
        im = Image.new("RGB", (32, 32))
        buf = io.BytesIO()
        im.save(buf, format="PNG")
        with self.assertRaises(ValueError):
            rgb565(buf.getvalue())

    def test_coverage(self):
        x0, x1, y0, y1 = coverage(52.9, -1.2, 50, 10)
        self.assertLessEqual(x0, x1)
        self.assertLessEqual(y0, y1)
        with self.assertRaises(ValueError):
            coverage(0, 179.9, 3000, 12)

if __name__ == "__main__":
    unittest.main()
