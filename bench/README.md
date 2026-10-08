# hwire Benchmark Suites

| Suite | Measurement | Documentation |
| --- | --- | --- |
| Parsers | HTTP start-line/header parsing with stack state | [Parsers](parsers/README.md) |
| Hashmaps | Storage construction, insertion, lookup and memory | [Hashmaps](hashmaps/README.md) |
| Production | Native HTTP request/header storage and string lookup with preallocated memory | [Production](production/README.md) |

## Running

Each suite runs independently with `make` in its directory. From `bench/`:

```sh
make                  # build, measure and report all suites
make parsers          # plain parsing
make hashmaps         # hashmap comparison
make production       # production header-processing scenario
make setup            # fetch active implementations' pinned dependencies
make build            # build all suites
make check            # check shared reports and all suites
make report           # render saved results
make list             # list active implementations
```

Registration contracts, dependencies, measurement boundaries and results are
specified in each suite's README. Shared benchmark helpers are in `shared/`;
pre-generated inputs are in [Data](data/README.md). Each implementation owns
its `fetch.sh`, `config.mk` and ignored dependency directory. Generated binaries,
raw measurements and downloaded product sources are not committed.


## Manual GitHub Actions

Open the [benchmark workflow](https://github.com/mah0x211/hwire/actions/workflows/benchmark.yml)
and select **Run workflow**. Repository write access is required.

- **Branch**: Revision to measure; its exact commit is recorded in the report.
- **suite**: `parsers`, `hashmaps`, `production` or `all` (default).
- **compare_ref**: Optional branch, tag or commit in this repository. It is
  measured after the selected revision, in the same job on the same runner.
  Both revisions must provide compatible suites and toolchain requirements.

The workflow runs on Linux x86-64 (`ubuntu-24.04`), using each suite's default
supported CPU variants and existing adaptive sampling. Suites and revisions run
sequentially. Read the tables in the Actions Summary; download the artifact for
Markdown reports, raw measurements, environment/build metadata and execution
logs. Available measurements and logs are retained when a suite fails.

The comparison reports describe each revision's own workloads and settings.
Check those settings before interpreting a difference as a source optimization.
Same-job execution reduces machine differences; run order and system load can
still affect measurements. Separate workflow runs may use different hardware.

This workflow runs only when manually requested. It does not run on pull requests,
pushes or a schedule, and does not modify repository files. Results in the suite
READMEs and the root README are published snapshots: update them deliberately
for substantial source changes, new comparison targets or release preparation.
Routine measurements remain in Actions summaries and artifacts.
