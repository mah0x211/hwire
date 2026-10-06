#!/usr/bin/env python3
"""Render saved storage scenarios; no container correctness checks."""
import argparse
import csv
import json
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RESULTS_DIR = ROOT / "results/storage"
SCENARIOS = ("reserved", "growth-allocated")
MODES = ("case-sensitive", "case-insensitive")
OPERATIONS = ("build", "insert", "hit", "miss")

def load_results(directory):
    manifest = directory / "active.json"
    active = set(json.loads(manifest.read_text())) if manifest.exists() else None
    rows = []
    for path in sorted(directory.glob("*.csv")):
        name, _, variant = path.stem.rpartition("-")
        if active is not None and name not in active:
            continue
        for row in csv.DictReader(path.open()):
            row.update(variant=variant, id=name)
            for column in ("count", "samples", "iterations", "container_bytes", "growths"):
                row[column] = int(row[column])
            for column in ("mean_ns", "stddev_ns", "load_factor"):
                row[column] = float(row[column])
            row["rciw"] = float(row["rciw"]) if row["rciw"] else None
            rows.append(row)
    return rows

def table(headers, rows):
    values = [list(map(str, headers)), *[list(map(str, row)) for row in rows]]
    widths = [max(3, max(map(len, column))) for column in zip(*values)]
    values.insert(1, ["-" * width for width in widths])
    for row in values:
        print("| " + " | ".join(value.ljust(width)
                                 for value, width in zip(row, widths)) + " |")
    print()

def render_memory(rows, count, scenario):
    memory = sorted((r for r in rows if r["operation"] == "build"),
                    key=lambda r: r["container_bytes"])
    baseline = memory[0]["container_bytes"]
    headers = ["Map", "Memory (bytes)", "Relative", "Bytes/key", "Slot load factor"]
    if scenario != "reserved":
        headers.append("Extensions")
    values = []
    for r in memory:
        row = [r["map"], r["container_bytes"],
               f'{r["container_bytes"]/baseline:.2f}×',
               f'{r["container_bytes"]/count:.2f}',
               f'{100*r["load_factor"]:.2f}%']
        if scenario != "reserved":
            row.append(r["growths"])
        values.append(row)
    table(headers, values)

def render_operation(rows, operation):
    group = sorted((r for r in rows if r["operation"] == operation),
                   key=lambda r: r["mean_ns"])
    baseline = group[0]["mean_ns"]
    unit = "ns/table" if operation == "build" else "ns/key" if operation == "insert" else "ns/lookup"
    table(["Map", f"Mean ± SD ({unit})", "Relative", "Mops/s", "Samples", "RCIW"],
          [[r["map"], f'{r["mean_ns"]:.2f} ±{r["stddev_ns"]:.2f}',
            f'{r["mean_ns"]/baseline:.2f}×',
            f'{1000/r["mean_ns"]:.2f}', r["samples"],
            "—" if r["rciw"] is None else f'{r["rciw"]*100:.2f}%' +
            (" (unmet)" if r["rciw"] > 0.02 else "")]
           for r in group])

def render_total_cost(rows, operation):
    builds = {r["id"]: r for r in rows if r["operation"] == "build"}
    lookups = {r["id"]: r for r in rows if r["operation"] == operation}
    order = sorted(builds, key=lambda name: builds[name]["mean_ns"] +
                   lookups[name]["mean_ns"])
    reference = order[0]
    reference_build = builds[reference]["mean_ns"]
    reference_lookup = lookups[reference]["mean_ns"]
    baseline = reference_build + reference_lookup
    values = []
    for name in order:
        build, lookup = builds[name], lookups[name]
        total = build["mean_ns"] + lookup["mean_ns"]
        saving = reference_lookup - lookup["mean_ns"]
        if name == reference:
            crossover = "Baseline"
        elif saving <= 0:
            crossover = "No crossover"
        else:
            crossover = f'{max(1, math.floor((build["mean_ns"] - reference_build) / saving) + 1):,}'
        label = build["map"]
        if any(r["rciw"] is not None and r["rciw"] > 0.02
               for r in (build, lookup)):
            label += " †"
        values.append([label, f'{total/1000:.3f}', f'{total/baseline:.2f}×',
                       f'{lookup["mean_ns"]:.2f} ±{lookup["stddev_ns"]:.2f}', crossover])
    title = operation.capitalize()
    table(["Map", f"Build + 1 {title} (µs)", "Relative",
           f"{title} Mean ± SD (ns/lookup)", "Lookups to beat baseline"], values)

