#!/usr/bin/env python3
"""Publish this suite's saved results and environment in its README."""
from pathlib import Path
import report
from report_common import publish_readme


def main():
    readme = Path(__file__).resolve().parents[1] / "README.md"
    prefix = readme.read_text().split("<!-- production-results -->", 1)[0]
    publish_readme(readme, prefix, report.RESULTS_DIR / "platform.txt",
                   lambda: report.main(include_environment=False), separator="\n\n<!-- production-results -->\n\n")


if __name__ == "__main__":
    main()
