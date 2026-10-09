#!/usr/bin/env python3
"""Report Parse and native-storage lookup costs for HTTP request scenarios."""
import csv
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "shared/scripts"))
from report_common import report_failures, render, message_section, environment

RESULTS_DIR = Path(__file__).resolve().parents[1] / "results"


def load_results(directory):
    data = {}
    manifest = directory / "active.json"
    active = json.loads(manifest.read_text()) if manifest.exists() else None
    for path in sorted(directory.glob("*.csv")):
        name, _, variant = path.stem.rpartition("-")
        if active is not None and path.stem not in active:
            continue
        for row in csv.DictReader(path.read_text().splitlines()):
            key = (row["fixture"], int(row["header_capacity"]))
            label = active.get(path.stem, name) if isinstance(active, dict) else name
            target = {"nosimd": "scalar", "sse2": "SSE2", "sse42": "SSE4.2",
                      "neon": "NEON", "native": "native", "siphash": "native, SipHash"}.get(variant, variant)
            label += f" ({target})"
            # Calculated totals use exactly the means displayed in individual tables.
            entry = dict(row, label=label, mean=round(float(row["mean_ns"]), 2),
                         stddev=float(row["stddev_ns"]), rciw=float(row["rciw"]))
            data.setdefault(key, {}).setdefault(path.stem, {})[row["operation"]] = entry
    return data


def measurement_table(group, operation, title, unit):
    rows = sorted((results[operation] for results in group.values()), key=lambda row: row["mean"])
    fastest = rows[0]["mean"]
    render(title, ["Implementation", f"Mean ± SD ({unit})", "Relative", "Throughput", "RCIW"],
           [[row["label"], f'{row["mean"]:.2f} ±{row["stddev"]:.2f}' +
             (" †" if row["rciw"] > 0.02 else ""),
             f'{row["mean"]/fastest:.2f}×',
             f'{1000/row["mean"]:.2f} M {"req/s" if operation == "parse" else "lookups/s"}',
             f'{100*row["rciw"]:.2f}%'] for row in rows], level=3)


def split_table(group, operation, title):
    rows = sorted((results for results in group.values() if operation in results),
                  key=lambda results: results[operation]["mean"])
    if not rows:
        return
    fastest = rows[0][operation]["mean"]
    render(title, ["Implementation", "Mean ± SD (ns/request)", "Relative",
                   "Vs complete input", "RCIW"],
           [[results[operation]["label"],
             f'{results[operation]["mean"]:.2f} ±{results[operation]["stddev"]:.2f}' +
             (" †" if results[operation]["rciw"] > 0.02 else ""),
             f'{results[operation]["mean"]/fastest:.2f}×',
             f'{results[operation]["mean"]/results["parse"]["mean"]:.2f}×',
             f'{100*results[operation]["rciw"]:.2f}%'] for results in rows], level=3)


