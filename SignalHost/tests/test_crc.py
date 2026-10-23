"""CRC protocol vector tests (UTF-8)."""

from __future__ import annotations

import struct
import unittest
import zlib

from calibration_host.crc import calibration_import_crc32, calibration_import_payload


class CrcTests(unittest.TestCase):
    def test_known_vectors(self) -> None:
        vectors = (
            ((0, 0, 0, 0, 2), 0xA5DC5306),
            ((123456, -7890, 999000, 250, 12), 0x45D2EE29),
            ((-2147483648, 2147483647, -1000, 2147483647, 100000), 0xBD41AB5C),
            ((-414002, 2056898, 999981, 7439, 38), 0x6B0AA818),
        )
        for fields, expected in vectors:
            with self.subTest(fields=fields):
                self.assertEqual(calibration_import_crc32(*fields), expected)

    def test_payload_is_little_endian_int32_array(self) -> None:
        self.assertEqual(
            calibration_import_payload(0x01020304, -2, 3, 4, 5).hex(),
            "04030201feffffff030000000400000005000000",
        )

    def test_big_endian_counterexample_differs(self) -> None:
        fields = (123456, -7890, 999000, 250, 12)
        big_endian_crc = zlib.crc32(struct.pack(">5i", *fields)) & 0xFFFFFFFF
        self.assertNotEqual(calibration_import_crc32(*fields), big_endian_crc)

    def test_rejects_non_int32(self) -> None:
        with self.assertRaises(ValueError):
            calibration_import_crc32(2**31, 0, 0, 0, 2)
        with self.assertRaises(TypeError):
            calibration_import_crc32(True, 0, 0, 0, 2)


if __name__ == "__main__":
    unittest.main()
