"""Apply line/branch exclusion comments to LLVM's LCOV export.

LLVM HTML and fuzz reports remain unfiltered. Keep the raw LCOV as evidence.
"""

from pathlib import Path
import sys


def filter_record(record):
    source = next(line[3:] for line in record if line.startswith("SF:"))
    lines = Path(source).read_text().splitlines()
    excluded = {i for i, line in enumerate(lines, 1) if "LCOV_EXCL_LINE" in line}
    branches = excluded | {
        i for i, line in enumerate(lines, 1) if "LCOV_EXCL_BR_LINE" in line
    }
    kept = []
    totals = {}
    removed = {"LF": 0, "LH": 0, "BRF": 0, "BRH": 0}
    for line in record:
        field = line.split(":", 1)[0]
        if field in removed:
            totals[field] = int(line.split(":", 1)[1])
            continue
        if line.startswith("DA:") and int(line[3:].split(",")[0]) in excluded:
            removed["LF"] += 1
            removed["LH"] += int(line.split(",")[1]) > 0
            continue
        if line.startswith("BRDA:") and int(line[5:].split(",")[0]) in branches:
            hits = line.rsplit(",", 1)[1]
            removed["BRF"] += 1
            removed["BRH"] += hits != "-" and int(hits) > 0
            continue
        kept.append(line)
    # Preserve LLVM's summary accounting for macro expansions and folded branches.
    for field, total in totals.items():
        kept.append("%s:%d" % (field, total - removed[field]))
    return kept


def filter_report(text):
    records = []
    for record in text.split("end_of_record"):
        lines = record.strip().splitlines()
        if lines:
            records.append("\n".join(filter_record(lines)) + "\nend_of_record\n")
    return "".join(records)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("Usage: filter_coverage.py <raw.info> <filtered.info>")
    Path(sys.argv[2]).write_text(filter_report(Path(sys.argv[1]).read_text()))
