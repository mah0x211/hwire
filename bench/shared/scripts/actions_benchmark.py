#!/usr/bin/env python3
"""Run existing suites and publish their reports without updating README files."""
import argparse
from datetime import datetime, timezone
import os
from pathlib import Path
import shutil
import subprocess

SUITES = ("parsers", "hashmaps", "production")


def append_summary(path, text):
    with path.open("a", encoding="utf-8") as stream:
        stream.write(text + "\n")


def metadata(source):
    commit = subprocess.check_output(
        ["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    lines = [f"commit: {commit}", f"date: {datetime.now(timezone.utc).isoformat()}"]
    commands = [["uname", "-a"], ["lscpu"], [os.getenv("CC", "cc"), "--version"],
                [os.getenv("CXX", "c++"), "--version"], ["make", "--version"],
                ["python3", "--version"], ["rustc", "--version"], ["cargo", "--version"]]
    if shutil.which("zig"):
        commands.append(["zig", "version"])
    for command in commands:
        lines.append("$ " + " ".join(command))
        lines.append(subprocess.check_output(command, text=True).strip())
    return commit, "\n".join(lines) + "\n"


def run_suite(source, suite, output, summary):
    directory = source / "bench" / suite
    output.mkdir(parents=True)
    if not (directory / "Makefile").is_file():
        append_summary(summary, f"**{suite}: failed** — this revision has no suite Makefile.")
        (output / "status.txt").write_text("Missing suite Makefile\n")
        return 1

    print(f"=== {suite}: build and measure ===", flush=True)
    with (output / "run.log").open("w") as log:
        with subprocess.Popen(["make", "--no-print-directory", "run"],
                              cwd=directory, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True) as process:
            for line in process.stdout:
                print(line, end="", flush=True)
                log.write(line)
            code = process.wait()
    results = directory / "results"
    if results.is_dir():
        shutil.copytree(results, output / "results")
    if code == 0:
        with (output / "report.md").open("w") as report, (output / "report.log").open("w") as log:
            code = subprocess.run(["make", "--no-print-directory", "--silent", "report"],
                                  cwd=directory, stdout=report, stderr=log).returncode
    (output / "status.txt").write_text(f"Exit status: {code}\n")
    if code:
        append_summary(summary, f"**{suite}: failed (exit {code})** — see the artifact logs.")
        return code

    append_summary(summary, f"\n# {suite.capitalize()}\n")
    platform = results / ("storage/platform.txt" if suite == "hashmaps" else "platform.txt")
    if platform.is_file():
        append_summary(summary, "<details>\n<summary>Build configuration</summary>\n\n```text\n" +
                       platform.read_text() + "```\n\n</details>\n")
    append_summary(summary, (output / "report.md").read_text())
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--suite", choices=("all", *SUITES), required=True)
    parser.add_argument("--label", choices=("measured", "comparison"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    args.output.mkdir(parents=True)
    commit, environment = metadata(source)
    (args.output / "environment.txt").write_text(environment)
    append_summary(args.summary, f"# {args.label.capitalize()} revision: `{commit}`\n\n"
                   "Reports use the existing suite workloads and sampling. "
                   "Results from different runs may use different runner hardware.\n\n"
                   "<details>\n<summary>Commit, toolchains and CPU</summary>\n\n```text\n" +
                   environment + "```\n\n</details>\n")
    for suite in SUITES if args.suite == "all" else (args.suite,):
        code = run_suite(source, suite, args.output / suite, args.summary)
        if code:
            return code
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
