"""Compare the engineering register reference with the bundled DFRobot editions."""

import csv
from html.parser import HTMLParser
from pathlib import Path
import re
import unittest


REFERENCE = Path(__file__).resolve().parents[1] / "docs" / "reference"
SKUS = ("SEN0591", "SEN0592")
MANUALS = {"EN": "modbus_reference", "CN": "wiki_CN"}
INTERVAL_MS = re.compile(r"(\d+)\s*[~～-]\s*(\d+)\s*ms")


class TableRows(HTMLParser):
    def __init__(self):
        super().__init__()
        self.rows = []
        self.row = []
        self.cell = None

    def handle_starttag(self, tag, attrs):
        if tag == "tr":
            self.row = []
        elif tag in ("td", "th"):
            self.cell = []

    def handle_data(self, data):
        if self.cell is not None:
            self.cell.append(data)

    def handle_endtag(self, tag):
        if tag in ("td", "th") and self.cell is not None:
            self.row.append(" ".join("".join(self.cell).split()))
            self.cell = None
        elif tag == "tr":
            self.rows.append(self.row)


def snapshot_rows(sku, language):
    path = REFERENCE / "originals" / "dfrobot" / f"{sku}_{MANUALS[language]}.html"
    parser = TableRows()
    parser.feed(path.read_text(encoding="utf-8"))
    parser.close()
    return parser.rows


