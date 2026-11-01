"""Offline safety regressions for photo type and Commons licensing."""
import unittest
from tools.build_aircraft_photo_pack import MODELS, PINNED, acceptable, good_filename


class PhotoSelectionTests(unittest.TestCase):
    def test_curated_replacements_exist(self):
        for code in ("A35K", "B739", "B753", "CRJ9", "GLF6"):
            self.assertIn(code, PINNED)
            self.assertIn(code, MODELS)

    def test_reject_restricted_licences(self):
        for value in ("CC BY-NC 4.0", "CC-BY-ND-4.0",
                      "CC BY-SA-NC 3.0", "All rights reserved"):
            self.assertFalse(acceptable(value), value)
        for value in ("CC0", "Public domain", "CC BY-SA 4.0", "CC BY 2.0"):
            self.assertTrue(acceptable(value), value)

    def test_reject_engine_and_cabin_not_airframe(self):
        self.assertFalse(good_filename(
            "Boeing 737-900 NG emergency oxygen masks 1.jpeg", "Boeing 737-900"))
        self.assertFalse(good_filename(
            "Rolls royce engine on boeing 757-300 - original.jpg", "Boeing 757-300"))
        self.assertFalse(good_filename(
            "RR BR725 engine on Gulfstream G650 (3).jpg", "Gulfstream G650"))

    def test_extended_aircraft_type_coverage(self):
        for code in ("R44", "EC35", "A139", "B77F", "A400", "C130"):
            self.assertIn(code, MODELS)


if __name__ == "__main__":
    unittest.main()
