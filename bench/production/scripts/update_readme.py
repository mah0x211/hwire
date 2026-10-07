#!/usr/bin/env python3
"""Publish native-header processing results in this suite's README."""
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import re
import report
from report_common import update_environment


def main():
    readme = Path(__file__).resolve().parents[1] / "README.md"
    prefix = readme.read_text().split("<!-- production-results -->", 1)[0].rstrip()
    prefix = update_environment(prefix, report.RESULTS_DIR / "platform.txt")
    output = StringIO()
    with redirect_stdout(output):
        report.main(include_environment=False)
    readme.write_text(re.sub(r"\n{4,}", "\n\n\n", prefix + "\n\n<!-- production-results -->\n\n" + output.getvalue().lstrip().rstrip()) + "\n")


if __name__ == "__main__":
    main()
