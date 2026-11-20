"""Guard the implementation plan against missing capabilities and skipped reviews."""

from pathlib import Path
from shutil import copytree
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

from scripts import check_promptset as checker


class PromptsetTests(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.prompts = Path(temporary.name) / "prompts"
        copytree(checker.PROMPTS, self.prompts)
        prompt_path = patch.object(checker, "PROMPTS", self.prompts)
        prompt_path.start()
        self.addCleanup(prompt_path.stop)

    def test_all_documented_operations_and_audits(self):
        self.assertEqual(checker.check_sequence(), 15)
        self.assertEqual(checker.check_operations(), (12, 20))

    def test_missing_conditional_write_or_wrong_expected_value(self):
        operations = self.prompts / "OPERATIONS.csv"
        original = operations.read_text(encoding="utf-8")
        line = next(row for row in original.splitlines(keepends=True) if row.startswith("0x0205,FC06,"))
        cases = [
            ("missing operation", ""),
            ("duplicate operation", line + line),
            ("invalid polarity", line.replace("polarity 0", "polarity 99").replace("0x0000", "0x00FF")),
            ("wrong wire value", line.replace("0x0000", "0x0001")),
        ]
        for name, replacement in cases:
            with self.subTest(name=name):
                operations.write_text(original.replace(line, replacement), encoding="utf-8")
                with self.assertRaises(ValueError):
                    checker.check_operations()

    def test_required_stage_review_cannot_be_removed(self):
        stage = self.prompts / "10-arduino-example.md"
        stage.write_text(stage.read_text(encoding="utf-8").replace(
            "## Delegation and self-audit", "## Notes"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "stage section"):
            checker.check_sequence()

    def test_cli_setting_must_match_the_public_operation(self):
        contract = self.prompts / "CLI_CONTRACT.md"
        contract.write_text(contract.read_text(encoding="utf-8").replace(
            "| `0x0205` | `polarity` |", "| `0x0205` | `angle` |"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "different setting key"):
            checker.check_operations()

    def test_semantic_units_and_common_sku_bounds(self):
        self.assertEqual(checker.common_write_word("baud", "14400"), 4)
        self.assertEqual(checker.common_write_word("uart-unit", "us"), 1)
        for key, value in (("uart-unit", "2"), ("baud", "4"), ("range", "5"), ("threshold", "5000")):
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                checker.common_write_word(key, value)
