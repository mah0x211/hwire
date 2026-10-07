#!/usr/bin/env python3
"""Compare benchmark results written by `make run` into results/.

Each result file is named <parser>-<variant>.txt and contains one line
per benchmark:

    <fixture>/<bytes> <samples> <iterations> <mean-ns> <stddev-ns> <rciw>

The report groups all compiler targets per fixture, ordered by mean time.
"""

import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "shared/scripts"))
from report_common import environment, render, message_section

RESULTS_DIR = Path(__file__).resolve().parents[1] / "results"
PLATFORM_FILE = RESULTS_DIR / "platform.txt"

CANONICAL_VARIANTS = ["nosimd", "sse2", "neon", "sse42", "native"]

def load_results(directory: Path) -> dict[str, dict[str, dict]]:
    """row name -> fixture -> {bytes, samples, iterations, mean, stddev}."""
    data: dict[str, dict[str, dict]] = {}
    manifest = directory / "active.json"
    active = json.loads(manifest.read_text()) if manifest.exists() else None
    for path in sorted(directory.glob("*.txt")):
        if path.name == PLATFORM_FILE.name or (active is not None and path.stem not in active):
            continue
        row: dict[str, dict] = {}
        for line in path.read_text().splitlines():
            fields = line.split()
            if not fields:
                continue
            name, _, size = fields[0].rpartition("/")
            entry = {"bytes": int(size) if size else None}
            if isinstance(active, dict):
                entry["label"] = active[path.stem]
            (entry["samples"], entry["iterations"], entry["mean"],
             entry["stddev"], entry["rciw"]) = map(float, fields[1:])
            row[name] = entry
        if row:
            data[path.stem] = row
    return data


def split_variant(name: str) -> tuple[str, str]:
    """'picohttpparser-sse42' -> ('picohttpparser', 'sse42')."""
    base, _, variant = name.rpartition("-")
    if base and variant in CANONICAL_VARIANTS:
        return base, variant
    return name, "default"


def human_rate(ns: float) -> str:
    if ns <= 0:
        return "-"
    rate = 1e9 / ns
    for unit, scale in (("M msg/s", 1e6), ("k msg/s", 1e3)):
        if rate >= scale:
            return f"{rate / scale:.2f} {unit}"
    return f"{rate:.2f} msg/s"


def main(include_environment=True) -> None:
    if not RESULTS_DIR.is_dir():
        sys.exit(f"no results directory at {RESULTS_DIR} (run `make run` first)")

    data = load_results(RESULTS_DIR)
    if not data:
        sys.exit(f"no result files in {RESULTS_DIR} (run `make run` first)")

    if include_environment:
        environment(PLATFORM_FILE)

    print("\n\n## Parse\n")
    print("Start line and headers only; native initialization or reset is timed. SIMD labels\n"
          "identify compiler targets, rather than guaranteeing SIMD use by every library.\n"
          "Scalar rows disable explicit parser SIMD and compiler loop vectorization.\n\n"
          "† Target RCIW was not reached. Relative compares the fastest build for each fixture.\n")
    suffixes = {"nosimd": " (scalar)", "sse2": " (SSE2)", "sse42": " (SSE4.2)",
                "neon": " (NEON)", "native": " (native)", "default": ""}
    fixtures = sorted({fixture for row in data.values() for fixture in row})
    for fixture in fixtures:
        message_section(fixture)
        measured = sorted(((name, results[fixture]) for name, results in data.items()
                           if fixture in results), key=lambda item: item[1]["mean"])
        fastest = measured[0][1]["mean"]
        rows = []
        for name, entry in measured:
            parser, variant = split_variant(name)
            marker = " †" if entry["rciw"] > 0.02 else ""
            rows.append([entry.get("label", parser) + suffixes[variant],
                         f"{entry['mean']:.1f} ±{entry['stddev']:.1f}{marker}",
                         f"{entry['mean']/fastest:.2f}×", human_rate(entry["mean"]),
                         f"{100*entry['rciw']:.2f}%"])
        render(None, ["Parser", "Mean ± SD (ns/message)", "Relative", "Throughput", "RCIW"], rows)




if __name__ == "__main__":
    main()
