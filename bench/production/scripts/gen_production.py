#!/usr/bin/env python3
"""Discover adapters and generate their declarations and registration table."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "shared/scripts"))
from registration import registered

ROOT = Path(__file__).resolve().parents[1]
SUFFIX = "_with_store"
REQUIRED = ("header_lookup",)


def write_table(out, variant, directory=None, implementations=None):
    names = list(registered(directory or ROOT, SUFFIX, REQUIRED, directions=("request",)))
    if implementations is not None:
        names = [name for name in names if name in implementations]
    lines = []
    for name in names:
        lines.append(f"int {name}_request{SUFFIX}(void **, const unsigned char *, size_t, size_t);")
        lines.append(f"void {name}_context_free(void *);")
        lines.append(f"size_t {name}_header_lookup(const void *, const char *, size_t);")
    lines.append("static const implementations_t implementations[] = {")
    for name in names:
        display = name
        fields = [f'.name = "{display}"', f".request = {name}_request{SUFFIX}",
                  f".context_free = {name}_context_free"]
        fields.append(f".header_lookup = {name}_header_lookup")
        lines.append("{ " + ", ".join(fields) + " },")
    lines.append("};")
    lines.append(f'#define BENCH_VARIANT "{variant}"')
    content = "\n".join(lines) + "\n"
    path = Path(out)
    if not path.exists() or path.read_text() != content:
        path.write_text(content)


if __name__ == "__main__":
    if sys.argv[1] == "list":
        print(" ".join(registered(ROOT, SUFFIX, REQUIRED, directions=("request",))))
    elif sys.argv[1] == "write":
        write_table(sys.argv[2], sys.argv[3], implementations=sys.argv[4:] or None)
    else:
        sys.exit("usage: gen_registration.py list | write OUT VARIANT")