def total_table(group, operation, title, scenario=None):
    baseline = min(group, key=lambda name: group[name]["parse"]["mean"] + group[name][operation]["mean"])
    base = group[baseline]
    fastest_one = base["parse"]["mean"] + base[operation]["mean"]
    table = []
    for name, rows in sorted(group.items(), key=lambda item: item[1]["parse"]["mean"] + item[1][operation]["mean"]):
        parse = rows["parse"]["mean"]
        lookup = rows[operation]["mean"]
        if name == baseline:
            crossover = "Baseline"
        elif lookup < base[operation]["mean"]:
            parse_delta = round(parse * 100) - round(base["parse"]["mean"] * 100)
            lookup_delta = round(base[operation]["mean"] * 100) - round(lookup * 100)
            crossover = str(max(1, parse_delta // lookup_delta + 1))
        else:
            crossover = "No crossover"
        label = rows["parse"]["label"] + (" †" if rows["parse"]["rciw"] > 0.02 or rows[operation]["rciw"] > 0.02 else "")
        table.append([label, f"{parse + lookup:.2f}",
                      f"{(parse + lookup)/fastest_one:.2f}×",
                      f"{lookup:.2f} ±{rows[operation]['stddev']:.2f}", crossover])
    lookup_name = "Miss" if operation == "miss" else "Hit"
    heading = f"Parse + Post-process + {title} (calculated)"
    if scenario is not None:
        heading = f"{scenario} — {heading}"
    render(heading,
           ["Implementation", f"Parse + Post-process + 1 {lookup_name} (ns)", "Relative",
            f"{lookup_name} Mean ± SD (ns/lookup)", "Lookups to beat baseline"], table, level=3)
    print(f"\nParse + Post-process + 1 lookup baseline: {base['parse']['label']}. Each Relative uses the fastest total in its column.\n"
          "No crossover: it cannot overtake under this model. "
          "The crossover is the first integer Q giving a strictly lower total.")


def lookup_comparison(group, operation, parse_operation="parse"):
    comparison = {}
    for name, results in group.items():
        if parse_operation not in results:
            continue
        for prefix, method in (("", "string"), ("prepared_", "prepared")):
            lookup = results.get(prefix + operation)
            if lookup is None:
                continue
            label = lookup["label"] + f" ({method})"
            comparison[(name, method)] = {
                "parse": dict(results[parse_operation], label=label),
                operation: dict(lookup, label=label),
            }
    return comparison


def main(include_environment=True):
    data = load_results(RESULTS_DIR)
    if not data:
        if report_failures(RESULTS_DIR):
            return
        sys.exit("no HTTP storage results (run make)")
    if include_environment:
        environment(RESULTS_DIR / "platform.txt")
    # CSV insertion order follows the driver's fixture registration order.
    for position, (key, group) in enumerate(data.items()):
        if position > 0:
            print("\n\n---")
        fixture, header_limit = key
        message_section(fixture, level=2)
        policy = next(iter(group.values()))["parse"]["allocation"]
        # Keep the allocation boundary explicit rather than saying
        # "preallocated allocation" in the published report.
        allocation = ("memory preallocated (system allocation excluded)"
                      if policy == "preallocated" else "system allocation included")
        print(f"\nSupported CPU builds; {allocation}; application header limit {header_limit}; headers only.\n")
        print("Parse + Post-process includes initialization, HTTP parsing and native header storage, "
              "including any growth. Query decomposition, decoding and storage are excluded. "
              "Input preparation and context cleanup are outside timing.\n")
        print("† Target RCIW was not reached; calculated totals inherit the marker from either component.\n")
        measurement_table(group, "parse", "Parse + Post-process — Complete Input", "ns/request")
        has_split = any("parse_split_50" in results for results in group.values())
        if has_split:
            length = int(next(iter(group.values()))["parse"]["bytes"])
            print("""

### Incomplete-input handling

| Implementation | First call | Second call |
| --- | --- | --- |
| hwire + hwire_table | Parse the prefix and store completed headers through callbacks | Clear the partial table index and reparse accumulated input; callbacks run again |
| nginx | Retain parser state, buffer position and completed headers | Resume from the consumed position and append newly completed headers |
| H2O | Pass the prefix to picohttpparser; leave native header conversion until completion | Pass accumulated input and the previous length (`last_len`), then populate native storage once |
| Actix Web | Parse the prefix into temporary httparse state and stack headers | Recreate temporary parsing state, parse accumulated input and convert to native storage once |

Request initialization occurs once per message. Input copying, arena reset and
context cleanup remain outside timing; partial-index clearing and repeated
parsing are inside timing. The same complete-input baseline isolates the extra
cost of each retry strategy for this fixture. Authentication is not performed.
""")
        cases = (
            ("hit", "Lookup Hit — Known Headers",
             "Host, Accept, Cookie, User-Agent, Connection and Referer"),
            ("hit_unknown", "Lookup Hit — Unknown Headers",
             "Sec-Fetch-Site, Sec-Fetch-Mode, Sec-Fetch-User, Sec-Fetch-Dest, Sec-CH-UA and Sec-CH-UA-Platform"),
            ("hit_mixed", "Lookup Hit — Mixed Headers",
             "Host, Sec-Fetch-Site, Cookie, Sec-Fetch-Mode, Connection and Sec-CH-UA-Platform"),
            ("miss", "Lookup Miss",
             "Hots, Accpet, Cooxie, User-Agend, Sec-CH-UA-Platforn and Referef"),
        )
        if not has_split:
            for operation, title, keys in cases:
                measurement_table(lookup_comparison(group, operation), operation, title, "ns/lookup")
                print(f"\nSearches {keys}, in that order, repeated with equal frequency.")
        if has_split:
            print("\n\n### Two-call Input Scenarios\n")
            print(f"The same {length}-byte request is exposed in two calls, first through byte "
                  f"{length // 2} (50%) or {length * 9 // 10} (90%), then through the end. "
                  "Both attempts, required partial-storage reset and final post-processing are timed together. "
                  "No arrival delay, socket I/O or receive-buffer copying is measured. "
                  "Vs complete input compares each build with its own complete-input result. "
                  "These controlled scenarios do not imply a real-world fragmentation frequency.\n")
            split_table(group, "parse_split_50", "Split at 50% — Parse + Post-process")
            split_table(group, "parse_split_90", "Split at 90% — Parse + Post-process")
            parse_scenarios = (("parse_split_50", "50% split"),
                               ("parse_split_90", "90% split"))
        else:
            parse_scenarios = (("parse", "Complete input"),)
        for parse_operation, scenario in parse_scenarios:
            print(f"\n\n### First Header Lookup Cost and Break-even — {scenario}\n")
            print("Estimate the total time to parse and store a request and perform its first header lookup. "
                  "Each table shows that total, the per-lookup cost, and how many lookups are needed "
                  "for faster searches to recover a higher parsing and storage cost.\n")
            if has_split:
                print(f"The parsing component uses the displayed {scenario} Parse + Post-process mean: "
                      "the cumulative time for both attempts, required storage reset and final conversion. "
                      "Lookup costs reuse the displayed completed-context lookup means for this same input.\n")
            print("Totals use the displayed means: "
                  "`Parse + Post-process mean + Q × lookup mean`. "
                  "Lookup costs are measured on a completed warm context; the first-lookup total is estimated, "
                  "not timed immediately after parsing. "
                  "The crossover is the first integer Q that beats the fastest Parse + Post-process + 1 lookup implementation.")
            for operation, title, _ in cases:
                total_table(lookup_comparison(group, operation, parse_operation),
                            operation, title, scenario=scenario if has_split else None)

    report_failures(RESULTS_DIR)


if __name__ == "__main__":
    main()
