"""Replay only this fuzz run's corpora and publish separate LLVM coverage."""

import json
import os
import shutil
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCES = (ROOT / "src/hwire.c", ROOT / "src/hwire_table.c", ROOT / "src/hwire_table_aes.h")
PROFDATA = os.environ.get("LLVM_PROFDATA", "llvm-profdata")
COV = os.environ.get("LLVM_COV", "llvm-cov")


def run(command, log):
    """Save diagnostics and propagate replay/report failures."""
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, timeout=60)
    log.write_text(result.stderr)
    result.check_returncode()
    return result.stdout


def report(directory):
    directory = directory.resolve()
    output = directory / "report"
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True, exist_ok=True)
    manifest = directory / "coverage-runs.tsv"
    runs = []
    groups = {}
    errors = []
    if not manifest.exists():
        errors.append("No coverage-enabled fuzz run was started.")
    else:
        for record in manifest.read_text().splitlines():
            target, variant, seconds = record.split("\t")
            work = directory / target / variant
            status_file = work / "exit-code"
            status = "Incomplete (build/run failed)"
            if status_file.exists():
                code = int(status_file.read_text())
                status = "Passed" if code == 0 else "Failed (exit %d)" % code
            entry = {"target": target, "variant": variant, "seconds": int(seconds),
                     "status": status}
            runs.append(entry)
            if status != "Passed":
                continue
            try:
                # Plain filenames overwrite profiles rather than merging prior runs.
                raw = work / "coverage-replay.profraw"
                raw.unlink(missing_ok=True)
                env = dict(os.environ, LLVM_PROFILE_FILE=str(raw))
                with (work / "coverage-replay.log").open("w") as log:
                    subprocess.run([str(work / "fuzz"), str(work / "corpus"),
                                    "-runs=0", "-max_len=8192", "-timeout=2",
                                    "-rss_limit_mb=512"], env=env, stdout=log,
                                   stderr=log, check=True, timeout=60)
                if not raw.exists():
                    raise RuntimeError("Coverage profile was not produced")
                group = "table" if target == "table" else "parser"
                groups.setdefault((group, variant), []).append((target, work, raw))
            except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
                entry["status"] = "Coverage replay failed"
                errors.append("%s / %s: %s" % (target, variant, exc))

    if not runs and manifest.exists():
        errors.append("No fuzz target completed setup.")

    rows = []
    for (group, variant), members in sorted(groups.items()):
        work = output / (group + "-" + variant)
        work.mkdir(parents=True, exist_ok=True)
        profile = work / "coverage.profdata"
        objects = [str(members[0][1] / "fuzz")]
        objects += ["-object=" + str(member[1] / "fuzz") for member in members[1:]]
        source_files = [SOURCES[0]] if group == "parser" else list(SOURCES[1:])
        try:
            run([PROFDATA, "merge", "-sparse", *[str(m[2]) for m in members],
                 "-o", str(profile)], work / "merge.log")
            data = run([COV, "export", *objects, "-instr-profile=" + str(profile),
                        *map(str, source_files)], work / "export.log")
            (work / "coverage.json").write_text(data)
            before = len(rows)
            for file in json.loads(data)["data"][0]["files"]:
                if Path(file["filename"]).resolve() in source_files:
                    rows.append({"source": Path(file["filename"]).name,
                                 "variant": variant,
                                 "targets": [m[0] for m in members],
                                 "summary": file["summary"]})
            if len(rows) == before:
                raise ValueError("No production source coverage was found")
            run([COV, "show", *objects, "-instr-profile=" + str(profile),
                 "-format=html", "-show-branches=count",
                 "-output-dir=" + str(work / "html"), *map(str, source_files)],
                work / "html.log")
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            errors.append("%s / %s coverage report: %s" % (group, variant, exc))

    summary = ["## Fuzzing coverage", "",
               "Coverage comes only from replaying generated fuzz corpora; ordinary test profiles are excluded.",
               "Durations are per target/build, excluding compilation and coverage replay.",
               "Unlisted targets were not started after an earlier failure. Percentages are informational, not pass thresholds.",
               "", "### Runs", "",
               "| Target | Build | Seconds | Result |", "| --- | --- | ---: | --- |"]
    for item in runs:
        summary.append("| {target} | {variant} | {seconds} | {status} |".format(**item))
    summary += ["", "### Source coverage", "",
                "| Source | Build | Fuzz targets | Lines | Branches | Functions |",
                "| --- | --- | --- | ---: | ---: | ---: |"]
    for item in rows:
        metrics = item["summary"]
        summary.append("| %s | %s | %s | %.2f%% | %.2f%% | %.2f%% |" %
                       (item["source"], item["variant"], ", ".join(item["targets"]),
                        metrics["lines"]["percent"], metrics["branches"]["percent"],
                        metrics["functions"]["percent"]))
    if errors:
        summary += ["", "### Report errors", ""] + ["- " + e for e in errors]
    summary += ["", "Download the fuzz-coverage artifact and open a build's html/index.html to inspect uncovered code.", ""]
    text = "\n".join(summary)
    (output / "summary.md").write_text(text)
    (output / "summary.json").write_text(json.dumps({"runs": runs, "coverage": rows,
                                                    "errors": errors}, indent=2))
    step_summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if step_summary:
        with open(step_summary, "a") as stream:
            stream.write(text)
    print(text)
    return 1 if errors else 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: coverage.py <coverage-run-directory>")
    sys.exit(report(Path(sys.argv[1])))
