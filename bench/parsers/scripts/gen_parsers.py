#!/usr/bin/env python3
"""Discover adapters and generate their declarations and registration table."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "shared/scripts"))
from registration import registered, write_generated

ROOT = Path(__file__).resolve().parents[1]
SUFFIX = ""
REQUIRED = ()


def write_table(out, variant, directory=None, implementations=None):
    names = list(registered(directory or ROOT, SUFFIX, REQUIRED))
    if implementations is not None:
        names = [name for name in names if name in implementations]
    lines = []
    for name in names:
        for direction in ("request", "response"):
            lines.append(f"int {name}_{direction}{SUFFIX}(void **, const unsigned char *, size_t);")
        lines.append(f"void {name}_context_free(void *);")

    lines.append("static const parsers_t parsers[] = {")
    for name in names:
        display = f"{name}-{variant}"
        fields = [f'.name = "{display}"', f".request = {name}_request{SUFFIX}",
                  f".response = {name}_response{SUFFIX}", f".context_free = {name}_context_free"]

        lines.append("{ " + ", ".join(fields) + " },")
    lines.append("};")

    write_generated(out, lines)


if __name__ == "__main__":
    if sys.argv[1] == "list":
        print(" ".join(registered(ROOT, SUFFIX, REQUIRED)))
    elif sys.argv[1] == "write":
        write_table(sys.argv[2], sys.argv[3], implementations=sys.argv[4:] or None)
    else:
        sys.exit("usage: gen_registration.py list | write OUT VARIANT")
