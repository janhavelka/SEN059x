"""Check the reference frames against DFRobot's archived examples in publication order."""

import csv
from pathlib import Path
import re
import unittest


REFERENCE = Path(__file__).resolve().parents[1] / "docs" / "reference"


class TestPublishedFrames(unittest.TestCase):
    def test_all_six_pairs_match_each_manual(self):
        with (REFERENCE / "protocol" / "modbus_vectors.csv").open(encoding="utf-8", newline="") as file:
            vectors = list(csv.DictReader(file))
        self.assertEqual(len(vectors), 6)
        for sku in ("SEN0591", "SEN0592"):
            for edition, host, slave in (("modbus_reference", "Host", "Slave"), ("wiki_CN", "主机", "从机")):
                page = REFERENCE / "originals" / "dfrobot" / f"{sku}_{edition}.html"
                # Match visible frame paragraphs only. The pages also embed serialized copies.
                paragraphs = re.findall(
                    rf"<p>({host}|{slave})\s*[:：]\s*([^<]+)</p>", page.read_text(encoding="utf-8")
                )
                with self.subTest(sku=sku, edition=edition):
                    self.assertEqual(len(paragraphs), 2 * len(vectors))
                    for index, row in enumerate(vectors):
                        for offset, (label, field) in enumerate(((host, "request_hex"), (slave, "response_hex"))):
                            role, source = paragraphs[2 * index + offset]
                            self.assertEqual(role, label)
                            frame = bytes.fromhex(source)
                            if edition == "modbus_reference" and row["case"] == "set_address_5":
                                self.assertEqual(len(frame), 7)
                                # Both English editions omit the high byte of the address value.
                                frame = frame[:4] + b"\x00" + frame[4:]
                            self.assertEqual(frame, bytes.fromhex(row[field]), (row["case"], field))
