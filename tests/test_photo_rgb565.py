import unittest
from PIL import Image

from tools.build_aircraft_photo_rgb565 import W, H, PIXEL_BYTES, FULL_W, FULL_H, encode_rgb565, prepare_full, decode_rgb565, frame_image


class PhotoRgb565Tests(unittest.TestCase):
    def test_red_green_blue_pixel_bytes_match_map_format(self):
        im = Image.new("RGB", (3, 1))
        im.putdata([(255, 0, 0), (0, 255, 0), (0, 0, 255)])
        self.assertEqual(encode_rgb565(im), bytes([
            0x00, 0xF8,   # red 0xF800, LITTLE endian
            0xE0, 0x07,   # green 0x07E0
            0x1F, 0x00,   # blue 0x001F
        ]))

    def test_dimensions_match_fixed_sd_reader(self):
        im = Image.new("RGB", (W, H), (10, 22, 35))
        raw = encode_rgb565(im)
        self.assertEqual(len(raw), PIXEL_BYTES)
        self.assertEqual(PIXEL_BYTES, 77824)

    def test_roundtrip_little_endian_not_bgr(self):
        im = Image.new("RGB", (3, 1))
        im.putdata([(255,0,0), (0,255,0), (0,0,255)])
        self.assertEqual(list(decode_rgb565(encode_rgb565(im),3,1).getdata()),
                         [(255,0,0), (0,255,0), (0,0,255)])

    def test_letterbox_keeps_whole_aircraft(self):
        im = Image.new("RGB",(400,100),(255,0,0))
        result = frame_image(im,(320,166))
        self.assertEqual(result.getpixel((160,83)),(255,0,0))
        self.assertEqual(result.getpixel((160,1)),(20,29,39))

    def test_fullscreen_dimensions(self):
        self.assertEqual(FULL_W, 320)
        self.assertEqual(FULL_H, 166)
        self.assertEqual(FULL_W * FULL_H * 2, 106240)

    def test_channel_round_trip_rgb565(self):
        im = Image.new("RGB", (1, 1), (130, 66, 244))
        v = encode_rgb565(im)
        pixel = v[0] | v[1] << 8
        self.assertEqual((pixel >> 11) & 31, 130 >> 3)
        self.assertEqual((pixel >> 5) & 63, 66 >> 2)
        self.assertEqual(pixel & 31, 244 >> 3)


if __name__ == "__main__":
    unittest.main()
