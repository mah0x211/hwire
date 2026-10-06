#!/usr/bin/env python3
"""Discover adapters; generate direct lookup loops and registration."""
import re
import sys
from pathlib import Path
HASHMAPS_DIR = Path(__file__).resolve().parents[1]
IDENT = re.compile(r"[A-Za-z][A-Za-z0-9_]*\Z")

def implementation(entry):
    return [p for p in (entry / "hashmap.c", entry / "hashmap.cpp") if p.is_file()]

def registered(directory=None):
    root = directory or HASHMAPS_DIR
    for entry in sorted(root.iterdir()):
        if not entry.is_dir() or entry.name.startswith("_"):
            continue
        sources = implementation(entry)
        if len(sources) == 1 and IDENT.fullmatch(entry.name):
            yield entry.name

def source_paths(directory=None):
    root = directory or HASHMAPS_DIR
    for name in registered(root):
        yield implementation(root / name)[0]

def configurations(name, directory=None):
    root = directory or HASHMAPS_DIR
    source = implementation(root / name)[0].read_text()
    yield ""
    pattern = r"\b" + re.escape(name) + r"_hashmap_new_exact_([A-Za-z0-9_]+)\s*\("
    yield from sorted(set(re.findall(pattern, source)))

def registrations(directory=None):
    for name in registered(directory):
        for suffix in configurations(name, directory):
            yield name + ("_" + suffix if suffix else "")

def write_table(out, variant, directory=None):
    root = directory or HASHMAPS_DIR
    names = list(registered(root))
    lines, rows = [], []
    for name in names:
        for config in configurations(name, root):
            suffix = "_" + config if config else ""
            lines.append(f"const char *{name}_hashmap_name{suffix}(void);")
            for mode in ("exact", "ci", "growth_exact", "growth_ci"):
                lines.append(f"void *{name}_hashmap_new_{mode}{suffix}(size_t);")
        lines.extend([
            f"void {name}_hashmap_free(void *);",
            f"size_t {name}_hashmap_bytes(const void *);",
            f"double {name}_hashmap_loadfactor(const void *);",
            f"size_t {name}_hashmap_growths(const void *);",
            f"int {name}_hashmap_push(void *, const hwire_kv_pair_t *);",
        ])
        lines.append(f"BENCH_DIRECT_POPULATE({name}_populate, {name}_hashmap_push)")
        for mode in ("get", "get_ci"):
            lines.append(f"const hwire_kv_pair_t *{name}_hashmap_{mode}(const void *, const char *, size_t);")
            lines.append(f"BENCH_DIRECT_LOOKUP({name}_measure_{mode}, {name}_hashmap_{mode})")
        for config in configurations(name, root):
            suffix = "_" + config if config else ""
            rows.append(
                f'    {{"{name}{suffix}", {name}_hashmap_name{suffix}, '
                f'{name}_hashmap_new_exact{suffix}, {name}_hashmap_new_ci{suffix}, '
                f'{name}_hashmap_new_growth_exact{suffix}, {name}_hashmap_new_growth_ci{suffix}, '
                f'{name}_hashmap_free, {name}_hashmap_bytes, {name}_hashmap_loadfactor, '
                f'{name}_hashmap_growths, {name}_populate, '
                f'{name}_measure_get, {name}_measure_get_ci}},')
    lines.extend(["static const hashmap_t maps[] = {", *rows, "};",
                  f'#define BENCH_VARIANT "{variant}"'])
    p = Path(out)
    content = "\n".join(lines) + "\n"
    if not p.exists() or p.read_text() != content:
        p.write_text(content)

def main():
    args = sys.argv[1:]
    if args == ["list"]:
        print(" ".join(registered()))
    elif args == ["registrations"]:
        print(" ".join(registrations()))
    elif args == ["sources"]:
        print(" ".join(str(p.relative_to(HASHMAPS_DIR)) for p in source_paths()))
    elif args == ["deps"]:
        print(" ".join(str(p.relative_to(HASHMAPS_DIR)) for name in registered()
                       for p in sorted((HASHMAPS_DIR / name).rglob("*"))
                       if p.is_file() and p.suffix in (".h", ".c", ".cpp", ".mk", ".sh")))
    elif len(args) == 3 and args[0] == "write":
        write_table(args[1], args[2])
    else:
        sys.exit("usage: gen_hashmaps.py list|registrations|sources|deps | write OUT VARIANT")
if __name__ == "__main__":
    main()
