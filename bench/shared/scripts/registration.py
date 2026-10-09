"""Discover active request/response adapters using their exported contracts."""
import os
import re
import sys
from pathlib import Path

IDENT = re.compile(r"[A-Za-z][A-Za-z0-9_]*\Z")


def adapter_directories(directory):
    """List enabled C-identifier directories, optionally selecting one adapter."""
    selected = os.getenv("BENCH_ADAPTER")
    for entry in sorted(directory.iterdir()):
        if not entry.is_dir() or entry.name.startswith("_"):
            continue
        if selected and entry.name != selected:
            continue
        if not IDENT.fullmatch(entry.name):
            print(f"warning: '{entry.name}' ignored (name must be a C identifier)",
                  file=sys.stderr)
            continue
        yield entry


def write_generated(out, lines):
    """Avoid recompilation when generated declarations have not changed."""
    content = "\n".join(lines) + "\n"
    path = Path(out)
    if not path.exists() or path.read_text() != content:
        path.write_text(content)


def registered(directory, suffix="", required=(), directions=("request", "response")):
    for entry in adapter_directories(directory):
        sources = [entry / (kind + ".c") for kind in directions]
        if not all(source.is_file() for source in sources):
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
