#!/usr/bin/env python3
"""Register message fixtures and generate the fixture-table source
(invoked by the Makefile).

The script owns the data/ directory: requests live in
data/request/*.h and responses in data/response/*.h, each
defining `static const unsigned char MSG_<NAME>[]` (the symbol is
derived from the file name). Names must be lowercase [a-z][a-z0-9_]*;
the directory selects the direction, and the generated table entries
carry a "req_"/"rsp_" display prefix so result files stay
self-describing.

Subcommands:

  list       print the registered fixture stems (invalid names are
             skipped with a warning on stderr)
  write OUT  write the fixture-table source to OUT (message includes
             plus REQUESTS[]/RESPONSES[])
"""

import ast
import re
import sys
from pathlib import Path
from registration import write_generated

BASE = Path(__file__).resolve().parents[2] / "data"
NAME = re.compile(r"[a-z][a-z0-9_]*\Z")

DIRECTIONS = (
    ("request", "REQUESTS", "req"),
    ("response", "RESPONSES", "rsp"),
)


def registered(subdir: str):
    """Yield the valid fixture stems found under data/<subdir>/."""
    rule = "name must be [a-z][a-z0-9_]*"
    directory = BASE / subdir
    if not directory.is_dir():
        return
    for path in sorted(directory.glob("*.h")):
        if NAME.match(path.stem):
            yield path.stem
        else:
            print(f"warning: message fixture '{subdir}/{path.stem}' ignored"
                  f" ({rule})", file=sys.stderr)


def fixture_info(name: str) -> dict:
    """Read presentation metadata and the actual input string from a fixture."""
    prefix, _, stem = name.partition("_")
    subdir = {"req": "request", "rsp": "response", "scenario": "scenarios"}[prefix]
    source = (BASE / subdir / (stem + ".h")).read_text()
    literal = r'"(?:\\.|[^"\\])*"'
    declaration = re.search(
        r'MSG_' + stem.upper() + r'\[\]\s*=\s*((?:' + literal + r'\s*)+);', source)
    message = "".join(ast.literal_eval(token)
                      for token in re.findall(literal, declaration[1]))
    title = re.search(r"@title (.*)", source)
    description = re.search(r"@description (.*)", source)
    return {"title": title[1] if title else stem.replace("_", " ").title(),
            "description": description[1] if description else "",
            "message": message}


def write_table(out: str) -> None:
    lines = []
    tables = {}
    for subdir, table, prefix in DIRECTIONS:
        names = list(registered(subdir))
        tables[table] = names
        lines.extend(f'#include "../data/{subdir}/{name}.h"' for name in names)
    for (subdir, table, prefix), names in zip(DIRECTIONS, tables.values()):
        lines.append(f"static const fixture_t {table}[] = {{")
        for name in names:
            sym = f"MSG_{name.upper()}"
            lines.append(f'{{ "{prefix}_{name}", {sym}, sizeof({sym}) - 1 }},')
        lines.append("};")
    write_generated(out, lines)


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd, args = sys.argv[1], sys.argv[2:]

    if cmd == "list":
        print(" ".join(
            name for subdir, _, _ in DIRECTIONS for name in registered(subdir)))
    elif cmd == "write":
        write_table(args[0])
    else:
        sys.exit(f"gen_fixtures.py: unknown subcommand: {cmd}")


if __name__ == "__main__":
    main()
