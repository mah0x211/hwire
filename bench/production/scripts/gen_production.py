#!/usr/bin/env python3
"""Discover adapters and generate their declarations and registration table."""
import sys
import re
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "shared/scripts"))
from registration import registered, write_generated

ROOT = Path(__file__).resolve().parents[1]
SUFFIX = "_with_store"
REQUIRED = ("header_lookup",)
PREPARED = ("header_query_new", "header_lookup_prepared", "header_query_free")


def write_table(out, variant, directory=None, implementations=None):
    names = list(registered(directory or ROOT, SUFFIX, REQUIRED, directions=("request",)))
    if implementations is not None:
        names = [name for name in names if name in implementations]
    lines = []
    prepared = set()
    split = set()
    for name in names:
        text = ((directory or ROOT) / name / "request.c").read_text()
        present = [bool(re.search(r"\b" + name + "_" + symbol + r"\s*\(", text))
                   for symbol in PREPARED]
        if all(present):
            prepared.add(name)
        elif any(present):
            raise ValueError(f"{name}: prepared lookup requires all three entry points")

        lines.append(f"int {name}_request{SUFFIX}(void **, const unsigned char *, size_t, size_t);")
        if re.search(r"\b" + name + r"_request_with_store_split\s*\(", text):
            split.add(name)
            lines.append(f"int {name}_request_with_store_split(void **, const unsigned char *, size_t, size_t, size_t);")
        lines.append(f"void {name}_context_free(void *);")
        lines.append(f"size_t {name}_header_lookup(const void *, const char *, size_t);")
        if name in prepared:
            lines.extend((f"void *{name}_header_query_new(const char *, size_t);",
                          f"size_t {name}_header_lookup_prepared(const void *, const void *);",
                          f"void {name}_header_query_free(void *);"))
    lines.append("static const implementations_t implementations[] = {")
    for name in names:
        display = name
        fields = [f'.name = "{display}"', f".request = {name}_request{SUFFIX}",
                  f".context_free = {name}_context_free"]
        fields.append(f".header_lookup = {name}_header_lookup")
        if name in split:
            fields.append(f".request_split = {name}_request_with_store_split")
        if name in prepared:
            fields.extend(f".{symbol} = {name}_{symbol}" for symbol in PREPARED)
        lines.append("{ " + ", ".join(fields) + " },")
    lines.append("};")
    lines.append(f'#define BENCH_VARIANT "{variant}"')
    write_generated(out, lines)


if __name__ == "__main__":
    if sys.argv[1] == "list":
        print(" ".join(registered(ROOT, SUFFIX, REQUIRED, directions=("request",))))
    elif sys.argv[1] == "write":
        write_table(sys.argv[2], sys.argv[3], implementations=sys.argv[4:] or None)
    else:
        sys.exit("usage: gen_registration.py list | write OUT VARIANT")