class TestRegisterReference(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with (REFERENCE / "protocol" / "registers.csv").open(encoding="utf-8", newline="") as source:
            cls.registers = list(csv.DictReader(source))
        cls.by_address = {row["address"]: row for row in cls.registers}
        cls.editions = {}
        for sku in SKUS:
            for language in MANUALS:
                cls.editions[sku, language] = {
                    row[1]: row for row in snapshot_rows(sku, language)
                    if len(row) == 5 and re.fullmatch(r"0x[0-9A-F]{4}", row[1])
                }

    def test_register_inventory_and_access(self):
        addresses = [row["address"] for row in self.registers]
        for edition, rows in self.editions.items():
            with self.subTest(edition=edition):
                self.assertCountEqual(addresses, rows.keys())
                for register in self.registers:
                    access = rows[register["address"]][0]
                    self.assertIn(access, ("Read-only", "只读", "Read/write", "Read and write", "读写"))
                    self.assertEqual(register["access"], "R" if access in ("Read-only", "只读") else "RW")

    def test_measurement_time_by_edition(self):
        for register in self.registers:
            if register["access"] != "R":
                continue
            for sku in SKUS:
                with self.subTest(sku=sku, address=register["address"]):
                    intervals = {}
                    for language in MANUALS:
                        description = self.editions[sku, language][register["address"]][-1]
                        (low, high), = INTERVAL_MS.findall(description)
                        intervals[language] = f"{low}–{high} ms"
                    expected = intervals["EN"]
                    if expected != intervals["CN"]:
                        expected = f"{expected} (EN), {intervals['CN']} (CN)"
                    self.assertEqual(register[sku.lower()], expected)

    def test_baud_codes_from_chinese_editions(self):
        baud = self.by_address["0x0201"]
        codes = re.findall(r"(\d+)=(\d+)", baud["remarks"])
        for sku in SKUS:
            with self.subTest(sku=sku):
                description = self.editions[sku, "CN"]["0x0201"][-1]
                source = re.findall(r"0x([0-9A-Fa-f]{4}):\s*(\d+)", description)
                self.assertEqual(len(source), 8)
                self.assertEqual(codes, [(str(int(code, 16)), rate) for code, rate in source])
                default_bps, = re.findall(r"默认\s*(\d+)", description)
                default_code, = (code for code, rate in codes if rate == default_bps)
                self.assertIn(f"Default {default_code}.", baud["remarks"])

    def test_angle_levels_from_chinese_editions(self):
        angle = self.by_address["0x0208"]
        sen0591 = self.editions["SEN0591", "CN"]["0x0208"][-1]
        angles = re.findall(r"角度约\s*(\d+)°", sen0591)
        self.assertEqual(len(angles), 4)
        self.assertEqual(angle["sen0591"], "/".join(angles) + "°")

        sen0592 = self.editions["SEN0592", "CN"]["0x0208"][-1]
        groups = []
        for source_label, label in (("单角度", "single"), ("水平角度", "dual H"), ("垂直角度", "dual V")):
            angles = re.findall(source_label + r"约\s*(\d+)°", sen0592)
            self.assertEqual(len(angles), 4, label)
            groups.append(label + " " + "/".join(angles) + "°")
        self.assertEqual(angle["sen0592"], ", ".join(groups))

    def test_configuration_ranges_and_defaults(self):
        for sku in SKUS:
            with self.subTest(sku=sku):
                rows = self.editions[sku, "CN"]
                model = sku.lower()
                address = self.by_address["0x0200"]
                lower, upper = re.search(r"(0x\w+)～(0x\w+)", rows["0x0200"][-1]).groups()
                self.assertEqual(address[model], f"{lower}..{upper}")
                default_id, = re.findall(r"默认\s*(0x\w+)", rows["0x0200"][-1])
                self.assertIn(f"default {default_id}", address["remarks"])

                polarity = self.by_address["0x0205"]
                switches = re.findall(r"0x([0-9A-F]+)：([^；]+)", rows["0x0205"][-1])
                switch_codes = [int(code, 16) for code, _ in switches]
                self.assertEqual(polarity[model], f"{min(switch_codes)}..{max(switch_codes)}")
                default_polarity, = (int(code, 16) for code, text in switches if "(默认)" in text)
                self.assertIn(f"for {default_polarity} (default)", polarity["remarks"])

                lower, upper = re.search(r"(\d+)～(\d+)mm", rows["0x0206"][-1]).groups()
                self.assertEqual(self.by_address["0x0206"][model], f"{lower}..{upper}")
                noise_min, noise_max = re.search(r"(\d+)～(\d+)\s*级", rows["0x021A"][-1]).groups()
                self.assertEqual(self.by_address["0x021A"][model], f"{noise_min}..{noise_max}")
                for register in ("0x0208", "0x021A"):
                    default, = re.findall(r"默认[第为]\s*(\d+)", rows[register][-1])
                    self.assertIn(f"Default {default}.", self.by_address[register]["remarks"])

                ranges = self.by_address["0x021F"]
                levels = re.findall(r"(\d)-约\s*(\d+)cm", rows["0x021F"][-1])
                distances = "/".join(distance for _, distance in levels)
                self.assertEqual(ranges[model], f"{levels[0][0]}..{levels[-1][0]} (~{distances} cm)")
                default_range, = re.findall(r"默认为\s*(\d+)", rows["0x021F"][-1])
                self.assertIn(f"{sku} default {default_range}.", ranges["remarks"])

    def test_sen0591_range_level_times(self):
        guide = (REFERENCE / "measurement-and-configuration.md").read_text(encoding="utf-8")
        table = re.findall(r"^\| (\d+) \| (\d+)–(\d+) \| (\d+)–(\d+) \|$", guide, re.MULTILINE)
        for language in MANUALS:
            with self.subTest(language=language):
                description = self.editions["SEN0591", language]["0x021F"][-1]
                # Each level starts with its distance in cm, followed by the two times.
                levels = re.findall(r"(\d)\s*-\s*(?:about|约)\s*\d+\s*cm([^;；]+)", description)
                expected = []
                for level, details in levels:
                    realtime, processed = INTERVAL_MS.findall(details)
                    expected.append((level, *realtime, *processed))
                self.assertEqual(len(expected), 4)
                self.assertEqual(table, expected)

    def test_single_packet_durations_and_8n1_wire_times(self):
        guide = (REFERENCE / "protocol" / "Modbus_RTU_notes.md").read_text(encoding="utf-8")
        values = {}
        for line in guide.splitlines():
            fields = [cell.strip() for cell in line.strip("|").split("|")]
            if len(fields) == 5 and fields[0].isdigit():
                baud, published, *calculated = fields
                values[baud] = published
                # 1 start, 8 data and 1 stop bit per byte. Convert seconds to ms.
                expected = [f"{size * 10 * 1000 / int(baud):.3f}" for size in (8, 7, 5)]
                self.assertEqual(calculated, expected, baud)
        for sku, language in self.editions:
            with self.subTest(sku=sku, language=language):
                source = {}
                for row in snapshot_rows(sku, language):
                    if len(row) == 3 and row[0].isdigit() and row[2].endswith("ms"):
                        source[row[1]] = row[2][:-2].strip()
                self.assertEqual(len(source), 8)
                self.assertEqual(values, source)
