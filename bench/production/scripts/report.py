#!/usr/bin/env python3
"""Report Parse and native-storage lookup costs for HTTP request scenarios."""
import csv
import json
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "shared/scripts"))
from report_common import render, message_section, environment

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
            # Calculated totals use exactly the means displayed in individual tables.
            entry = dict(row, label=label, mean=round(float(row["mean_ns"]), 2),
                         stddev=float(row["stddev_ns"]), rciw=float(row["rciw"]))
            data.setdefault(variant, {}).setdefault(key, {}).setdefault(name, {})[row["operation"]] = entry
    return data


def measurement_table(group, operation, title, unit):
    rows = sorted((results[operation] for results in group.values()), key=lambda row: row["mean"])
    fastest = rows[0]["mean"]
    render(title, ["Implementation", f"Mean ± SD ({unit})", "Relative", "Throughput", "RCIW"],
           [[row["label"], f'{row["mean"]:.2f} ±{row["stddev"]:.2f}' +
             (" †" if row["rciw"] > 0.02 else ""),
             f'{row["mean"]/fastest:.2f}×',
             f'{1000/row["mean"]:.2f} M {"req/s" if operation == "parse" else "lookups/s"}',
             f'{100*row["rciw"]:.2f}%'] for row in rows], level=4)


def total_table(group, operation, title):
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
    render(f"Parse + Post-process + {title} (calculated)",
           ["Implementation", f"Parse + Post-process + 1 {lookup_name} (ns)", "Relative",
            f"{lookup_name} Mean ± SD (ns/lookup)", "Lookups to beat baseline"], table, level=4)
    print(f"\nParse + Post-process + 1 lookup baseline: {base['parse']['label']}. Each Relative uses the fastest total in its column.\n"
          "No crossover: it cannot overtake under this model. "
          "The crossover is the first integer Q giving a strictly lower total.")


def main(include_environment=True):
    data = load_results(RESULTS_DIR)
    if not data:
        sys.exit("no HTTP storage results (run make)")
    if include_environment:
        environment(RESULTS_DIR / "platform.txt")
    for variant, fixtures in data.items():
        for key, group in sorted(fixtures.items()):
            fixture, header_limit = key
            message_section(fixture)
            policy = next(iter(group.values()))["parse"]["allocation"]
            # Keep the allocation boundary explicit rather than saying
            # "preallocated allocation" in the published report.
            allocation = ("memory preallocated (system allocation excluded)"
                          if policy == "preallocated" else "system allocation included")
            print(f"\n{variant} CPU build; {allocation}; application header limit {header_limit}; headers only.\n")
            print("Parse + Post-process includes initialization, HTTP parsing and native header storage, "
                  "including any growth. Query decomposition, decoding and storage are excluded. "
                  "Input preparation and context cleanup are outside timing.\n")
            print("† Target RCIW was not reached; calculated totals inherit the marker from either component.\n")
            measurement_table(group, "parse", "Parse + Post-process", "ns/request")
            print("\nKnown/unknown describes native header-name definitions. Unknown hit names are present "
                  "in the message but absent from those definitions; hwire_table treats all names as strings. "
                  "Key lengths differ between groups, so costs include both name representation and key length. "
                  "Each lookup starts from the original string; native name conversion is timed. "
                  "Time is the mean per lookup for these keys on a completed warm context.")
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
            for operation, title, keys in cases:
                measurement_table(group, operation, title, "ns/lookup")
                print(f"\nSearches {keys}, in that order, repeated with equal frequency.")
            print("\n\n### First Header Lookup Cost and Break-even\n")
            print("Estimate the total time to parse and store a request and perform its first header lookup. "
                  "Each table shows that total, the per-lookup cost, and how many lookups are needed "
                  "for faster searches to recover a higher parsing and storage cost.\n")
            print("Totals use the displayed means: "
                  "`Parse + Post-process mean + Q × lookup mean`. "
                  "Lookup costs are measured on a completed warm context; the first-lookup total is estimated, "
                  "not timed immediately after parsing. "
                  "The crossover is the first integer Q that beats the fastest Parse + Post-process + 1 lookup implementation.")
            for operation, title, _ in cases:
                total_table(group, operation, title)



if __name__ == "__main__":
    main()
