#!/usr/bin/env python3
"""Compare benchmark results written by `make run` into results/.

Each result file is named <parser>-<variant>.txt and contains one line
per benchmark:

    <fixture>/<bytes> <samples> <iterations> <mean-ns> <stddev-ns>

The report is organized per variant: every variant found in the result
files gets its own set of tables (time, throughput) containing the
parsers measured under that build configuration, with ratios relative to
hwire under the same variant. Files without a known variant suffix form
a "default" group.
"""

import sys
from pathlib import Path

RESULTS_DIR = Path(__file__).resolve().parent / "results"
PLATFORM_FILE = RESULTS_DIR / "platform.txt"

CANONICAL_VARIANTS = ["nosimd", "sse2", "neon", "sse42", "avx2"]

def load_results(directory: Path) -> dict[str, dict[str, dict]]:
    """row name -> fixture -> {bytes, samples, iterations, mean, stddev}."""
    data: dict[str, dict[str, dict]] = {}
    for path in sorted(directory.glob("*.txt")):
        if path.name == PLATFORM_FILE.name:
            continue  # environment info, not benchmark results
        row: dict[str, dict] = {}
        for line in path.read_text().splitlines():
            fields = line.split()
            if not fields:
                continue
            name, _, size = fields[0].rpartition("/")
            entry = {"bytes": int(size) if size else None}
            try:
                values = [float(v) for v in fields[1:]]
                if len(values) == 1:
                    entry.update(samples=None, iterations=None,
                                 mean=values[0], stddev=0.0)
                else:
                    (entry["samples"], entry["iterations"], entry["mean"],
                     entry["stddev"]) = values
            except ValueError:
                sys.exit(f"{path.name}: unparsable line: {line!r}")
            row[name] = entry
        if row:
            data[path.stem] = row
    return data


def row_sort_key(name: str) -> tuple:
    # hwire first (it is the library under test), others alphabetically
    return (0 if name == "hwire" else 1, name)


def split_variant(name: str) -> tuple[str, str]:
    """'picohttpparser-sse42' -> ('picohttpparser', 'sse42')."""
    base, _, variant = name.rpartition("-")
    if base and variant in CANONICAL_VARIANTS:
        return base, variant
    return name, "default"


def human_throughput(nbytes: float, ns: float) -> str:
    bps = nbytes / (ns * 1e-9)
    for unit, scale in (("GB/s", 1e9), ("MB/s", 1e6), ("kB/s", 1e3)):
        if bps >= scale:
            return f"{bps / scale:.2f} {unit}"
    return f"{bps:.2f} B/s"


def render(title: str, header: list[str], rows: list[list[str]]) -> None:
    widths = [max(len(header[i]), *(len(r[i]) for r in rows))
              for i in range(len(header))]
    line = "  ".join(h.ljust(w) for h, w in zip(header, widths))
    print(title)
    print(line)
    print("-" * len(line))
    for row in rows:
        print("  ".join(c.ljust(w) for c, w in zip(row, widths)))
    print()


def print_platform() -> None:
    """Show the environment that produced the results (collected at
    measurement time by platform.sh), if it is available."""
    if not PLATFORM_FILE.is_file():
        return
    print("platform")
    print("--------")
    for line in PLATFORM_FILE.read_text().splitlines():
        print(f"  {line}")
    print()


def main() -> None:
    if not RESULTS_DIR.is_dir():
        sys.exit(f"no results directory at {RESULTS_DIR} (run `make run` first)")

    data = load_results(RESULTS_DIR)
    if not data:
        sys.exit(f"no result files in {RESULTS_DIR} (run `make run` first)")

    print_platform()

    # Group rows per variant; order variants canonically, "default" first.
    groups: dict[str, list[str]] = {}
    for name in data:
        groups.setdefault(split_variant(name)[1], []).append(name)
    variant_order = ([v for v in CANONICAL_VARIANTS if v in groups]
                     + [v for v in sorted(groups) if v not in CANONICAL_VARIANTS
                        and v != "default"])
    if "default" in groups:
        variant_order = ["default"] + variant_order
    for rows in groups.values():
        rows.sort(key=row_sort_key)

    # Fixture byte sizes (per fixture, from whichever row has them)
    sizes = {}
    for row in data:
        for fixture, entry in data[row].items():
            if entry["bytes"]:
                sizes.setdefault(fixture, entry["bytes"])

    for variant in variant_order:
        rows = groups[variant]
        baseline = next((r for r in rows if r == f"hwire-{variant}"), rows[0])

        fixtures: list[str] = []
        for row in rows:
            for fixture in data[row]:
                if fixture not in fixtures:
                    fixtures.append(fixture)

        print(f"===== variant: {variant} "
              f"(base = {baseline}) " + "=" * max(0, 40 - len(variant)))
        print()

        # time per message
        out = []
        for row in rows:
            cells = [row]
            for fixture in fixtures:
                entry = data[row].get(fixture)
                if entry is None:
                    cells.append("-")
                    continue
                cell = f"{entry['mean']:.1f} ±{entry['stddev']:.1f}"
                if len(rows) > 1:
                    base = data[baseline].get(fixture)
                    if base:
                        cell += (f" (x{entry['mean'] / base['mean']:.2f})"
                                 if row != baseline else " (base)")
                cells.append(cell)
            out.append(cells)
        render("time per message, ns (lower is better)",
               ["parser"] + [f"{f} ({sizes[f]}B)" if f in sizes else f
                             for f in fixtures], out)

        # throughput
        out = []
        for row in rows:
            cells = [row]
            for fixture in fixtures:
                entry = data[row].get(fixture)
                if entry is None or not entry["bytes"]:
                    cells.append("-")
                else:
                    cells.append(human_throughput(entry["bytes"],
                                                  entry["mean"]))
            out.append(cells)
        render("throughput (higher is better)", ["parser"] + fixtures, out)

    # measurement note (identical for the shared driver)
    first = next(iter(next(iter(data.values())).values()))
    if first.get("samples"):
        print(f"measurement: {int(first['samples'])} samples x "
              f"{int(first['iterations'])} iterations per benchmark")
    print("(xN.NN = N.NN times the base parser's time, same variant)")


if __name__ == "__main__":
    main()
