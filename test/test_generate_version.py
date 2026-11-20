#!/usr/bin/env python3
"""Execute the real version CLI against isolated copies of its input files."""

import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
FILES = (
    "library.json",
    "Doxyfile",
    "idf_component.yml",
    "include/SEN059x/Version.h",
    "scripts/generate_version.py",
)


class VersionGeneratorTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="sen059x-version-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for relative in FILES:
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, target)
        self.version = json.loads(self.read("library.json"))["version"]

    def read(self, relative):
        return (self.root / relative).read_text(encoding="utf-8")

    def write(self, relative, text):
        with (self.root / relative).open("w", encoding="utf-8", newline="\n") as output:
            output.write(text)

    def snapshot(self):
        return {
            relative: ((self.root / relative).read_bytes(),
                       (self.root / relative).stat().st_mtime_ns)
            for relative in FILES
        }

    def run_cli(self, *arguments, expected=0):
        result = subprocess.run(
            [sys.executable, str(self.root / "scripts/generate_version.py"), *arguments],
            cwd=self.root,
            capture_output=True,
            text=True,
            timeout=15,
            check=False,
        )
        self.assertEqual(expected, result.returncode, result.stdout + result.stderr)
        return result

    def stale_manifest(self):
        text = self.read("idf_component.yml")
        stale = re.sub(r'^version:.*$', 'version: "0.0.0" # keep this comment',
                       text, count=1, flags=re.MULTILINE)
        self.assertNotEqual(text, stale)
        self.write("idf_component.yml", stale)
        return stale

    def test_check_rejects_stale_manifest_without_writing_any_file(self):
        self.stale_manifest()
        before = self.snapshot()
        result = self.run_cli("check", expected=1)
        self.assertIn("Out of date:", result.stdout)
        self.assertIn("idf_component.yml", result.stdout)
        self.assertEqual(before, self.snapshot())

    def test_sync_updates_only_manifest_version_and_is_idempotent(self):
        stale = self.stale_manifest()
        # An unrelated nested dependency version must remain untouched, as must
        # metadata/export filters and the inline comment on the root version.
        stale += '\ncustom_metadata:\n  version: "8.7.6"\n'
        self.write("idf_component.yml", stale)
        before = self.snapshot()
        self.run_cli("sync")
        expected = stale.replace('version: "0.0.0"', f'version: "{self.version}"', 1)
        self.assertEqual(expected, self.read("idf_component.yml"))
        after = self.snapshot()
        for relative in FILES:
            if relative != "idf_component.yml":
                self.assertEqual(before[relative], after[relative])
        self.run_cli("sync")
        self.run_cli("check")
        self.assertEqual(after, self.snapshot())

    def test_source_version_change_synchronizes_all_three_outputs(self):
        data = json.loads(self.read("library.json"))
        data["version"] = "4.5.6"
        self.write("library.json", json.dumps(data, indent=2) + "\n")
        before = self.snapshot()
        result = self.run_cli("check", expected=1)
        for name in ("Version.h", "Doxyfile", "idf_component.yml"):
            self.assertIn(name, result.stdout)
        self.assertEqual(before, self.snapshot())
        self.run_cli("sync")
        self.assertIn('#define SEN059X_VERSION_STRING "4.5.6"',
                      self.read("include/SEN059x/Version.h"))
        self.assertRegex(self.read("Doxyfile"), r'PROJECT_NUMBER\s*=\s*"4\.5\.6"')
        self.assertIn('version: "4.5.6"', self.read("idf_component.yml"))
        self.run_cli("check")

    def test_set_and_bump_preserve_build_metadata_policy(self):
        self.run_cli("set", "2.3.4")
        self.run_cli("bump", "patch")
        self.assertEqual("2.3.5", json.loads(self.read("library.json"))["version"])
        self.assertIn('version: "2.3.5"', self.read("idf_component.yml"))
        header = self.read("include/SEN059x/Version.h")
        self.assertIn("#define SEN059X_BUILD_DATE __DATE__", header)
        self.assertIn("#define SEN059X_BUILD_TIME __TIME__", header)
        self.assertIn('#define SEN059X_GIT_COMMIT "unknown"', header)
        self.assertIn('#define SEN059X_GIT_STATUS "unknown"', header)
        self.run_cli("check")

    def test_prerelease_labels_survive_sync(self):
        self.run_cli("set", "1.2.3-dev.4")
        header = self.read("include/SEN059x/Version.h")
        self.assertIn('#define SEN059X_VERSION_STRING "1.2.3-dev.4"', header)
        self.assertIn("VERSION_CODE = 10203;", header)
        self.run_cli("check")
        before = self.snapshot()
        for malformed in ("1.2.3-dev.04", "1.2.3-dev.", "1.2.3-"):
            self.run_cli("set", malformed, expected=1)
        self.assertEqual(before, self.snapshot())

    def test_missing_or_duplicate_root_version_is_rejected_without_writes(self):
        original = self.read("idf_component.yml")
        for malformed in (
            re.sub(r'^version:.*\n', "", original, count=1, flags=re.MULTILINE),
            original + '\nversion: "9.9.9"\n',
        ):
            with self.subTest(manifest=malformed):
                self.write("idf_component.yml", malformed)
                before = self.snapshot()
                result = self.run_cli("sync", expected=1)
                self.assertIn("exactly one root version assignment", result.stderr)
                self.assertEqual(before, self.snapshot())

    def test_set_and_bump_reject_malformed_templates_without_writes(self):
        cases = (
            ("idf_component.yml", r'^version:.*\n', '\nversion: "9.9.9"\n',
             "exactly one root version assignment"),
            ("Doxyfile", r'^PROJECT_NUMBER[^\n]*\n', '\nPROJECT_NUMBER = "9.9.9"\n',
             "exactly one PROJECT_NUMBER assignment"),
        )
        for relative, pattern, duplicate, message in cases:
            original = self.read(relative)
            malformed_templates = (
                ("missing", re.sub(pattern, "", original, count=1, flags=re.MULTILINE)),
                ("duplicate", original + duplicate),
            )
            for label, malformed in malformed_templates:
                self.write(relative, malformed)
                for arguments in (("set", "4.5.6"), ("bump", "patch")):
                    with self.subTest(file=relative, error=label, command=arguments[0]):
                        before = self.snapshot()
                        result = self.run_cli(*arguments, expected=1)
                        self.assertIn(message, result.stderr)
                        self.assertEqual(before, self.snapshot())
            self.write(relative, original)


if __name__ == "__main__":
    unittest.main()
