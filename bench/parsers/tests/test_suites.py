import json
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import gen_parsers
import report
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "shared/scripts"))
import gen_fixtures


class DataTests(unittest.TestCase):
    def test_http_fixtures_use_data_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "fixtures.c"
            gen_fixtures.write_table(str(path))
            self.assertIn('#include "../data/request/browser_get.h"', path.read_text())
            self.assertIn('#include "../data/response/browser_response.h"', path.read_text())
            self.assertNotIn("messages/", path.read_text())

    def test_fixture_registration_updates_only_when_names_change(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "data"
            request = data / "request"
            request.mkdir(parents=True)
            (data / "response").mkdir()
            header = request / "example.h"
            header.write_text('static const unsigned char MSG_EXAMPLE[] = "";')
            output = root / "fixtures.c"
            previous = gen_fixtures.BASE
            try:
                gen_fixtures.BASE = data
                gen_fixtures.write_table(str(output))
                original = output.stat().st_mtime_ns
                gen_fixtures.write_table(str(output))
                self.assertEqual(output.stat().st_mtime_ns, original)
                self.assertIn("example.h", output.read_text())
                header.unlink()
                gen_fixtures.write_table(str(output))
                self.assertNotIn("example.h", output.read_text())
            finally:
                gen_fixtures.BASE = previous


class RegistrationTests(unittest.TestCase):
    def test_parser_adapters_do_not_depend_on_driver(self):
        root = Path(__file__).resolve().parents[1]

        for name in gen_parsers.registered(root):
            for source in (root / name / "request.c",
                           root / name / "response.c"):
                self.assertNotIn("BENCH_", source.read_text(), str(source))


class ReportTests(unittest.TestCase):

    def test_only_active_parser_results_are_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ("hwire-sse42", "disabled-sse42"):
                (root / f"{name}.txt").write_text("req/95 10 100000 50 1 0.01\n")
            (root / "active.json").write_text(json.dumps(["hwire-sse42"]))
            self.assertEqual(set(report.load_results(root)), {"hwire-sse42"})

    def test_parser_rate(self):
        self.assertEqual(report.human_rate(200),"5.00 M msg/s")
        self.assertEqual(report.human_rate(200000),"5.00 k msg/s")
        self.assertEqual(report.human_rate(2000000000),"0.50 msg/s")
        self.assertEqual(report.human_rate(0),"-")
        self.assertEqual(report.split_variant("hwire-neon"),("hwire","neon"))

if __name__=="__main__": unittest.main()
