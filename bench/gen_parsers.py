#!/usr/bin/env python3
"""Register parsers and generate the parser-table source (invoked by the
Makefile).

The script owns the parsers/ directory: it discovers registrations (a
parsers/<name>/ directory with both request.c and response.c adapters),
validates the names (C identifiers: adapters are compiled as
<name>_request / <name>_response), and writes the generated table.

Subcommands:

  list             print the registered parser names (invalid names are
                   skipped with a warning on stderr)
  write OUT VARIANT  write the parser-table source to OUT (adapter
                   declarations plus parsers[] with entries named
                   "<parser>-<variant>")
"""

import re
import sys
from pathlib import Path

PARSERS_DIR = Path(__file__).resolve().parent / "parsers"
IDENT = re.compile(r"[A-Za-z][A-Za-z0-9_]*\Z")


def registered():
    """Yield the valid parser names found under parsers/."""
    rule = "name must be a C identifier: [A-Za-z][A-Za-z0-9_]*"
    for entry in sorted(PARSERS_DIR.iterdir()):
        if not (entry.is_dir()
                and (entry / "request.c").is_file()
                and (entry / "response.c").is_file()):
            continue
        if IDENT.match(entry.name):
            yield entry.name
        else:
            print(f"warning: parser '{entry.name}' ignored ({rule})",
                  file=sys.stderr)


def write_table(out: str, variant: str) -> None:
    names = list(registered())
    lines = []
    for name in names:
        lines.append(f"int {name}_request(const unsigned char *, size_t);")
        lines.append(f"int {name}_response(const unsigned char *, size_t);")
    lines.append("static const parsers_t parsers[] = {")
    for name in names:
        lines.append(f'{{ .name = "{name}-{variant}", '
                     f".request = {name}_request, "
                     f".response = {name}_response, }},")
    lines.append("};")
    with open(out, "w") as f:
        f.write("\n".join(lines) + "\n")


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    cmd, args = sys.argv[1], sys.argv[2:]

    if cmd == "list":
        print(" ".join(registered()))
    elif cmd == "write":
        write_table(args[0], args[1])
    else:
        sys.exit(f"gen_parsers.py: unknown subcommand: {cmd}")


if __name__ == "__main__":
    main()
