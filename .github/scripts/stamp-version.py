"""Stamp a development source snapshot with its release version."""

import argparse
from pathlib import Path
import re


def stamp_version(root, version):
    """Validate CalVer and replace both development macros in src/hwire.h."""
    if not re.fullmatch(r"[0-9]{4}\.(?:0[1-9]|1[0-2])\.(?:0|[1-9][0-9]*)", version):
        raise ValueError("expected YYYY.MM.SEQUENCE, for example 2026.10.0")

    header = Path(root) / "src" / "hwire.h"
    text = header.read_text(encoding="utf-8")
    replacements = {
        '#define HWIRE_VERSION "development"': f'#define HWIRE_VERSION "{version}"',
        '#define HWIRE_VERSION_IS_DEVELOPMENT 1': '#define HWIRE_VERSION_IS_DEVELOPMENT 0',
    }
    lines = text.splitlines()
    for old in replacements:
        if lines.count(old) != 1:
            raise ValueError(f"expected exactly one development definition: {old}")
    for old, new in replacements.items():
        text = text.replace(old, new)
    header.write_text(text, encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", help="release version in YYYY.MM.SEQUENCE format")
    parser.add_argument("root", type=Path, help="source snapshot to stamp")
    args = parser.parse_args()
    try:
        stamp_version(args.root, args.version)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
