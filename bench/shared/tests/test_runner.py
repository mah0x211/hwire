import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import run as runner
import actions_benchmark
from report_common import report_failures


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.suite = self.root / "production"
        (self.suite / "scripts").mkdir(parents=True)
        (self.root / "platform.sh").write_text('#!/bin/sh\nprintf "cpu: test\\n" > "$1"\n')
        self.names = ["good", "buildfail", "measurefail", "setupfail"]
        (self.suite / "scripts/gen_production.py").write_text(
            'print(' + repr(" ".join(self.names)) + ')\n')
        (self.suite / "Makefile").write_text('''
benchmark-config:
	@printf '%s\\n' 'scalar native' '$(ADAPTER)' 'cc' '-O2' '' '' 'ADAPTER_TOKEN=from-config'
benchmark-info:
	@test "$$ADAPTER_TOKEN" = from-config
	@printf '\\n'
build-adapter:
	@python3 fake.py build '$(ADAPTER)' '$(VARIANTS)' '$(BIN)'
report:
	@python3 fake.py report
''')
        (self.suite / "fake.py").write_text('''import json, os, pathlib, sys
if sys.argv[1] != "report" and os.environ.get("ADAPTER_TOKEN") != "from-config":
 raise SystemExit("error: adapter environment missing during build")
if sys.argv[1] == "report":
 print("SUCCESSFUL: " + ",".join(json.loads(pathlib.Path("results/active.json").read_text())))
else:
 name, variant, directory = sys.argv[2:]
 if name == "buildfail" and variant == "native":
  raise SystemExit("error: intentionally failed build")
 binary = pathlib.Path(directory) / variant / "bench_production"
 binary.parent.mkdir(parents=True, exist_ok=True)
 binary.write_text("#!/usr/bin/env python3\\nimport os, pathlib\\nassert os.environ.get('ADAPTER_TOKEN') == 'from-config'\\np=pathlib.Path('results')\\np.mkdir()\\n(p/" + repr(name+'-'+variant+'.csv') + ").write_text('fixture,operation\\\\n')\\nraise SystemExit(" + str(int(name == "measurefail")) + ")\\n")
 binary.chmod(0o755)
''')
        for name in self.names:
            directory = self.suite / name
            directory.mkdir()
            (directory / "setup.sh").write_text(
                f'#!/bin/sh\ntest "$ADAPTER_TOKEN" = from-config || exit 8\necho "{name}:setup:$1" >> trace\n' +
                ('echo "Missing dependency"; exit 1\n' if name == "setupfail" else ''))
            (directory / "fetch.sh").write_text(
                f'#!/bin/sh\ntest "$ADAPTER_TOKEN" = from-config || exit 8\necho "{name}:fetch" >> trace\n')

    def measure(self, **kwargs):
        with contextlib.redirect_stdout(io.StringIO()):
            return runner.run(self.suite, **kwargs)

    def test_failures_are_isolated_and_partial_results_excluded(self):
        results = self.suite / "results"
        results.mkdir()
        (results / "stale-native.csv").write_text("stale")
        self.assertEqual(self.measure(), 1)
        active = json.loads((results / "active.json").read_text())
        self.assertEqual(set(active), {"good-scalar", "good-native", "buildfail-scalar"})
        self.assertFalse((results / "stale-native.csv").exists())
        self.assertFalse((results / "measurefail-native.csv").exists())
        states = json.loads((results / "status.json").read_text())
        failures = {row["phase"] for row in states if row["exit_code"]}
        self.assertEqual(failures, {"setup", "build", "measure"})
        trace = (self.suite / "trace").read_text()
        self.assertNotIn("setupfail:fetch", trace)
        self.assertIn("good:setup:check\ngood:fetch", trace)
        self.assertIn("good-native", (results / "logs/report.log").read_text())
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertTrue(report_failures(results))
        self.assertIn("Missing dependency", output.getvalue())

    def test_fetch_failure_skips_build_and_other_adapters_continue(self):
        (self.suite / "good/fetch.sh").write_text('echo "error: fetch failed"; exit 1\n')
        self.assertEqual(self.measure(adapters=["good", "buildfail"], variants=["scalar"]), 1)
        states = json.loads((self.suite / "results/status.json").read_text())
        self.assertEqual([(row["adapter"], row["phase"]) for row in states],
                         [("good", "fetch"), ("buildfail", "measure")])
        self.assertFalse((self.suite / "bin/good").exists())

    def test_install_is_explicit_and_build_preserves_measurements(self):
        results = self.suite / "results"
        results.mkdir()
        (results / "preserved.csv").write_text("old measurement")
        self.assertEqual(self.measure(adapters=["good"], variants=["native"],
                                      install=True, action="build"), 0)
        self.assertIn("good:setup:install", (self.suite / "trace").read_text())
        self.assertTrue((results / "preserved.csv").exists())
        states = json.loads((self.suite / "bin/status/build/status.json").read_text())
        self.assertEqual(states[0]["phase"], "build")
        self.assertEqual(json.loads((self.suite / "bin/status/build/active.json").read_text()), {})

    def test_all_failed_targets_still_produce_failure_report(self):
        self.assertEqual(self.measure(adapters=["setupfail"]), 1)
        results = self.suite / "results"
        self.assertEqual(json.loads((results / "active.json").read_text()), {})
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertTrue(report_failures(results))
        self.assertIn("setupfail", output.getvalue())
        self.assertTrue((results / "logs/report.log").is_file())

    def test_setup_only_does_not_build(self):
        self.assertEqual(self.measure(adapters=["good"], action="setup"), 0)
        self.assertFalse((self.suite / "bin/good").exists())
        self.assertIn("good:fetch", (self.suite / "trace").read_text())

    def test_unsupported_variant_reports_no_success(self):
        self.assertEqual(self.measure(adapters=["good"], variants=["unsupported"]), 1)
        self.assertFalse((self.suite / "trace").exists())
        states = json.loads((self.suite / "results/status.json").read_text())
        self.assertEqual(states[0]["phase"], "configure")


class ActionsTests(unittest.TestCase):
    def test_failure_does_not_stop_later_suites(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            args = ["actions_benchmark.py", "--source", str(root), "--suite", "all",
                    "--label", "measured", "--output", str(root / "out"),
                    "--summary", str(root / "summary.md")]
            with patch.object(sys, "argv", args), \
                 patch.object(actions_benchmark, "metadata", return_value=("commit", "env")), \
                 patch.object(actions_benchmark, "run_suite", side_effect=[1, 0, 0]) as execute:
                self.assertEqual(actions_benchmark.main(), 1)
            self.assertEqual([call.args[1] for call in execute.call_args_list],
                             ["parsers", "hashmaps", "production"])

    def test_partial_report_is_preserved_when_suite_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            suite = root / "bench/parsers"
            suite.mkdir(parents=True)
            (suite / "Makefile").write_text('''run:
	@mkdir -p results
	@echo failed > results/status.json
	@exit 1
report:
	@echo PARTIAL_REPORT
''')
            with contextlib.redirect_stdout(io.StringIO()):
                code = actions_benchmark.run_suite(root, "parsers", root / "out", root / "summary.md")
            self.assertNotEqual(code, 0)
            self.assertIn("PARTIAL_REPORT", (root / "summary.md").read_text())
            self.assertIn("failed", (root / "summary.md").read_text())
            self.assertTrue((root / "out/results/status.json").exists())


if __name__ == "__main__":
    unittest.main()
