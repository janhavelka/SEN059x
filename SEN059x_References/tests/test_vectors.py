#!/usr/bin/env python3
"""Offline unittest of CRC/reference RTU vectors, no external dependencies."""
from pathlib import Path
import csv
import importlib.util
import unittest

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'examples' / 'python' / 'sen059x_probe.py'
spec = importlib.util.spec_from_file_location('sen059x_probe', SRC)
sensor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sensor)


class TestVendorVectors(unittest.TestCase):
    def test_every_vector_crc(self):
        with (ROOT / 'protocol' / 'modbus_vectors.csv').open(encoding='utf-8', newline='') as fp:
            rows = list(csv.DictReader(fp))
        self.assertGreaterEqual(len(rows), 6)
        for row in rows:
            with self.subTest(case=row['case']):
                for field in ('request_hex', 'response_hex'):
                    frame = bytes.fromhex(row[field])
                    self.assertGreaterEqual(len(frame), 5)
                    crc = sensor.crc16_modbus(frame[:-2])
                    self.assertEqual((frame[-2], frame[-1]), (crc & 255, crc >> 8))

    def test_direct_read_requests(self):
        self.assertEqual(sensor.make_read(1, 0x0101), bytes.fromhex('01 03 01 01 00 01 D4 36'))
        self.assertEqual(sensor.make_read(1, 0x0102), bytes.fromhex('01 03 01 02 00 01 24 36'))

    def test_reject_broadcast(self):
        for val in (0, 248, 255):
            with self.assertRaises(ValueError):
                sensor.make_read(val, 0x0101)

    def test_vendor_address_typo(self):
        correct = bytes.fromhex('01 06 02 00 00 05 48 71')
        self.assertEqual(sensor.with_crc(correct[:-2]), correct)
        self.assertEqual(len(correct), 8)


if __name__ == '__main__':
    unittest.main(verbosity=2)
