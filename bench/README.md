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