def render(rows):
    scenario_titles = {
        "reserved": "Reserved Capacity",
        "growth-allocated": "Allocated Growth",
    }
    scenario_summaries = {
        "reserved": (
            "Store 32, 64 or 128 unique keys with storage reserved for the entire "
            "dataset. Build includes initial allocation, initialization and all "
            "insertions; no capacity expansion occurs."
        ),
        "growth-allocated": (
            "Grow from an initial reservation of 32 keys to 256 unique keys "
            "with three additional extensions. Build includes initial allocation, "
            "initialization, insertion and expansion work, including ordinary "
            "heap allocations and any copying or rehashing."
        ),
    }
    for variant in sorted({r["variant"] for r in rows}):
        for scenario in SCENARIOS:
            for mode in MODES:
                group = [r for r in rows
                         if (r["variant"], r["scenario"], r["mode"]) ==
                         (variant, scenario, mode)]
                if not group:
                    continue
                backend = "" if variant == "native" else f" ({variant})"
                print(f"\n## {mode.title()} {scenario_titles[scenario]}{backend}\n")
                comparison = ("Exact byte comparisons." if mode == "case-sensitive"
                              else "ASCII case-insensitive comparisons.")
                print(comparison + " " + scenario_summaries[scenario] + "\n")
                print("Cleanup runs outside all timed intervals.\n")
                print("Insert excludes initial setup and includes any expansion "
                      "triggered by insertion. Hit and miss measure the fully "
                      "populated maps.\n")
                metrics = ("memory", *OPERATIONS)
                counts = sorted({r["count"] for r in group})
                for metric in metrics:
                    print(f"\n### {metric.capitalize()}\n")
                    for count in counts:
                        print(f"**{count} keys**\n")
                        case = [r for r in group if r["count"] == count]
                        if metric == "memory":
                            render_memory(case, count, scenario)
                        else:
                            render_operation(case, metric)
                print("\n### Build + Lookup Total Cost\n")
                print("Estimated from measured means as Build + Q × lookup time, "
                      "rather than a timed first lookup immediately after construction. "
                      "Each table uses the fastest Build + 1 lookup as its baseline. "
                      "The crossover column is the minimum total lookup count Q "
                      "that makes a map faster than that baseline; No crossover "
                      "means it cannot overtake under this model. † marks an input "
                      "with unmet Target RCIW; small timing differences make "
                      "crossover estimates uncertain.\n")
                for operation in ("hit", "miss"):
                    print(f"\n#### Build + {operation.capitalize()}\n")
                    for count in counts:
                        print(f"**{count} keys**\n")
                        case = [r for r in group if r["count"] == count]
                        render_total_cost(case, operation)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", nargs="?", type=Path, default=RESULTS_DIR)
    parser.add_argument("--write-readme", action="store_true")
    args = parser.parse_args()
    rows = load_results(args.directory)
    if args.write_readme:
        import contextlib
        import io
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            render(rows)
        readme = ROOT / "README.md"
        text = readme.read_text()
        prefix, _, previous = text.partition("# Benchmark\n")
        variants = "|".join(re.escape(v) for v in sorted({r["variant"] for r in rows}))
        match = re.search(r"^## (?:" + variants +
                          r"|Case-Sensitive .*|Case-Insensitive .*)$",
                          previous, re.MULTILINE)
        introduction = previous[:match.start()] if match else "\n"
        readme.write_text((prefix + "# Benchmark\n" + introduction.rstrip() +
                           "\n\n" + out.getvalue()).rstrip() + "\n")
    else:
        render(rows)
if __name__ == "__main__":
    main()
