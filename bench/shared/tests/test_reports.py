import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

BENCH = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(BENCH / "shared/scripts"))
from registration import registered


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


production = module("production_report", BENCH / "production/scripts/report.py")
hashmaps = module("hashmap_report", BENCH / "hashmaps/scripts/report_hashmaps.py")


class ReportContractTests(unittest.TestCase):
    def test_request_only_registration_does_not_require_response(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            adapter = root / "example"
            adapter.mkdir()
            (adapter / "request.c").write_text(
                "int example_request_with_store(); size_t example_header_lookup();")
            self.assertEqual(list(registered(root, "_with_store", ("header_lookup",),
                                             directions=("request",))), ["example"])
            self.assertEqual(list(registered(root)), [])
            adapter.rename(root / "_example")
            self.assertEqual(list(registered(root, "_with_store", directions=("request",))), [])

    def test_production_baseline_is_fastest_first_lookup_total(self):
        def row(mean, label):
            return {"mean": mean, "label": label, "rciw": 0, "stddev": 0}
        group = {
            "A": {"parse": row(100, "A"), "hit": row(100, "A")},
            "B": {"parse": row(110, "B"), "hit": row(1, "B")},
        }
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            production.total_table(group, "hit", "Hit")
        first = next(line for line in output.getvalue().splitlines() if line.startswith("| B "))
        self.assertIn("1.00×", first)
        self.assertIn("Baseline", first)

    def test_hashmap_report_excludes_inactive_variants_and_maps(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "active.json").write_text(json.dumps(["example-native"]))
            result = (
                "count,samples,iterations,container_bytes,growths,mean_ns,stddev_ns,load_factor,rciw,operation,scenario,mode,map\n"
                "32,20,100,1000,0,100,0.1,0.5,0.01,build,reserved,case-sensitive,example\n")
            for name in ("example-native", "example-siphash", "disabled-native"):
                (root / f"{name}.csv").write_text(result)
            rows = hashmaps.load_results(root)
            self.assertEqual([(row["id"], row["variant"]) for row in rows],
                             [("example", "native")])
            (root / "active.json").write_text(
                json.dumps(["example-native", "example-siphash"]))
            rows = hashmaps.load_results(root)
            self.assertEqual([(row["id"], row["variant"]) for row in rows],
                             [("example", "native"), ("example", "siphash")])

    def test_hashmap_rounding_and_strict_crossover(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "active.json").write_text(json.dumps(["example-native"]))
            (root / "example-native.csv").write_text(
                "count,samples,iterations,container_bytes,growths,mean_ns,stddev_ns,load_factor,rciw,operation,scenario,mode,map\n"
                "32,20,100,1000,0,100.004,0.1,0.5,0.01,build,reserved,case-sensitive,example\n")
            self.assertEqual(hashmaps.load_results(root)[0]["mean_ns"], 100.00)
        def row(name, operation, mean):
            return {"id": name, "map": name, "operation": operation,
                    "mean_ns": mean, "stddev_ns": 0, "rciw": 0}
        rows = [row("A", "build", 100), row("A", "hit", 10),
                row("B", "build", 200), row("B", "hit", 5)]
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            hashmaps.render_total_cost(rows, "hit")
        result = next(line for line in output.getvalue().splitlines() if line.startswith("| B "))
        self.assertEqual(result.split("|")[-2].strip(), "21")


if __name__ == "__main__":
    unittest.main()
