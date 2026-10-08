"""Tests for execution statistics from successful and interrupted fuzz runs."""

from pathlib import Path
import tempfile
import unittest

from coverage import read_run_statistics


class RunStatisticsTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)

    def test_completed_run(self):
        (self.work / "run.log").write_text(
            "INFO: Seed: 0\nDone 1000 runs in 2 second(s)\n"
            "stat::number_of_executed_units: 1000\n"
            "stat::average_exec_per_sec: 500\nstat::peak_rss_mb: 55\n")
        corpus = self.work / "corpus"
        corpus.mkdir()
        (corpus / "first").write_bytes(b"ab")
        (corpus / "second").write_bytes(b"xyz")
        (corpus / "directory").mkdir()
        self.assertEqual(read_run_statistics(self.work), {
            "executions": 1000, "elapsed_seconds": 2, "exec_per_second": 500,
            "corpus_files": 2, "corpus_bytes": 5, "peak_rss_mb": 55, "seed": 0,
        })

    def test_interrupted_run_preserves_available_statistics(self):
        (self.work / "run.log").write_text(
            "INFO: Seed: 42\nERROR: libFuzzer: deadly signal\n"
            "stat::number_of_executed_units: 7\nstat::peak_rss_mb: 60\n")
        metrics = read_run_statistics(self.work)
        self.assertEqual(metrics["executions"], 7)
        self.assertEqual(metrics["peak_rss_mb"], 60)
        self.assertEqual(metrics["seed"], 42)
        self.assertIsNone(metrics["elapsed_seconds"])
        self.assertIsNone(metrics["exec_per_second"])
        self.assertIsNone(metrics["corpus_files"])

    def test_missing_log_does_not_report_old_corpus(self):
        corpus = self.work / "corpus"
        corpus.mkdir()
        (corpus / "old").write_bytes(b"old input")
        self.assertTrue(all(value is None for value in
                            read_run_statistics(self.work).values()))

    def test_older_log_without_final_statistics(self):
        (self.work / "run.log").write_text(
            "INFO: Seed: 123\nDone 1000 runs in 2 second(s)\n")
        metrics = read_run_statistics(self.work)
        self.assertEqual(metrics["executions"], 1000)
        self.assertEqual(metrics["elapsed_seconds"], 2)
        self.assertEqual(metrics["exec_per_second"], 500)
        self.assertIsNone(metrics["peak_rss_mb"])


if __name__ == "__main__":
    unittest.main()
