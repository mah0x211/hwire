#!/usr/bin/env python3
"""Build and measure adapters independently, then report successes and failures."""
import argparse
import json
import os
from pathlib import Path
import shutil
import shlex
import signal
import subprocess
import tempfile

GENERATORS = {"parsers": "gen_parsers.py", "production": "gen_production.py",
              "hashmaps": "gen_hashmaps.py"}
BINARIES = {"parsers": "bench_parsers", "production": "bench_production",
            "hashmaps": "bench_hashmap"}


def execute(command, cwd, log, env=None):
    """Stream complete output into a retained log and return an error excerpt."""
    with log.open("a") as stream:
        print("$ " + " ".join(map(str, command)), flush=True)
        stream.write("$ " + " ".join(map(str, command)) + "\n")
        try:
            with subprocess.Popen(command, cwd=cwd, env=env, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True,
                                  errors="replace") as process:
                for line in process.stdout:
                    print(line, end="", flush=True)
                    stream.write(line)
                code = process.wait()
        except OSError as error:
            stream.write(str(error) + "\n")
            code = 1
    output = log.read_text(errors="replace").splitlines()
    errors = [line for line in output if any(word in line.lower()
              for word in ("fatal error", "error:", "missing", "requires", "not found"))]
    excerpt = "\n".join((errors or [line for line in output if line.strip()])[-3:])
    if code < 0:
        excerpt = f"Process terminated by {signal.Signals(-code).name}.\n" + excerpt
    return code, excerpt[-1000:]


