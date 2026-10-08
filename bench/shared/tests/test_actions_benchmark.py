import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from actions_benchmark import main, run_suite


class ActionsBenchmarkTests(unittest.TestCase):
    def test_reports_and_raw_results_are_preserved_for_each_suite(self):
        with tempfile.TemporaryDirectory(prefix="bench actions ") as tmp:
            root = Path(tmp)
            summary = root / "summary.md"
            for suite in ("parsers", "hashmaps", "production"):
                directory = root / "source" / "bench" / suite
                directory.mkdir(parents=True)
                results = "results/storage" if suite == "hashmaps" else "results"
                (directory / "README.md").write_text("published snapshot\n")
                (directory / "Makefile").write_text(
                    f"run:\n\t@mkdir -p {results}\n"
                    f"\t@echo 'raw measurement' > {results}/sample.csv\n"
                    f"\t@echo 'cflags: -O2' > {results}/platform.txt\n"
                    "\t@echo 'progress' >&2\n"
                    "report:\n\t@echo '| Map | Mean |'\n"
                    "\t@echo '| --- | --- |'\n\t@echo '| example | 10 |'\n")
                output = root / "artifacts" / suite
                with contextlib.redirect_stdout(io.StringIO()):
                    code = run_suite(root / "source", suite, output, summary)
                self.assertEqual(code, 0)
                self.assertIn("| example | 10 |", (output / "report.md").read_text())
                self.assertIn("progress", (output / "run.log").read_text())
                self.assertEqual((output / results / "sample.csv").read_text(), "raw measurement\n")
                self.assertEqual((directory / "README.md").read_text(), "published snapshot\n")
            self.assertEqual(summary.read_text().count("| example | 10 |"), 3)
            self.assertEqual(summary.read_text().count("cflags: -O2"), 3)

    def test_failed_measurement_keeps_partial_results_and_error_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            directory = root / "source/bench/parsers"
            directory.mkdir(parents=True)
            (directory / "Makefile").write_text(
                "run:\n\t@mkdir results\n\t@echo partial > results/sample.txt\n"
                "\t@echo 'measurement failed' >&2\n\t@exit 7\n"
                "report:\n\t@echo 'must not render'\n")
            output, summary = root / "output", root / "summary.md"
            with contextlib.redirect_stdout(io.StringIO()):
                code = run_suite(root / "source", "parsers", output, summary)
            self.assertNotEqual(code, 0)
            self.assertIn("failed", summary.read_text())
            self.assertNotIn("must not render", summary.read_text())
            self.assertIn("measurement failed", (output / "run.log").read_text())
            self.assertTrue((output / "results/sample.txt").exists())
            self.assertFalse((output / "report.md").exists())

    def test_failed_report_is_not_published_as_a_success(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            directory = root / "source/bench/parsers"
            directory.mkdir(parents=True)
            (directory / "Makefile").write_text(
                "run:\n\t@mkdir results\n"
                "report:\n\t@echo 'partial report'\n"
                "\t@echo 'render failed' >&2\n\t@exit 8\n")
            output, summary = root / "output", root / "summary.md"
            with contextlib.redirect_stdout(io.StringIO()):
                code = run_suite(root / "source", "parsers", output, summary)
            self.assertNotEqual(code, 0)
            self.assertNotIn("partial report", summary.read_text())
            self.assertIn("render failed", (output / "report.log").read_text())
            self.assertIn("partial report", (output / "report.md").read_text())

    def test_revisions_keep_separate_reports_and_commit_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            summary = root / "summary.md"
            for label, commit in (("measured", "a" * 40), ("comparison", "b" * 40)):
                source = root / label
                directory = source / "bench/hashmaps"
                directory.mkdir(parents=True)
                (directory / "Makefile").write_text(
                    "run:\n\t@mkdir -p results/storage\n"
                    f"\t@echo {label} > results/storage/raw.csv\n"
                    f"report:\n\t@echo '{label} report'\n")
                output = root / "artifacts" / label
                argv = ["actions_benchmark.py", "--source", str(source), "--suite", "hashmaps",
                        "--label", label, "--output", str(output), "--summary", str(summary)]
                with patch("sys.argv", argv), patch("actions_benchmark.metadata",
                        return_value=(commit, f"commit: {commit}\n")) as metadata:
                    with contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(main(), 0)
                    metadata.assert_called_once_with(source.resolve())
                self.assertEqual((output / "hashmaps/results/storage/raw.csv").read_text(), label + "\n")
                self.assertEqual((output / "hashmaps/report.md").read_text(), label + " report\n")
                self.assertIn(commit, (output / "environment.txt").read_text())
                self.assertIn(commit, summary.read_text())
            self.assertIn("measured report", summary.read_text())
            self.assertIn("comparison report", summary.read_text())

    def test_revision_without_suite_fails_clearly(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            output, summary = root / "output", root / "summary.md"
            code = run_suite(root, "production", output, summary)
            self.assertNotEqual(code, 0)
            self.assertIn("no suite Makefile", summary.read_text())
            self.assertIn("Missing suite Makefile", (output / "status.txt").read_text())


if __name__ == "__main__":
    unittest.main()
