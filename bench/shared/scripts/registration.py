"""Discover active request/response adapters using their exported contracts."""
import re
import sys

IDENT = re.compile(r"[A-Za-z][A-Za-z0-9_]*\Z")


def registered(directory, suffix="", required=(), directions=("request", "response")):
    for entry in sorted(directory.iterdir()):
        if not entry.is_dir() or entry.name.startswith("_"):
            continue
        sources = [entry / (kind + ".c") for kind in directions]
        if not all(source.is_file() for source in sources):
            continue
        if not IDENT.fullmatch(entry.name):
            print(f"warning: '{entry.name}' ignored (name must be a C identifier)",
                  file=sys.stderr)
            continue
        texts = [source.read_text() for source in sources]
        if not all(re.search(r"\b" + entry.name + "_" + kind + suffix + r"\s*\(", text)
                   for kind, text in zip(directions, texts)):
            continue
        if not all(re.search(r"\b" + entry.name + "_" + name + r"\s*\(", "".join(texts))
                   for name in required):
            print(f"warning: '{entry.name}' ignored (missing required entry points)",
                  file=sys.stderr)
            continue
        yield entry.name
