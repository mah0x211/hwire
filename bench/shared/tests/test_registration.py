import contextlib
from io import StringIO
import os
import shlex
import subprocess
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from registration import adapter_directories, write_generated
from report_common import publish_readme


class SharedHelpersTests(unittest.TestCase):
    def test_disabled_and_unselected_directories_are_excluded(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ("first", "second", "_disabled", "invalid-name"):
                (root / name).mkdir()
            with patch.dict(os.environ, {"BENCH_ADAPTER": "second"}):
                self.assertEqual([p.name for p in adapter_directories(root)], ["second"])
            with patch.dict(os.environ, {"BENCH_ADAPTER": ""}), contextlib.redirect_stderr(StringIO()):
                self.assertEqual([p.name for p in adapter_directories(root)], ["first", "second"])

    def test_make_metadata_preserves_quoted_environment_values(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            shared = Path(__file__).resolve().parents[1]
            (root / "Makefile").write_text(
                "ADAPTER := example\nVARIANTS := native\n"
                "example_ENV := FIRST=\"two words\" SECOND='quoted value'\n"
                f"include {shared}/runner.mk\n")
            fields = subprocess.check_output(["make", "--silent", "benchmark-config"],
                                             cwd=root, text=True).splitlines()
            env = dict(value.split("=", 1) for value in shlex.split(fields[-1]))
            self.assertEqual(env, {"FIRST": "two words", "SECOND": "quoted value"})

    def test_unchanged_generated_files_keep_their_timestamp(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "registry.c"
            write_generated(out, ["first"])
            before = out.stat().st_mtime_ns
            write_generated(out, ["first"])
            self.assertEqual(out.stat().st_mtime_ns, before)
            write_generated(out, ["second"])
            self.assertEqual(out.read_text(), "second\n")

    def test_publishing_preserves_docs_and_replaces_environment(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            readme, platform = root / "README.md", root / "platform.txt"
            platform.write_text("cpu: new CPU\n")
            prefix = "# Documentation\n\n# Benchmark\n\n<!-- benchmark-environment -->\nold CPU\n<!-- /benchmark-environment -->"
            publish_readme(readme, prefix, platform, lambda: print("## New report\n\nresult"),
                           separator="\n\n<!-- results -->\n\n")
            text = readme.read_text()
            self.assertIn("# Documentation", text)
            self.assertIn("new CPU", text)
            self.assertNotIn("old CPU", text)
            self.assertEqual(text.count("<!-- results -->"), 1)
            self.assertIn("## New report\n\nresult\n", text)


if __name__ == "__main__":
    unittest.main()
