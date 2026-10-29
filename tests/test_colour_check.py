import unittest
from tools.build_colour_check import COLOURS, HEIGHT, WIDTH, encode, make_image

class ColourCheckTests(unittest.TestCase):
    def test_shape_and_little_endian(self):
        raw=encode(make_image())
        self.assertEqual(len(raw),WIDTH*HEIGHT*2)
        self.assertEqual(raw[:2],b"\\x00\\xf8")
        self.assertEqual(raw[40*2:40*2+2],b"\\xe0\\x07")
        self.assertEqual(raw[80*2:80*2+2],b"\\x1f\\x00")
    def test_all_swatches_are_exact(self):
        raw=encode(make_image())
        for i,(r,g,b) in enumerate(COLOURS):
            offset=(10*WIDTH+i*40+10)*2
            value=raw[offset]|(raw[offset+1]<<8)
            expected=((r&248)<<8)|((g&252)<<3)|(b>>3)
            self.assertEqual(value,expected)

if __name__=="__main__": unittest.main()
