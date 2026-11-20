"""Guard the implementation plan against missing capabilities and skipped reviews."""

import json
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
        manifest = json.loads((self.prompts / "SEQUENCE.json").read_text(encoding="utf-8"))
        self.assertEqual(checker.check_sequence(), len(manifest["stages"]))
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
        stage = next(self.prompts.glob("01-*.md"))
        stage.write_text(stage.read_text(encoding="utf-8").replace(
            "## Delegation and self-audit", "## Notes"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "stage section"):
            checker.check_sequence()

    def test_human_navigation_must_agree_with_manifest(self):
        manifest = json.loads((self.prompts / "SEQUENCE.json").read_text(encoding="utf-8"))
        first = manifest["stages"][0]
        changes = (
            (first["file"], f"# 01 — {first['title']}", "# 99 — Stale title", "stage heading"),
            ("README.md", "Initial scaffold", "02", "stage index"),
            ("PROGRESS.md", f"| 01 | `{first['produces']}`", "| 01 | `old-output`", "ledger"),
            ("START_HERE.md", f"docs/prompts/{first['file']}", "docs/prompts/99-old-name.md", "START_HERE"),
        )
        for filename, expected, incorrect, message in changes:
            with self.subTest(filename=filename):
                path = self.prompts / filename
                original = path.read_text(encoding="utf-8")
                self.assertIn(expected, original)
                path.write_text(original.replace(expected, incorrect), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, message):
                    checker.check_sequence()
                path.write_text(original, encoding="utf-8")

    def test_optional_physical_review_can_remain_pending(self):
        manifest = json.loads((self.prompts / "SEQUENCE.json").read_text(encoding="utf-8"))
        stages = {stage["produces"]: stage for stage in manifest["stages"]}
        self.assertEqual(set(stages["bench-audit"]["review_if_available"]), checker.BENCH_OUTPUTS)
        required_optional = checker.BENCH_OUTPUTS | {"bench-audit"}
        self.assertTrue(required_optional.issubset(stages["delivery-review"]["review_if_available"]))
        # Require the physical audit indirectly through the operator guide.
        docs = stages["operator-docs"]
        docs["requires"].append("bench-audit")
        path = self.prompts / docs["file"]
        path.write_text(path.read_text(encoding="utf-8").replace(
            "## Inputs\n", "## Inputs\n\nRequires `bench-audit`.\n"), encoding="utf-8")
        (self.prompts / "SEQUENCE.json").write_text(json.dumps(manifest), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "optional evidence is already a required prerequisite"):
            checker.check_sequence()

    def test_audit_ranges_include_only_the_whole_preceding_group(self):
        path = self.prompts / "SEQUENCE.json"
        manifest = json.loads(path.read_text(encoding="utf-8"))
        checkpoint = next(stage for stage in manifest["stages"] if "audits" in stage)
        originals = checkpoint["audits"]
        for invalid in (originals[1:], originals[::-1], originals[:-1] + [checkpoint["id"]]):
            with self.subTest(sources=invalid):
                checkpoint["audits"] = invalid
                path.write_text(json.dumps(manifest), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "preceding original prompt range"):
                    checker.check_sequence()

    def test_checkpoint_source_links_must_point_to_the_declared_originals(self):
        manifest = json.loads((self.prompts / "SEQUENCE.json").read_text(encoding="utf-8"))
        stages = {stage["id"]: stage for stage in manifest["stages"]}
        checkpoint = next(stage for stage in stages.values() if "audits" in stage)
        first, second = (stages[source]["file"] for source in checkpoint["audits"][:2])
        path = self.prompts / checkpoint["file"]
        lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
        line = next(i for i, value in enumerate(lines) if value.startswith("Original prompts "))
        original = lines[line]
        for before, after in ((f"({first})", f"({second})"), (f"[{checkpoint['audits'][0]}]", "[99]")):
            with self.subTest(replacement=after):
                lines[line] = original.replace(before, after)
                path.write_text("".join(lines), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "original prompt links"):
                    checker.check_sequence()

    def test_review_cannot_drop_parallel_scope_or_actual_diff_verification(self):
        manifest = json.loads((self.prompts / "SEQUENCE.json").read_text(encoding="utf-8"))
        stages = manifest["stages"]
        for stage in (stages[0], next(s for s in stages if "audits" in s)):
            path = self.prompts / stage["file"]
            original = path.read_text(encoding="utf-8")
            for before, after, error in (
                ("in parallel", "sequentially", "parallel subagents"),
                ("code simplicity", "naming", "six audit topics"),
                ("actual code and diff", "previous summary", "inspect actual code and diff"),
                ("Fix every confirmed", "Defer every confirmed", "fix confirmed issues"),
                ("Repeat the required verification", "Record the earlier results", "verify until complete"),
                ("concise report", "status flag", "report findings"),
            ):
                with self.subTest(stage=stage["id"], rule=before):
                    self.assertIn(before, original)
                    path.write_text(original.replace(before, after), encoding="utf-8")
                    with self.assertRaisesRegex(ValueError, error):
                        checker.check_sequence()
            path.write_text(original, encoding="utf-8")

    def test_terminal_handoff_cannot_bypass_its_checkpoint(self):
        manifest = json.loads((self.prompts / "SEQUENCE.json").read_text(encoding="utf-8"))
        delivery = next(s for s in manifest["stages"] if s["produces"] == "delivery-review")
        delivery["requires"].remove("package-audit")
        (self.prompts / "SEQUENCE.json").write_text(json.dumps(manifest), encoding="utf-8")
        path = self.prompts / delivery["file"]
        path.write_text(path.read_text(encoding="utf-8").replace(
            "`package-audit`", "the build evidence"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "bypasses software audit"):
            checker.check_sequence()

    def test_incoming_document_link_cannot_point_to_a_retired_prompt(self):
        root = self.prompts.parent
        docs = root / "docs"
        docs.mkdir()
        (docs / "ARCHITECTURE.md").write_text("[consumer](../prompts/99-retired.md)", encoding="utf-8")
        with patch.object(checker, "ROOT", root):
            with self.assertRaisesRegex(ValueError, "ARCHITECTURE.md: missing local link"):
                checker.check_links()

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
