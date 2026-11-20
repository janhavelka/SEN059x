#!/usr/bin/env python3
"""Offline unittest of CRC/reference RTU vectors, no external dependencies."""
from pathlib import Path
import csv
import importlib.util
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'examples' / 'python' / 'sen059x_probe.py'
spec = importlib.util.spec_from_file_location('sen059x_probe', SRC)
sensor = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sensor)


class TestVendorVectors(unittest.TestCase):
    def test_every_vector_crc(self):
        vectors = ROOT / 'docs' / 'reference' / 'protocol' / 'modbus_vectors.csv'
        with vectors.open(encoding='utf-8', newline='') as fp:
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


class BenchPort:
    """Nonblocking serial port delivering chunks at specified transaction times."""

    def __init__(self, arrivals=(), baudrate=115200):
        self.arrivals = list(arrivals)
        self.baudrate = baudrate
        self.now = 0.0
        self.buffer = bytearray()
        self.written = []
        self.write_time = 0.0
        self.write_count = 8

    @property
    def in_waiting(self):
        while self.arrivals and self.arrivals[0][0] <= self.now:
            self.buffer.extend(self.arrivals.pop(0)[1])
        return len(self.buffer)

    def read(self, count):
        size = min(count, self.in_waiting)
        chunk = bytes(self.buffer[:size])
        del self.buffer[:size]
        return chunk

    def write(self, data):
        self.now += self.write_time
        self.written.append(data)
        return self.write_count

    def sleep(self, duration):
        self.now += duration


class TestBenchTransactions(unittest.TestCase):
    def read(self, port, timeout=0.1):
        with patch.object(sensor.time, 'monotonic', lambda: port.now), patch.object(
                sensor.time, 'sleep', port.sleep):
            return sensor.read_register(port, 1, 0x0101, timeout)

    def test_fragmented_response_and_slow_baud_frame_gap(self):
        frame = bytes.fromhex('01 03 02 02 EF F8 A8')
        port = BenchPort([(0.012, frame[:4]), (0.016, frame[4:])], baudrate=4800)
        self.assertEqual(self.read(port), 751)
        self.assertEqual(port.written, [bytes.fromhex('01 03 01 01 00 01 D4 36')])
        self.assertGreaterEqual(port.now - 0.016, 3.5 * 10 / 4800)

    def test_trailing_byte_rejects_valid_prefix(self):
        frame = bytes.fromhex('01 03 02 02 EF F8 A8')
        port = BenchPort([(0.005, frame), (0.006, b'\xff')])
        with self.assertRaisesRegex(ValueError, 'Overlong'):
            self.read(port)

    def test_usb_split_at_fast_baud_uses_relative_timeout(self):
        frame = bytes.fromhex('01 03 02 02 EF F8 A8')
        port = BenchPort([(100.001, frame[:2]), (100.021, frame[2:])])
        port.now = 100.0
        with patch.object(sensor.time, 'monotonic', lambda: port.now), patch.object(
                sensor.time, 'sleep', port.sleep):
            self.assertEqual(sensor.read_response(port, 1, timeout_seconds=0.05), 751)

    def test_fragmented_exception_and_trailing_byte(self):
        frame = sensor.with_crc(bytes.fromhex('01 83 02'))
        for suffix, message in ((b'', 'exception code 0x02'), (b'\xff', 'Overlong')):
            port = BenchPort([(0.003, frame[:1]), (0.020, frame[1:]),
                              (0.021, suffix)])
            with self.subTest(suffix=suffix), self.assertRaisesRegex(ValueError, message):
                self.read(port)

    def test_validate_exception_crc_and_address_before_code(self):
        for frame, message in (
                (sensor.with_crc(bytes.fromhex('01 83 02')), 'exception code 0x02'),
                (sensor.with_crc(bytes.fromhex('02 83 02')), 'Wrong address'),
                (bytes.fromhex('01 83 02 00 00'), 'Bad CRC')):
            with self.subTest(frame=frame), self.assertRaisesRegex(ValueError, message):
                self.read(BenchPort([(0.005, frame)]))

    def test_malformed_normal_replies(self):
        for payload, message in (
                ('01 03 01 02 EF', 'Unexpected data length'),
                ('01 04 02 02 EF', 'Unexpected response header')):
            frame = sensor.with_crc(bytes.fromhex(payload))
            with self.subTest(payload=payload), self.assertRaisesRegex(ValueError, message):
                self.read(BenchPort([(0.005, frame)]))

    def test_truncated_response_waits_for_deadline(self):
        port = BenchPort([(0.003, bytes.fromhex('01 03 02 02 EF'))])
        with self.assertRaises(TimeoutError):
            self.read(port, timeout=0.030)
        self.assertAlmostEqual(port.now, 0.030)

    def test_timeout_includes_write_and_frame_completion(self):
        frame = bytes.fromhex('01 03 02 02 EF F8 A8')
        for at in (0.0095, 0.014):
            port = BenchPort([(at, frame)])
            port.write_time = 0.008
            with self.subTest(at=at), self.assertRaises(TimeoutError):
                self.read(port, timeout=0.01)

    def test_reject_stale_data_and_partial_write(self):
        port = BenchPort([(0, bytes.fromhex('01 03 02 02 EF F8 A8'))])
        with self.assertRaisesRegex(ValueError, 'before request'):
            self.read(port)
        self.assertFalse(port.written)
        port = BenchPort()
        port.write_count = 6
        with self.assertRaisesRegex(OSError, 'Incomplete request write'):
            self.read(port)


if __name__ == '__main__':
    unittest.main(verbosity=2)
