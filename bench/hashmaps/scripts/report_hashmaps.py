#!/usr/bin/env python3
"""Render saved storage scenarios; no container correctness checks."""
import argparse
import csv
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "shared/scripts"))
from report_common import environment, publish_readme, report_failures, render as render_table

ROOT = Path(__file__).resolve().parents[1]
RESULTS_DIR = ROOT / "results/storage"
SCENARIOS = ("reserved", "growth-allocated")
MODES = ("case-sensitive", "case-insensitive")
OPERATIONS = ("build", "insert", "hit", "miss")
METRIC_NOTES = {
    "memory": "Final live container/storage bytes; borrowed key/value contents and allocator metadata are excluded.",
    "build": "Initial acquisition and initialization plus all insertions; any expansion is included.",
    "insert": "Insertions only; initial setup is excluded and insertion-triggered expansion is included.",
    "hit": "Successful searches in the fully populated map, with equal frequency for each selected key.",
    "miss": "Unsuccessful searches in the fully populated map using the prepared miss keys.",
}

def load_results(directory):
    manifest = directory / "active.json"
    active = set(json.loads(manifest.read_text())) if manifest.exists() else None
    rows = []
    for path in sorted(directory.glob("*.csv")):
        name, _, variant = path.stem.rpartition("-")
        if active is not None and path.stem not in active:
            continue
        for row in csv.DictReader(path.read_text().splitlines()):
            row.update(variant=variant, id=name)
            for column in ("count", "samples", "iterations", "container_bytes", "growths"):
                row[column] = int(row[column])
            for column in ("mean_ns", "stddev_ns", "load_factor"):
                row[column] = float(row[column])
            # Use the same hundredth-nanosecond means as the displayed tables.
            row["mean_ns"] = round(row["mean_ns"], 2)
            row["rciw"] = float(row["rciw"]) if row["rciw"] else None
            rows.append(row)
    return rows

def table(headers, rows):
    render_table(None, list(map(str, headers)),
                 [list(map(str, row)) for row in rows])
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
            build_delta = round(build["mean_ns"] * 100) - round(reference_build * 100)
            lookup_delta = round(reference_lookup * 100) - round(lookup["mean_ns"] * 100)
            crossover = f"{max(1, build_delta // lookup_delta + 1):,}"
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
                metrics = ("memory", *OPERATIONS)
                counts = sorted({r["count"] for r in group})
                for metric in metrics:
                    print(f"\n### {metric.capitalize()}\n")
                    print(METRIC_NOTES[metric] + "\n")
                    for count in counts:
                        print(f"**{count} keys**\n")
                        case = [r for r in group if r["count"] == count]
                        if metric == "memory":
                            render_memory(case, count, scenario)
                        else:
                            render_operation(case, metric)
                print("\n### First Lookup Cost and Break-even\n")
                print("Estimate the total time to build and populate a map and perform its first key lookup. "
                      "Each table shows that total, the per-lookup cost, and how many lookups are needed "
                      "for faster searches to recover a higher construction cost.\n")
                print("Totals use the displayed means: `Build + Q × lookup mean`. "
                      "Hit and Miss are shown separately; for hit fraction p, the combined estimate is "
                      "`Build + Q × (p × hit + (1 − p) × miss)`. "
                      "Lookup costs are measured on a fully populated warm map; "
                      "the first-lookup total is estimated, not timed immediately after construction.\n")
                print("Each table uses the fastest Build + 1 lookup as its baseline. "
                      "The crossover is the first integer Q that beats that baseline; "
                      "No crossover means it cannot overtake under this model. "
                      "† marks an input with unmet Target RCIW; small timing differences make "
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
        readme = ROOT / "README.md"
        text = readme.read_text()
        prefix, _, previous = text.partition("# Benchmark\n")
        variants = "|".join(re.escape(v) for v in sorted({r["variant"] for r in rows}))
        match = re.search(r"^## (?:" + variants +
                          r"|Case-Sensitive .*|Case-Insensitive .*)$",
                          previous, re.MULTILINE)
        introduction = previous[:match.start()] if match else "\n"
        prefix += "# Benchmark\n" + introduction.rstrip()
        publish_readme(readme, prefix, args.directory / "platform.txt",
                       lambda: render(rows))
    else:
        environment(args.directory / "platform.txt")
        render(rows)
        report_failures(args.directory)
if __name__ == "__main__":
    main()