def run(suite, variants=(), adapters=(), arguments=(), install=False, action="run"):
    suite = Path(suite).resolve()
    kind = suite.name
    generator = suite / "scripts" / GENERATORS[kind]
    names = subprocess.check_output(["python3", str(generator), "list"], text=True).split()
    if adapters:
        unknown = set(adapters) - set(names)
        if unknown:
            raise ValueError("unregistered adapters: " + ", ".join(sorted(unknown)))
        names = [name for name in names if name in adapters]
    results = (suite / "results" / ("storage" if kind == "hashmaps" else "")
               if action == "run" else suite / "bin/status" / action)
    if results.exists():
        shutil.rmtree(results)
    logs = results / "logs"
    logs.mkdir(parents=True)
    states, active, build_info = [], {}, []
    platform_config = None

    def record(name, variant, phase, code, log, error=""):
        states.append(dict(adapter=name, variant=variant, phase=phase,
                           exit_code=code, log=str(log.relative_to(results)),
                           error=error if code else ""))
        (results / "status.json").write_text(json.dumps(states, indent=2) + "\n")
        print(f"{name}/{variant}: {phase} {'FAILED' if code else 'completed'}", flush=True)

    for name in names:
        env = dict(os.environ, BENCH_ADAPTER=name, INSTALL_DEPS="1" if install else "0")
        selector = [f"IMPLEMENTATIONS={name}", f"MAPS={name}", f"ADAPTER={name}",
                    f"BIN=bin/{name}"]
        log = logs / f"{name}-configure.log"
        try:
            config = subprocess.check_output(["make", "--no-print-directory", "--silent",
                                              "benchmark-config", *selector],
                                             cwd=suite, env=env, text=True,
                                             stderr=subprocess.STDOUT).splitlines()
            supported, label, cc, cflags, cxx, cxxflags, environment = config
            env.update(assignment.split("=", 1) for assignment in shlex.split(environment))
            selected = [v for v in supported.split() if not variants or v in variants]
        except (subprocess.CalledProcessError, ValueError) as error:
            output = getattr(error, "output", str(error))
            log.write_text(output)
            record(name, "all", "configure", 1, log, output[-1000:])
            continue
        if not selected:
            continue
        if platform_config is None:
            platform_config = (cc, cflags, cxx, cxxflags)
        failed = False
        for phase, script in (("setup", "setup.sh"), ("fetch", "fetch.sh")):
            if not (suite / name / script).is_file():
                continue
            log = logs / f"{name}-{phase}.log"
            command = ["sh", str(suite / name / script)]
            if phase == "setup":
                command.append("install" if install else "check")
            code, error = execute(command, suite, log, env)
            if code:
                for variant in selected:
                    record(name, variant, phase, code, log, error)
                failed = True
                break
        if failed:
            continue
        info_log = logs / f"{name}-info.log"
        try:
            info = subprocess.check_output(["make", "--no-print-directory", "--silent",
                                             "benchmark-info", *selector], cwd=suite,
                                            env=env, text=True, stderr=subprocess.STDOUT)
            info_log.write_text(info)
            details = info.splitlines()
            if details and details[0]:
                build_info.append(f"{name} build: {details[0]}")
            build_info.extend(details[1:])
        except subprocess.CalledProcessError as error:
            info_log.write_text(error.output)
            for variant in selected:
                record(name, variant, "configure", error.returncode, info_log, error.output[-1000:])
            continue
        if action == "setup":
            record(name, "all", "setup", 0, info_log)
            continue
        for variant in selected:
            log = logs / f"{name}-{variant}.log"
            command = ["make", "--no-print-directory", "build-adapter", *selector,
                       f"VARIANTS={variant}", "SKIP_SETUP=1"]
            code, error = execute(command, suite, log, env)
            if code:
                record(name, variant, "build", code, log, error)
                continue
            if action == "build":
                record(name, variant, "build", 0, log)
                continue
            binary = suite / "bin" / name / variant / BINARIES[kind]
            if kind == "hashmaps":
                info_log = logs / f"{name}-{variant}-info.log"
                try:
                    description = subprocess.check_output([str(binary), "--describe"],
                        cwd=suite, env=env, text=True, stderr=subprocess.STDOUT)
                    info_log.write_text(description)
                    build_info.append(f"{name}/{variant} build: {description.strip()}")
                except (subprocess.CalledProcessError, OSError) as error:
                    output = getattr(error, "output", str(error))
                    info_log.write_text(output)
                    record(name, variant, "metadata", getattr(error, "returncode", 1), info_log, output[-1000:])
                    continue
            # Unchanged drivers write their relative results into a fresh cwd.
            # Never publish output from a failed or interrupted measurement.
            with tempfile.TemporaryDirectory(prefix="bench-measure-") as tmp:
                code, error = execute([str(binary), *arguments], tmp, log, env)
                if not code:
                    output = Path(tmp) / "results"
                    if kind == "hashmaps":
                        output /= "storage"
                    suffix = "*.txt" if kind == "parsers" else "*.csv"
                    files = sorted(output.glob(suffix))
                    if not files:
                        code, error = 1, "No measurement files produced."
                    else:
                        for file in files:
                            shutil.copy2(file, results / file.name)
                            active[file.stem] = label
            record(name, variant, "measure", code, log, error)
    (results / "active.json").write_text(json.dumps(active, indent=2) + "\n")
    if not states:
        log = logs / "discovery.log"
        log.write_text("No adapters support the requested variants.\n")
        record("suite", "all", "configure", 1, log, log.read_text().strip())
    if action != "run":
        return int(any(state["exit_code"] for state in states))
    if platform_config:
        cc, cflags, cxx, cxxflags = platform_config
        log = logs / "platform.log"
        code, error = execute(["sh", str(suite.parent / "platform.sh"),
                               str(results / "platform.txt"), cc, cflags], suite, log)
        if code:
            record("suite", "all", "metadata", code, log, error)
        with (results / "platform.txt").open("a") as stream:
            if kind == "hashmaps":
                stream.write(f"cxx: {cxx}\ncxxflags: {cxxflags}\n")
            stream.write("\n".join(build_info) + "\n")
    code, error = execute(["make", "--no-print-directory", "--silent", "report"],
                           suite, logs / "report.log")
    if code:
        record("suite", "all", "report", code, logs / "report.log", error)
    return int(any(state["exit_code"] for state in states))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", type=Path, required=True)
    parser.add_argument("--variant", action="append", default=[])
    parser.add_argument("--adapter", action="append", default=[])
    parser.add_argument("--argument", action="append", default=[])
    actions = parser.add_mutually_exclusive_group()
    actions.add_argument("--build-only", dest="action", action="store_const", const="build")
    actions.add_argument("--setup-only", dest="action", action="store_const", const="setup")
    parser.set_defaults(action="run")
    args = parser.parse_args()
    if args.suite.resolve().name not in GENERATORS:
        parser.error("unknown benchmark suite")
    return run(args.suite, args.variant, args.adapter, args.argument,
               os.getenv("INSTALL_DEPS") == "1", args.action)


if __name__ == "__main__":
    raise SystemExit(main())
