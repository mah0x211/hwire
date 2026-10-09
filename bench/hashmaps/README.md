# Benchmarking Hashmaps

Compare hashmap storage for HTTP header and query key/value slices.
Case-sensitive and ASCII
case-insensitive maps are measured separately. HTTP parsing is excluded.


## Workloads

| Scenario | Keys | Initial reserve | Storage acquisition |
|---|---:|---:|---|
| Reserved | 32 / 64 / 128 | Same as key count | Before insert/lookup timing; included in build |
| Growth, allocated | 256 | 32 | Standard library allocation |

The driver selects keys from the pre-generated
[header corpus](../data/headers/headers_512.h). It removes ASCII-CI duplicates,
then shuffles with seed 42 and takes prefixes without replacement. Every map,
mode and sample uses the same sets. Key/value slices borrow immutable corpus
storage. Hit queries use separate buffers; CI queries swap ASCII casing.
Miss keys use the corpus's first-byte replacement. This controls key sizes
without claiming to reproduce the frequency of headers in browser traffic.

Adapter code contains storage operations only, with no
timers, samples or result validation.


## Metrics

| Metric | Meaning |
|---|---|
| Mean ± SD | Mean operation time ± sample standard deviation; each sample is an average over repeated operations |
| Relative | Time tables: mean / fastest mean. Memory tables: live bytes / smallest live bytes. 1.00× is the baseline; 1.20× means 20% more time or memory |
| RCIW | Full confidence interval width / mean; describes uncertainty in the estimated mean |
| Build, ns/table | Initial context/storage acquisition and initialization, then all insertions |
| Insert, ns/key | All insertions; initial setup excluded, growth included |
| Hit / miss, ns/lookup | Searches in the fully populated container |
| Memory (bytes) / Bytes/key | Final live container/storage bytes / stored unique keys |
| Slot load factor | Stored unique keys / allocated hash slots, returned by each adapter; distinct from entry-capacity usage and the configured maximum |
| Extensions | Additional capacity transitions after the initial 32-key reservation |

Cleanup runs after timing. Build includes initial allocation, initialization
and all insertions. Insert excludes initial setup and includes allocations
triggered by expansion. Lookup setup and input preparation are outside timing.

Memory excludes borrowed key/value contents and system allocator metadata.


### Sampling

Sampling starts with 20 samples and checks every 10 up to 100. Target RCIW
is 2% (full interval width / mean), using Student-t intervals with Bonferroni
correction over nine stopping points. Iterations are calibrated to about
1 ms measured work per sample, up to one million key operations. Progress and
achieved precision appear on stderr; unmet targets are marked in the report.
The mean cost of an empty timer pair is calibrated before measurement and
subtracted from each measured interval. Build/insert time a complete population; hit/miss time repeated complete
traversals, amortizing timer calls. No fixed lookup-count scenario is added.


## Adding a Benchmark Target

Add `<name>/hashmap.c` or `<name>/hashmap.cpp`, using a directory name that
matches `[A-Za-z][A-Za-z0-9_]*`. Registration and `maplist.c`
are generated from those directories. Export every function below, using
`extern "C"` for C++ implementations.

### Dependency setup and build configuration

Each adapter owns its dependency retrieval and build settings. Neither the
registration script nor the Makefile needs library-specific download rules.

- Optional `<name>/setup.sh check|install` checks system dependencies or installs
  them when `INSTALL_DEPS=1`. Return nonzero on failure; `fetch.sh` runs only
  after setup succeeds. Both scripts run from the suite directory.
- Optional `<name>/fetch.sh` downloads a fixed upstream revision. `make setup`
  calls it with `sh` from the hashmap suite directory; use the script's directory
  to locate adapter files. Return zero on success, including when the requested
  revision is already present; return nonzero on failure.
- Optional `<name>/config.mk` declares the settings below. Paths are relative to
  `bench/hashmaps/`. Each variable uses the adapter directory name as its prefix.
- The adapter chooses its download directory and excludes it in its own
  `.gitignore`. The included adapters use `deps/`; other names are supported.
- `make` and `make build` run setup before compilation. `_`-prefixed adapters
  contribute no fetch scripts, configuration or sources.

```make
# example/config.mk
example_CPPFLAGS := -Iexample/vendor/include
example_SOURCES := example/vendor/src/map.cpp
example_CFLAGS :=
example_CXXFLAGS :=
example_LDLIBS :=
```

| Variable | Meaning |
|---|---|
| `<name>_CPPFLAGS` | Additional preprocessor/include flags for this adapter and its dependency sources |
| `<name>_SOURCES` | Additional C/C++ translation units; `hashmap.c` or `hashmap.cpp` is registered automatically |
| `<name>_CFLAGS` | Additional flags for this adapter's C translation units |
| `<name>_CXXFLAGS` | Additional flags for this adapter's C++ translation units |
| `<name>_LDLIBS` | Additional linker flags/libraries for the timing binary |
| `<name>_ENV` | Space-separated `NAME=value` assignments (shell quoting supported) for this adapter’s setup, fetch, build and execution |

All settings are optional. Dependency source paths must remain under the
adapter directory so that its compilation flags apply. Header files within
registered adapter directories are tracked as build dependencies.

### Adapter contract

- Use the library's standard allocator and `hashmap_hash.h` for the common
  hash. Initialize hash keys with seed 42 in constructors.
- Copy the `hwire_kv_pair_t` descriptor into the map. Its key/value bytes are
  borrowed from immutable input that remains alive until context destruction.
- Case-sensitive contexts compare keys by length and exact bytes. CI contexts
  hash and compare under ASCII case folding; preserve all non-ASCII bytes.
  Use `hashmap_key.h` for the common wordwise CI comparison.
- The driver supplies valid arguments, sufficient input lifetimes and unique
  keys under the selected comparison mode. Add storage operations without
  input-validation loops, timers, sampling logic or result checks.
- Each constructor creates a fresh, empty context. Reserved constructors
  provide room for the full dataset; growth constructors initially reserve
  32 keys and expand during pushes. Use the same implementation for both
  modes, with only the hash/equality rules and required index storage changing.

The driver uses the functions as follows:

| Measurement | Before timing | Timed calls | After timing |
|---|---|---|---|
| Build | Input preparation | Constructor, then one push per key | Free context |
| Insert | Constructor | One push per key, including triggered growth | Free context |
| Hit / Miss | Constructor and population | Repeated get or get_ci | Free context |
| Memory / load / extensions | Constructor and population | No timed interval | Read metadata, then free context |

Construction and population also run during untimed metadata preparation and
lookup setup. Timed calibration runs determine iteration counts and are excluded
from the reported sample statistics. Each measurement context is released before
the next population; adapters must not reuse a previously populated map.

### Functions

```c
/**
 * Return a stable display name for CSV rows and progress output.
 * Called outside timed intervals; the returned string must remain valid for
 * the entire benchmark run. Include the configuration when names would
 * otherwise be ambiguous, for example "Map (configuration)". Use a name without
 * commas or line breaks because it is written as a CSV field.
 *
 * @return A borrowed, non-NULL, NUL-terminated display name.
 */
const char *name_hashmap_name(void);

/**
 * Create an empty case-sensitive map with room for the reserved dataset.
 * Allocate the context and reserve storage using the library's native API.
 * Subsequent pushes of capacity unique keys must not trigger growth.
 * Called inside Build timing; outside Insert and Hit/Miss timing, and during
 * untimed metadata preparation. Return only after initialization is complete.
 *
 * @param capacity Number of unique keys to store: currently 32, 64 or 128.
 * @return An owned context released by name_hashmap_free, or NULL on failure.
 */
void *name_hashmap_new_exact(size_t capacity);

/**
 * Create an empty ASCII case-insensitive map for the reserved dataset.
 * Reserve room for capacity unique keys and configure consistent CI hashing
 * and equality. Select only the CI index when the implementation allows it.
 * Called at the same points and with the same timing as new_exact.
 *
 * @param capacity Number of unique keys to store: currently 32, 64 or 128.
 * @return An owned context released by name_hashmap_free, or NULL on failure.
 */
void *name_hashmap_new_ci(size_t capacity);

/**
 * Create an empty case-sensitive map for the allocated-growth workload.
 * Reserve 32 keys initially; final_capacity is the final dataset size, not
 * the initial reservation. Pushes use the implementation's growth mechanism.
 * Do not reserve the full
 * dataset or preallocate expansion blocks in this constructor.
 * Initial allocation/initialization is inside Build timing and outside Insert
 * and Hit/Miss timing. Allocation and initialization triggered by pushes are
 * inside both Build and Insert timing.
 *
 * @param final_capacity Final number of unique keys: currently 256.
 * @return An owned context released by name_hashmap_free, or NULL on failure.
 */
void *name_hashmap_new_growth_exact(size_t final_capacity);

/**
 * Create an empty ASCII case-insensitive map for allocated growth.
 * Use the same initial reservation and growth policy as new_growth_exact,
 * with CI hashing/equality and only the required CI index storage.
 * Called at the same points and with the same timing as new_growth_exact.
 *
 * @param final_capacity Final number of unique keys: currently 256.
 * @return An owned context released by name_hashmap_free, or NULL on failure.
 */
void *name_hashmap_new_growth_ci(size_t final_capacity);

/**
 * Destroy the map and release all allocations owned by its context.
 * For linked tables, release every segment as well as the initial context.
 * Called after every population/lookup measurement and metadata preparation,
 * always outside timing. Do not release borrowed key/value bytes.
 *
 * @param context Context returned by a successful constructor.
 */
void name_hashmap_free(void *context);

/**
 * Store one key/value descriptor using the context's configured mode.
 * Copy the descriptor; retain references to the caller's key/value bytes.
 * Use the native insertion API and acquire/initialize additional storage when
 * growth requires it. Called once per selected key inside Build and Insert
 * timing, and outside timing when preparing metadata or Hit/Miss contexts.
 * Input keys are unique under the selected mode; no duplicate-retention
 * mechanism is required by this workload.
 *
 * @param context Initialized map receiving the insertion.
 * @param pair Borrowed descriptor; its slice bytes outlive the context.
 * @return Nonzero on successful insertion; zero on failure.
 */
int name_hashmap_push(void *context, const hwire_kv_pair_t *pair);

/**
 * Find a key by its length and exact bytes in a case-sensitive context.
 * Called repeatedly inside both Hit and Miss timing after population.
 * Hash the supplied slice and use the map's native lookup. Return the stored
 * descriptor directly, without allocating, copying or modifying the map.
 *
 * @param context Fully populated map created by an exact constructor.
 * @param key Borrowed query bytes; NUL termination is not required.
 * @param len Query length in bytes, excluding any terminating NUL.
 * @return Borrowed stored descriptor, or NULL if the key is absent.
 */
const hwire_kv_pair_t *name_hashmap_get(const void *context,
                                     const char *key, size_t len);

/**
 * Find a key using ASCII case-insensitive hashing and equality.
 * Called repeatedly inside both Hit and Miss timing for a CI context.
 * Queries may differ in ASCII casing from stored keys. Use the native lookup
 * with CI hashing/equality; do not allocate a normalized key or result copy.
 *
 * @param context Fully populated map created by a CI constructor.
 * @param key Borrowed query bytes; NUL termination is not required.
 * @param len Query length in bytes, excluding any terminating NUL.
 * @return Borrowed stored descriptor, or NULL if the key is absent.
 */
const hwire_kv_pair_t *name_hashmap_get_ci(const void *context,
                                        const char *key, size_t len);

/**
 * Report final live memory owned by the populated map.
 * Called only during untimed metadata preparation. Include the context,
 * bucket/control arrays, stored descriptors and every retained table segment.
 * Use native size metadata or the implementation's actual allocation layout.
 * Exclude borrowed key/value bytes, system allocator metadata and buffers
 * already released by resizing. Do not return cumulative allocated bytes.
 *
 * @param context Fully populated map.
 * @return Final live memory in bytes.
 */
size_t name_hashmap_bytes(const void *context);

/**
 * Report actual occupied hash slots divided by allocated hash slots.
 * Called only during untimed metadata preparation. Use the native container's
 * occupancy and slot capacity, not its configured maximum load factor or the
 * requested key capacity. For linked tables, divide total occupied slots by
 * total allocated slots; do not average per-table load factors.
 *
 * @param context Fully populated map with unique keys.
 * @return Actual slot occupancy as a ratio from 0.0 to 1.0.
 */
double name_hashmap_loadfactor(const void *context);

/**
 * Report capacity extensions after the initial reservation.
 * Called only during untimed metadata preparation. Count native capacity
 * transitions or appended storage segments. Doubling implementations may derive
 * the count from initial and final slot capacities. Initial reservation is
 * excluded; an extension can involve multiple allocation calls.
 *
 * @param context Fully populated map.
 * @return Zero for Reserved Capacity, three for the current Growth workload.
 */
size_t name_hashmap_growths(const void *context);
```

### Additional configurations

An adapter may also export `name_hashmap_new_exact_<config>`,
`name_hashmap_new_ci_<config>`, their `new_growth_` counterparts and
`name_hashmap_name_<config>`. The generator discovers these constructor
suffixes and registers additional configurations sharing the base adapter's
lookup, insertion and cleanup functions. Configuration suffixes and display
names are supplied by adapters; registration embeds no implementation names.

# Benchmark

<!-- benchmark-environment -->
## Environment

```text
date                 : 2026-10-10T08:29:31+09:00
uname                : Linux 6.8.0-142-generic x86_64
os                   : Ubuntu 24.04.3 LTS
cpu                  : AMD Ryzen 7 PRO 4750GE with Radeon Graphics
clock                : 3.09 GHz
cores                : 2
memory               : 1894 MiB
cache l1-Data        : 64K
cache l1-Instruction : 64K
cache l2-Unified     : 512K
cache l3-Unified     : 16384K
virtualization       : kvm
```
<!-- /benchmark-environment -->

## Requirements

- GNU Make: Build and run the suite.
- Python 3.10+: Register adapters, generate inputs and render reports.
- C11 compiler and linker: Compile the driver and C adapters.
- C++17 compiler and `ar`: Build C++ adapters and static archives.
- `curl`, `tar` and network access: Fetch pinned dependencies during initial setup.

Dependencies are fetched before timing. Repeated setup reuses downloaded
revisions; each target owns its `fetch.sh` and `config.mk`.


## Commands

| Command | Action |
|---|---|
| `make` / `make run` | Build and measure all four scenarios |
| `make setup` | Fetch the pinned dependencies of registered adapters |
| `make build` | Fetch dependencies and build the timing binaries |
| `make growth` | Measure only the expansion scenario |
| `make report` | Render saved CSVs as Markdown tables |
| `python3 scripts/report_hashmaps.py --write-readme` | Replace the Benchmark section with saved results |
| `make adapters` | Build adapter static libraries |
| `make check` | Build and check only the statistical helper |
| `make list` | List registered maps |
| `make clean` | Remove build artifacts, retain results |

Results are saved locally in `results/storage/<map>-<variant>.csv` and excluded
from version control. The generated tables are published in this README.
`--quick` runs a short development measurement, not publication sampling.
`--map ID` selects one registered adapter. `--metadata` outputs actual load factors
without timing or replacing saved results. Directory prefixes `_` disable
adapters and their configurations, including dependencies, without changing
the driver.


### Dependencies and failures

The Makefile includes `../shared/runner.mk`, which invokes the shared
`../shared/scripts/run.py`. Each adapter owns its optional `setup.sh`, `fetch.sh`
and `config.mk`. The runner calls `setup.sh check` first, then runs `fetch.sh`
only after setup succeeds.
Use `make run INSTALL_DEPS=1` to permit installation through the adapter's
`setup.sh install`; ordinary local runs only check dependencies. CI enables
installation. Adapter setup may use the [shared dependency helpers](../shared/README.md); dependency
names and toolchain requirements remain in the adapter directory.

Adapters and supported build variants are built and measured independently.
A setup, source-fetch, build or measurement failure is recorded and other targets
continue. The report compares only successful measurements and ends with a
Failed Benchmark Targets table containing short errors. Full logs and
`status.json` are retained in the result directory and uploaded by Actions.
After generating the report, the command exits nonzero if any target failed.

Each `make run` replaces the suite's generated measurements, status and logs.
Failed or partially completed measurements and older results are excluded.
`make build` and `make setup` also continue across adapters; their statuses and
logs are kept under `bin/status/`, without replacing saved measurements.

### Configuration

Native x86 builds use `-march=native`. `make VARIANTS=siphash` selects
the fallback hash build. Hashing and comparison details
for the supplied adapters are documented under Benchmark.

Release builds use `-O2 -DNDEBUG`. The platform file records the compiler,
flags and hash backend. Published measurements use the x86 reference system.


## Benchmark Targets

Compiler and flag entries below record the published measurements.

Compare fixed-capacity linked tables with three expandable maps, using C/C++
adapters for borrowed HTTP header and query key/value slices. Each target is
measured with case-sensitive and ASCII case-insensitive keys.

All targets use the same seeded hash backend: native AES, or SipHash-1-3 with
`make VARIANTS=siphash`. ASCII-CI comparisons fold and compare eight bytes at a
time, with bytewise handling of the remainder. Adapters report load from native
containers. Equal extension counts do not imply equal allocation counts, resize
points, memory use or rehash work.

### hwire_table

Caller-supplied pair/index arrays support fixed capacities and linked expansion,
while retaining insertion order and duplicate values.

<details>
<summary>Adapter and build details</summary>

- Adapter: [hashmap.c](hwire/hashmap.c)
- Library: Current sources in `../../src/`
- Compiler: <!-- compiler:compiler -->`gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags -->
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
  - `siphash`: Native flags plus `-DHWIRE_NO_AES`; selects SipHash-1-3 for the shared hash backend.
- Build: `C`; native CPU target; AES hash; 2N / 4N / 8N slot configurations

Only index slot capacity differs between configurations. Every pair array is
filled completely; slot loads are 50%, 25% and 12.5% respectively. Growth uses
32+32+64+128 entries: the initial segment plus three extensions. Linked tables
report aggregate slot load. Each KV is stored once in the pair array, and the
hash may be inlined.

</details>


### CC

An expandable map with custom hashing/comparison for borrowed string keys.

<details>
<summary>Adapter and build details</summary>

- Adapter: [hashmap.cpp](cc/hashmap.cpp)
- Library: CC v1.4.3, revision pinned in [fetch.sh](cc/fetch.sh)
- Compiler: <!-- compiler:cxx -->`g++`<!-- /compiler -->
- CFLAGS: <!-- flags:cxxflags -->`-O2 -DNDEBUG -std=c++17`<!-- /flags --> (CXXFLAGS)
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
  - `siphash`: Native flags plus `-DHWIRE_NO_AES`; selects SipHash-1-3 for the shared hash backend.
- Build: `C++17` adapter; native CPU target; shared AES hash

Reserve 32 keys for growth, then use native automatic expansion. The map stores
a key descriptor and copied KV value. Custom CI comparison retains CC's ordering
when keys differ. Hashing calls the shared bridge.

</details>


### absl::flat_hash_map

An expandable C++ flat hash map with custom hashing and key equality.

<details>
<summary>Adapter and build details</summary>

- Adapter: [hashmap.cpp](abseil/hashmap.cpp)
- Library: Abseil 20260817.0, revision pinned in [fetch.sh](abseil/fetch.sh)
- Compiler: <!-- compiler:cxx -->`g++`<!-- /compiler -->
- CFLAGS: <!-- flags:cxxflags -->`-O2 -DNDEBUG -std=c++17`<!-- /flags --> (CXXFLAGS)
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
  - `siphash`: Native flags plus `-DHWIRE_NO_AES`; selects SipHash-1-3 for the shared hash backend.
- Build: `C++17`; native CPU target; shared AES hash

Reserve 32 keys for growth, then use native automatic expansion. The map stores
the copied KV value with its key. Hashing calls the shared bridge.

</details>


### khashl

An expandable C hash map with custom hashing and equality for borrowed keys.

<details>
<summary>Adapter and build details</summary>

- Adapter: [hashmap.c](khashl/hashmap.c)
- Library: Revision pinned in [UPSTREAM.md](khashl/UPSTREAM.md) and [fetch.sh](khashl/fetch.sh)
- Compiler: <!-- compiler:compiler -->`gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags -->
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
  - `siphash`: Native flags plus `-DHWIRE_NO_AES`; selects SipHash-1-3 for the shared hash backend.
- Build: `C`; native CPU target; shared AES hash (low 32 bits)

Reserve 32 keys for growth, then use native automatic expansion. The map stores
a key descriptor and copied KV value. Its native hash interface consumes the
low 32 bits of the shared hash bridge result. Native representation costs are
included in memory measurements for every target.

</details>


## Sampling and precision

| Setting / result | Value |
|---|---|
| Target RCIW | 2% (full confidence interval width / mean) |
| Sample count | 20–100, checked every 10 samples |
| Iterations | Calibrated to approximately 1 ms per sample, capped at one million key operations |
| Allocated Growth extensions | Three per implementation, including each hwire slot configuration |
| Results meeting Target RCIW | 187 / 192 |
| Results reaching the sample limit above target | 5, marked `unmet` in the tables |
| Execution files | Raw CSVs and platform metadata retained locally, outside version control |

- **Largest unmet intervals:** khashl, case-sensitive Reserved Capacity,
  128-key Insert has RCIW 6.35%; hwire 2N, case-sensitive Reserved Capacity,
  128-key Hit has RCIW 5.41%. Their speed and crossover comparisons have
  wider uncertainty.


## Case-Sensitive Reserved Capacity

Exact byte comparisons. Store 32, 64 or 128 unique keys with storage reserved for the entire dataset. Build includes initial allocation, initialization and all insertions; no capacity expansion occurs.

Cleanup runs outside all timed intervals.


### Memory

Final live container/storage bytes; borrowed key/value contents and allocator metadata are excluded.

**32 keys**


| Map                 | Memory (bytes) | Relative | Bytes/key | Slot load factor |
| ------------------- | -------------- | -------- | --------- | ---------------- |
| hwire_table (2N)    | 1360           | 1.00×    | 42.50     | 50.00%           |
| hwire_table (4N)    | 1488           | 1.09×    | 46.50     | 25.00%           |
| hwire_table (8N)    | 1744           | 1.28×    | 54.50     | 12.50%           |
| absl::flat_hash_map | 2896           | 2.13×    | 90.50     | 50.79%           |
| khashl              | 3640           | 2.68×    | 113.75    | 50.00%           |
| CC                  | 3776           | 2.78×    | 118.00    | 50.00%           |

**64 keys**


| Map                 | Memory (bytes) | Relative | Bytes/key | Slot load factor |
| ------------------- | -------------- | -------- | --------- | ---------------- |
| hwire_table (2N)    | 2640           | 1.00×    | 41.25     | 50.00%           |
| hwire_table (4N)    | 2896           | 1.10×    | 45.25     | 25.00%           |
| hwire_table (8N)    | 3408           | 1.29×    | 53.25     | 12.50%           |
| absl::flat_hash_map | 6032           | 2.28×    | 94.25     | 50.39%           |
| khashl              | 7232           | 2.74×    | 113.00    | 50.00%           |
| CC                  | 7488           | 2.84×    | 117.00    | 50.00%           |

**128 keys**


| Map                 | Memory (bytes) | Relative | Bytes/key | Slot load factor |
| ------------------- | -------------- | -------- | --------- | ---------------- |
| hwire_table (2N)    | 5200           | 1.00×    | 40.62     | 50.00%           |
| hwire_table (4N)    | 5712           | 1.10×    | 44.62     | 25.00%           |
| hwire_table (8N)    | 6736           | 1.30×    | 52.62     | 12.50%           |
| absl::flat_hash_map | 12312          | 2.37×    | 96.19     | 50.20%           |
| khashl              | 14416          | 2.77×    | 112.62    | 50.00%           |
| CC                  | 14912          | 2.87×    | 116.50    | 50.00%           |


### Build

Initial acquisition and initialization plus all insertions; any expansion is included.

**32 keys**


| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 295.21 ±2.54         | 1.00×    | 3.39   | 20      | 1.20% |
| hwire_table (4N)    | 298.97 ±2.93         | 1.01×    | 3.34   | 20      | 1.37% |
| hwire_table (2N)    | 305.93 ±5.30         | 1.04×    | 3.27   | 40      | 1.61% |
| khashl              | 364.70 ±4.66         | 1.24×    | 2.74   | 20      | 1.79% |
| CC                  | 405.59 ±1.95         | 1.37×    | 2.47   | 20      | 0.67% |
| absl::flat_hash_map | 601.56 ±10.34        | 2.04×    | 1.66   | 40      | 1.60% |

**64 keys**


| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 586.07 ±5.35         | 1.00×    | 1.71   | 20      | 1.28% |
| hwire_table (2N)    | 594.20 ±6.40         | 1.01×    | 1.68   | 20      | 1.51% |
| hwire_table (8N)    | 601.12 ±5.80         | 1.03×    | 1.66   | 20      | 1.35% |
| khashl              | 657.08 ±11.26        | 1.12×    | 1.52   | 30      | 1.87% |
| CC                  | 815.39 ±3.18         | 1.39×    | 1.23   | 20      | 0.55% |
| absl::flat_hash_map | 1183.03 ±5.36        | 2.02×    | 0.85   | 20      | 0.63% |

**128 keys**


| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | -------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 1191.39 ±86.65       | 1.00×    | 0.84   | 100     | 4.12% (unmet) |
| hwire_table (2N)    | 1195.95 ±16.16       | 1.00×    | 0.84   | 20      | 1.89%         |
| hwire_table (4N)    | 1212.34 ±6.88        | 1.02×    | 0.82   | 20      | 0.79%         |
| khashl              | 1250.78 ±4.43        | 1.05×    | 0.80   | 20      | 0.50%         |
| CC                  | 1585.71 ±9.43        | 1.33×    | 0.63   | 20      | 0.83%         |
| absl::flat_hash_map | 2391.44 ±6.78        | 2.01×    | 0.42   | 20      | 0.40%         |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**32 keys**


| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 8.51 ±0.09         | 1.00×    | 117.51 | 20      | 1.55% |
| hwire_table (4N)    | 8.72 ±0.12         | 1.02×    | 114.68 | 20      | 1.88% |
| hwire_table (2N)    | 8.89 ±0.11         | 1.04×    | 112.49 | 20      | 1.65% |
| khashl              | 9.14 ±0.11         | 1.07×    | 109.41 | 20      | 1.63% |
| CC                  | 12.04 ±0.13        | 1.41×    | 83.06  | 20      | 1.49% |
| absl::flat_hash_map | 17.00 ±0.10        | 2.00×    | 58.82  | 20      | 0.83% |

**64 keys**


| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | ------------------ | -------- | ------ | ------- | ------------- |
| hwire_table (4N)    | 8.80 ±0.10         | 1.00×    | 113.64 | 20      | 1.55%         |
| hwire_table (2N)    | 8.91 ±0.11         | 1.01×    | 112.23 | 20      | 1.68%         |
| hwire_table (8N)    | 8.95 ±0.10         | 1.02×    | 111.73 | 20      | 1.54%         |
| khashl              | 9.29 ±0.75         | 1.06×    | 107.64 | 100     | 4.57% (unmet) |
| CC                  | 12.41 ±0.05        | 1.41×    | 80.58  | 20      | 0.58%         |
| absl::flat_hash_map | 17.78 ±0.09        | 2.02×    | 56.24  | 20      | 0.69%         |

**128 keys**


| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 8.95 ±0.09         | 1.00×    | 111.73 | 20      | 1.39% |
| hwire_table (2N)    | 9.09 ±0.11         | 1.02×    | 110.01 | 20      | 1.68% |
| hwire_table (4N)    | 9.24 ±0.06         | 1.03×    | 108.23 | 20      | 0.90% |
| khashl              | 9.29 ±0.16         | 1.04×    | 107.64 | 30      | 1.85% |
| CC                  | 12.18 ±0.05        | 1.36×    | 82.10  | 20      | 0.56% |
| absl::flat_hash_map | 18.52 ±0.11        | 2.07×    | 54.00  | 20      | 0.86% |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**32 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 8.70 ±0.18            | 1.00×    | 114.94 | 50      | 1.65%         |
| hwire_table (4N)    | 8.97 ±0.53            | 1.03×    | 111.48 | 100     | 3.33% (unmet) |
| hwire_table (2N)    | 9.17 ±0.72            | 1.05×    | 109.05 | 100     | 4.48% (unmet) |
| khashl              | 10.61 ±0.21           | 1.22×    | 94.25  | 40      | 1.84%         |
| CC                  | 11.14 ±1.04           | 1.28×    | 89.77  | 100     | 5.29% (unmet) |
| absl::flat_hash_map | 13.44 ±0.08           | 1.54×    | 74.40  | 20      | 0.83%         |

**64 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (4N)    | 8.85 ±0.08            | 1.00×    | 112.99 | 20      | 1.30%         |
| hwire_table (8N)    | 9.03 ±0.63            | 1.02×    | 110.74 | 100     | 3.96% (unmet) |
| hwire_table (2N)    | 9.17 ±0.08            | 1.04×    | 109.05 | 20      | 1.24%         |
| khashl              | 11.06 ±0.11           | 1.25×    | 90.42  | 20      | 1.38%         |
| CC                  | 11.19 ±0.42           | 1.26×    | 89.37  | 100     | 2.12% (unmet) |
| absl::flat_hash_map | 14.51 ±0.09           | 1.64×    | 68.92  | 20      | 0.88%         |

**128 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 9.70 ±0.05            | 1.00×    | 103.09 | 20      | 0.66%         |
| hwire_table (4N)    | 9.78 ±0.07            | 1.01×    | 102.25 | 20      | 1.06%         |
| hwire_table (2N)    | 10.03 ±0.78           | 1.03×    | 99.70  | 100     | 4.41% (unmet) |
| khashl              | 11.94 ±0.06           | 1.23×    | 83.75  | 20      | 0.74%         |
| CC                  | 12.08 ±0.06           | 1.25×    | 82.78  | 20      | 0.71%         |
| absl::flat_hash_map | 15.38 ±0.05           | 1.59×    | 65.02  | 20      | 0.47%         |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**32 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 5.67 ±0.06            | 1.00×    | 176.37 | 20      | 1.46% |
| hwire_table (4N)    | 5.88 ±0.04            | 1.04×    | 170.07 | 20      | 1.00% |
| hwire_table (2N)    | 7.04 ±0.16            | 1.24×    | 142.05 | 50      | 1.87% |
| CC                  | 7.54 ±0.09            | 1.33×    | 132.63 | 20      | 1.58% |
| khashl              | 7.97 ±0.04            | 1.41×    | 125.47 | 20      | 0.77% |
| absl::flat_hash_map | 8.33 ±0.19            | 1.47×    | 120.05 | 50      | 1.84% |

**64 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 5.45 ±0.04            | 1.00×    | 183.49 | 20      | 0.99%         |
| hwire_table (4N)    | 5.80 ±0.03            | 1.06×    | 172.41 | 20      | 0.79%         |
| hwire_table (2N)    | 6.85 ±0.04            | 1.26×    | 145.99 | 20      | 0.78%         |
| CC                  | 7.61 ±0.17            | 1.40×    | 131.41 | 50      | 1.81%         |
| khashl              | 7.79 ±0.29            | 1.43×    | 128.37 | 100     | 2.14% (unmet) |
| absl::flat_hash_map | 8.67 ±0.02            | 1.59×    | 115.34 | 20      | 0.27%         |

**128 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 5.64 ±0.06            | 1.00×    | 177.30 | 20      | 1.38%         |
| hwire_table (4N)    | 5.92 ±0.43            | 1.05×    | 168.92 | 100     | 4.16% (unmet) |
| hwire_table (2N)    | 7.06 ±0.09            | 1.25×    | 141.64 | 20      | 1.82%         |
| CC                  | 7.39 ±0.13            | 1.31×    | 135.32 | 30      | 1.98%         |
| khashl              | 8.44 ±0.23            | 1.50×    | 118.48 | 70      | 1.90%         |
| absl::flat_hash_map | 9.13 ±0.05            | 1.62×    | 109.53 | 20      | 0.79%         |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**32 keys**


| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (8N)    | 0.304              | 1.00×    | 8.70 ±0.18                | Baseline                 |
| hwire_table (4N) †  | 0.308              | 1.01×    | 8.97 ±0.53                | No crossover             |
| hwire_table (2N) †  | 0.315              | 1.04×    | 9.17 ±0.72                | No crossover             |
| khashl              | 0.375              | 1.23×    | 10.61 ±0.21               | No crossover             |
| CC †                | 0.417              | 1.37×    | 11.14 ±1.04               | No crossover             |
| absl::flat_hash_map | 0.615              | 2.02×    | 13.44 ±0.08               | No crossover             |

**64 keys**


| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (4N)    | 0.595              | 1.00×    | 8.85 ±0.08                | Baseline                 |
| hwire_table (2N)    | 0.603              | 1.01×    | 9.17 ±0.08                | No crossover             |
| hwire_table (8N) †  | 0.610              | 1.03×    | 9.03 ±0.63                | No crossover             |
| khashl              | 0.668              | 1.12×    | 11.06 ±0.11               | No crossover             |
| CC †                | 0.827              | 1.39×    | 11.19 ±0.42               | No crossover             |
| absl::flat_hash_map | 1.198              | 2.01×    | 14.51 ±0.09               | No crossover             |

**128 keys**


| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (8N) †  | 1.201              | 1.00×    | 9.70 ±0.05                | Baseline                 |
| hwire_table (2N) †  | 1.206              | 1.00×    | 10.03 ±0.78               | No crossover             |
| hwire_table (4N)    | 1.222              | 1.02×    | 9.78 ±0.07                | No crossover             |
| khashl              | 1.263              | 1.05×    | 11.94 ±0.06               | No crossover             |
| CC                  | 1.598              | 1.33×    | 12.08 ±0.06               | No crossover             |
| absl::flat_hash_map | 2.407              | 2.00×    | 15.38 ±0.05               | No crossover             |


#### Build + Miss

**32 keys**


| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (8N)    | 0.301               | 1.00×    | 5.67 ±0.06                 | Baseline                 |
| hwire_table (4N)    | 0.305               | 1.01×    | 5.88 ±0.04                 | No crossover             |
| hwire_table (2N)    | 0.313               | 1.04×    | 7.04 ±0.16                 | No crossover             |
| khashl              | 0.373               | 1.24×    | 7.97 ±0.04                 | No crossover             |
| CC                  | 0.413               | 1.37×    | 7.54 ±0.09                 | No crossover             |
| absl::flat_hash_map | 0.610               | 2.03×    | 8.33 ±0.19                 | No crossover             |

**64 keys**


| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (4N)    | 0.592               | 1.00×    | 5.80 ±0.03                 | Baseline                 |
| hwire_table (2N)    | 0.601               | 1.02×    | 6.85 ±0.04                 | No crossover             |
| hwire_table (8N)    | 0.607               | 1.02×    | 5.45 ±0.04                 | 44                       |
| khashl †            | 0.665               | 1.12×    | 7.79 ±0.29                 | No crossover             |
| CC                  | 0.823               | 1.39×    | 7.61 ±0.17                 | No crossover             |
| absl::flat_hash_map | 1.192               | 2.01×    | 8.67 ±0.02                 | No crossover             |

**128 keys**


| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (8N) †  | 1.197               | 1.00×    | 5.64 ±0.06                 | Baseline                 |
| hwire_table (2N)    | 1.203               | 1.00×    | 7.06 ±0.09                 | No crossover             |
| hwire_table (4N) †  | 1.218               | 1.02×    | 5.92 ±0.43                 | No crossover             |
| khashl              | 1.259               | 1.05×    | 8.44 ±0.23                 | No crossover             |
| CC                  | 1.593               | 1.33×    | 7.39 ±0.13                 | No crossover             |
| absl::flat_hash_map | 2.401               | 2.01×    | 9.13 ±0.05                 | No crossover             |


## Case-Insensitive Reserved Capacity

ASCII case-insensitive comparisons. Store 32, 64 or 128 unique keys with storage reserved for the entire dataset. Build includes initial allocation, initialization and all insertions; no capacity expansion occurs.

Cleanup runs outside all timed intervals.


### Memory

Final live container/storage bytes; borrowed key/value contents and allocator metadata are excluded.

**32 keys**


| Map                 | Memory (bytes) | Relative | Bytes/key | Slot load factor |
| ------------------- | -------------- | -------- | --------- | ---------------- |
| hwire_table (2N)    | 1360           | 1.00×    | 42.50     | 50.00%           |
| hwire_table (4N)    | 1488           | 1.09×    | 46.50     | 25.00%           |
| hwire_table (8N)    | 1744           | 1.28×    | 54.50     | 12.50%           |
| absl::flat_hash_map | 2896           | 2.13×    | 90.50     | 50.79%           |
| khashl              | 3640           | 2.68×    | 113.75    | 50.00%           |
| CC                  | 3776           | 2.78×    | 118.00    | 50.00%           |

**64 keys**


| Map                 | Memory (bytes) | Relative | Bytes/key | Slot load factor |
| ------------------- | -------------- | -------- | --------- | ---------------- |
| hwire_table (2N)    | 2640           | 1.00×    | 41.25     | 50.00%           |
| hwire_table (4N)    | 2896           | 1.10×    | 45.25     | 25.00%           |
| hwire_table (8N)    | 3408           | 1.29×    | 53.25     | 12.50%           |
| absl::flat_hash_map | 6032           | 2.28×    | 94.25     | 50.39%           |
| khashl              | 7232           | 2.74×    | 113.00    | 50.00%           |
| CC                  | 7488           | 2.84×    | 117.00    | 50.00%           |

**128 keys**


| Map                 | Memory (bytes) | Relative | Bytes/key | Slot load factor |
| ------------------- | -------------- | -------- | --------- | ---------------- |
| hwire_table (2N)    | 5200           | 1.00×    | 40.62     | 50.00%           |
| hwire_table (4N)    | 5712           | 1.10×    | 44.62     | 25.00%           |
| hwire_table (8N)    | 6736           | 1.30×    | 52.62     | 12.50%           |
| absl::flat_hash_map | 12312          | 2.37×    | 96.19     | 50.20%           |
| khashl              | 14416          | 2.77×    | 112.62    | 50.00%           |
| CC                  | 14912          | 2.87×    | 116.50    | 50.00%           |


### Build

Initial acquisition and initialization plus all insertions; any expansion is included.

**32 keys**


| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | -------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 362.97 ±37.61        | 1.00×    | 2.76   | 100     | 5.88% (unmet) |
| hwire_table (4N)    | 364.24 ±2.64         | 1.00×    | 2.75   | 20      | 1.01%         |
| hwire_table (2N)    | 376.15 ±3.39         | 1.04×    | 2.66   | 20      | 1.26%         |
| khashl              | 418.41 ±3.77         | 1.15×    | 2.39   | 20      | 1.26%         |
| CC                  | 461.49 ±20.14        | 1.27×    | 2.17   | 100     | 2.47% (unmet) |
| absl::flat_hash_map | 655.31 ±3.99         | 1.81×    | 1.53   | 20      | 0.85%         |

**64 keys**


| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 726.79 ±9.02         | 1.00×    | 1.38   | 20      | 1.73% |
| hwire_table (8N)    | 726.93 ±5.47         | 1.00×    | 1.38   | 20      | 1.05% |
| hwire_table (2N)    | 728.99 ±6.94         | 1.00×    | 1.37   | 20      | 1.33% |
| khashl              | 741.24 ±5.33         | 1.02×    | 1.35   | 20      | 1.01% |
| CC                  | 885.65 ±8.81         | 1.22×    | 1.13   | 20      | 1.39% |
| absl::flat_hash_map | 1313.78 ±15.16       | 1.81×    | 0.76   | 20      | 1.61% |

**128 keys**


| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 1435.81 ±10.25       | 1.00×    | 0.70   | 20      | 1.00% |
| khashl              | 1450.07 ±8.07        | 1.01×    | 0.69   | 20      | 0.78% |
| hwire_table (8N)    | 1451.58 ±11.66       | 1.01×    | 0.69   | 20      | 1.12% |
| hwire_table (2N)    | 1465.65 ±9.53        | 1.02×    | 0.68   | 20      | 0.91% |
| CC                  | 1700.41 ±8.09        | 1.18×    | 0.59   | 20      | 0.67% |
| absl::flat_hash_map | 2619.73 ±10.96       | 1.82×    | 0.38   | 20      | 0.59% |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**32 keys**


| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | ------------------ | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 10.54 ±0.08        | 1.00×    | 94.88  | 20      | 1.08%         |
| hwire_table (4N)    | 10.72 ±0.12        | 1.02×    | 93.28  | 20      | 1.54%         |
| khashl              | 10.99 ±0.80        | 1.04×    | 90.99  | 100     | 4.13% (unmet) |
| hwire_table (2N)    | 11.18 ±0.11        | 1.06×    | 89.45  | 20      | 1.43%         |
| CC                  | 13.56 ±0.09        | 1.29×    | 73.75  | 20      | 0.91%         |
| absl::flat_hash_map | 18.74 ±0.10        | 1.78×    | 53.36  | 20      | 0.73%         |

**64 keys**


| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | ------------------ | -------- | ------ | ------- | ------------- |
| khashl              | 10.56 ±0.05        | 1.00×    | 94.70  | 20      | 0.65%         |
| hwire_table (8N)    | 10.94 ±0.10        | 1.04×    | 91.41  | 20      | 1.33%         |
| hwire_table (4N)    | 11.02 ±0.55        | 1.04×    | 90.74  | 100     | 2.83% (unmet) |
| hwire_table (2N)    | 11.30 ±1.03        | 1.07×    | 88.50  | 100     | 5.16% (unmet) |
| CC                  | 13.50 ±0.14        | 1.28×    | 74.07  | 20      | 1.50%         |
| absl::flat_hash_map | 19.53 ±0.13        | 1.85×    | 51.20  | 20      | 0.90%         |

**128 keys**


| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | ------------------ | -------- | ------ | ------- | ------------- |
| khashl              | 10.76 ±0.05        | 1.00×    | 92.94  | 20      | 0.71%         |
| hwire_table (4N)    | 11.05 ±0.11        | 1.03×    | 90.50  | 20      | 1.35%         |
| hwire_table (8N)    | 11.16 ±0.74        | 1.04×    | 89.61  | 100     | 3.78% (unmet) |
| hwire_table (2N)    | 11.26 ±0.11        | 1.05×    | 88.81  | 20      | 1.40%         |
| CC                  | 13.04 ±0.05        | 1.21×    | 76.69  | 20      | 0.58%         |
| absl::flat_hash_map | 19.98 ±0.07        | 1.86×    | 50.05  | 20      | 0.48%         |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**32 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 18.35 ±0.07           | 1.00×    | 54.50  | 20      | 0.50% |
| khashl              | 19.04 ±0.19           | 1.04×    | 52.52  | 20      | 1.40% |
| hwire_table (8N)    | 19.87 ±0.11           | 1.08×    | 50.33  | 20      | 0.74% |
| hwire_table (4N)    | 19.94 ±0.14           | 1.09×    | 50.15  | 20      | 1.01% |
| hwire_table (2N)    | 20.38 ±0.10           | 1.11×    | 49.07  | 20      | 0.66% |
| absl::flat_hash_map | 21.44 ±0.14           | 1.17×    | 46.64  | 20      | 0.90% |

**64 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| khashl              | 17.81 ±0.06           | 1.00×    | 56.15  | 20      | 0.46% |
| CC                  | 17.83 ±0.12           | 1.00×    | 56.09  | 20      | 0.91% |
| hwire_table (4N)    | 19.61 ±0.10           | 1.10×    | 50.99  | 20      | 0.70% |
| hwire_table (8N)    | 19.63 ±0.10           | 1.10×    | 50.94  | 20      | 0.69% |
| hwire_table (2N)    | 19.95 ±0.13           | 1.12×    | 50.13  | 20      | 0.92% |
| absl::flat_hash_map | 21.15 ±0.07           | 1.19×    | 47.28  | 20      | 0.43% |

**128 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| khashl              | 19.56 ±0.10           | 1.00×    | 51.12  | 20      | 0.72%         |
| CC                  | 19.88 ±0.34           | 1.02×    | 50.30  | 30      | 1.85%         |
| hwire_table (8N)    | 21.28 ±0.19           | 1.09×    | 46.99  | 20      | 1.24%         |
| hwire_table (2N)    | 21.41 ±0.13           | 1.09×    | 46.71  | 20      | 0.88%         |
| hwire_table (4N)    | 22.19 ±2.97           | 1.13×    | 45.07  | 100     | 7.60% (unmet) |
| absl::flat_hash_map | 23.07 ±0.06           | 1.18×    | 43.35  | 20      | 0.37%         |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**32 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 7.67 ±0.06            | 1.00×    | 130.38 | 20      | 1.17%         |
| CC                  | 7.82 ±0.04            | 1.02×    | 127.88 | 20      | 0.67%         |
| hwire_table (4N)    | 8.53 ±0.59            | 1.11×    | 117.23 | 100     | 3.94% (unmet) |
| khashl              | 8.98 ±0.06            | 1.17×    | 111.36 | 20      | 0.89%         |
| absl::flat_hash_map | 10.54 ±0.47           | 1.37×    | 94.88  | 100     | 2.54% (unmet) |
| hwire_table (2N)    | 12.58 ±1.68           | 1.64×    | 79.49  | 100     | 7.58% (unmet) |

**64 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 7.78 ±0.06            | 1.00×    | 128.53 | 20      | 1.16% |
| CC                  | 7.81 ±0.03            | 1.00×    | 128.04 | 20      | 0.55% |
| hwire_table (4N)    | 8.03 ±0.06            | 1.03×    | 124.53 | 20      | 1.06% |
| khashl              | 9.78 ±0.04            | 1.26×    | 102.25 | 20      | 0.62% |
| hwire_table (2N)    | 10.07 ±0.08           | 1.29×    | 99.30  | 20      | 1.06% |
| absl::flat_hash_map | 10.65 ±0.05           | 1.37×    | 93.90  | 20      | 0.69% |

**128 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.80 ±0.03            | 1.00×    | 128.21 | 20      | 0.46% |
| hwire_table (8N)    | 8.00 ±0.09            | 1.03×    | 125.00 | 20      | 1.61% |
| hwire_table (4N)    | 8.58 ±0.07            | 1.10×    | 116.55 | 20      | 1.12% |
| khashl              | 9.44 ±0.08            | 1.21×    | 105.93 | 20      | 1.18% |
| hwire_table (2N)    | 9.76 ±0.08            | 1.25×    | 102.46 | 20      | 1.13% |
| absl::flat_hash_map | 10.86 ±0.11           | 1.39×    | 92.08  | 20      | 1.47% |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**32 keys**


| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (8N) †  | 0.383              | 1.00×    | 19.87 ±0.11               | Baseline                 |
| hwire_table (4N)    | 0.384              | 1.00×    | 19.94 ±0.14               | No crossover             |
| hwire_table (2N)    | 0.397              | 1.04×    | 20.38 ±0.10               | No crossover             |
| khashl              | 0.437              | 1.14×    | 19.04 ±0.19               | 67                       |
| CC †                | 0.480              | 1.25×    | 18.35 ±0.07               | 65                       |
| absl::flat_hash_map | 0.677              | 1.77×    | 21.44 ±0.14               | No crossover             |

**64 keys**


| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (4N)    | 0.746              | 1.00×    | 19.61 ±0.10               | Baseline                 |
| hwire_table (8N)    | 0.747              | 1.00×    | 19.63 ±0.10               | No crossover             |
| hwire_table (2N)    | 0.749              | 1.00×    | 19.95 ±0.13               | No crossover             |
| khashl              | 0.759              | 1.02×    | 17.81 ±0.06               | 9                        |
| CC                  | 0.903              | 1.21×    | 17.83 ±0.12               | 90                       |
| absl::flat_hash_map | 1.335              | 1.79×    | 21.15 ±0.07               | No crossover             |

**128 keys**


| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (4N) †  | 1.458              | 1.00×    | 22.19 ±2.97               | Baseline                 |
| khashl              | 1.470              | 1.01×    | 19.56 ±0.10               | 6                        |
| hwire_table (8N)    | 1.473              | 1.01×    | 21.28 ±0.19               | 18                       |
| hwire_table (2N)    | 1.487              | 1.02×    | 21.41 ±0.13               | 39                       |
| CC                  | 1.720              | 1.18×    | 19.88 ±0.34               | 115                      |
| absl::flat_hash_map | 2.643              | 1.81×    | 23.07 ±0.06               | No crossover             |


#### Build + Miss

**32 keys**


| Map                   | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| --------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (8N) †    | 0.371               | 1.00×    | 7.67 ±0.06                 | Baseline                 |
| hwire_table (4N) †    | 0.373               | 1.01×    | 8.53 ±0.59                 | No crossover             |
| hwire_table (2N) †    | 0.389               | 1.05×    | 12.58 ±1.68                | No crossover             |
| khashl                | 0.427               | 1.15×    | 8.98 ±0.06                 | No crossover             |
| CC †                  | 0.469               | 1.27×    | 7.82 ±0.04                 | No crossover             |
| absl::flat_hash_map † | 0.666               | 1.80×    | 10.54 ±0.47                | No crossover             |

**64 keys**


| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (8N)    | 0.735               | 1.00×    | 7.78 ±0.06                 | Baseline                 |
| hwire_table (4N)    | 0.735               | 1.00×    | 8.03 ±0.06                 | No crossover             |
| hwire_table (2N)    | 0.739               | 1.01×    | 10.07 ±0.08                | No crossover             |
| khashl              | 0.751               | 1.02×    | 9.78 ±0.04                 | No crossover             |
| CC                  | 0.893               | 1.22×    | 7.81 ±0.03                 | No crossover             |
| absl::flat_hash_map | 1.324               | 1.80×    | 10.65 ±0.05                | No crossover             |

**128 keys**


| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (4N)    | 1.444               | 1.00×    | 8.58 ±0.07                 | Baseline                 |
| khashl              | 1.460               | 1.01×    | 9.44 ±0.08                 | No crossover             |
| hwire_table (8N)    | 1.460               | 1.01×    | 8.00 ±0.09                 | 28                       |
| hwire_table (2N)    | 1.475               | 1.02×    | 9.76 ±0.08                 | No crossover             |
| CC                  | 1.708               | 1.18×    | 7.80 ±0.03                 | 340                      |
| absl::flat_hash_map | 2.631               | 1.82×    | 10.86 ±0.11                | No crossover             |


## Case-Sensitive Allocated Growth

Exact byte comparisons. Grow from an initial reservation of 32 keys to 256 unique keys with three additional extensions. Build includes initial allocation, initialization, insertion and expansion work, including ordinary heap allocations and any copying or rehashing.

Cleanup runs outside all timed intervals.


### Memory

Final live container/storage bytes; borrowed key/value contents and allocator metadata are excluded.

**256 keys**


| Map                 | Memory (bytes) | Relative | Bytes/key | Slot load factor | Extensions |
| ------------------- | -------------- | -------- | --------- | ---------------- | ---------- |
| hwire_table (2N)    | 10560          | 1.00×    | 41.25     | 50.00%           | 3          |
| hwire_table (4N)    | 11584          | 1.10×    | 45.25     | 25.00%           | 3          |
| hwire_table (8N)    | 13632          | 1.29×    | 53.25     | 12.50%           | 3          |
| absl::flat_hash_map | 25096          | 2.38×    | 98.03     | 50.10%           | 3          |
| khashl              | 28784          | 2.73×    | 112.44    | 50.00%           | 3          |
| CC                  | 29760          | 2.82×    | 116.25    | 50.00%           | 3          |


### Build

Initial acquisition and initialization plus all insertions; any expansion is included.

**256 keys**


| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 2551.73 ±26.17       | 1.00×    | 0.39   | 20      | 1.43% |
| hwire_table (2N)    | 2584.25 ±14.25       | 1.01×    | 0.39   | 20      | 0.77% |
| hwire_table (8N)    | 2601.33 ±69.25       | 1.02×    | 0.38   | 60      | 1.98% |
| khashl              | 7065.27 ±51.28       | 2.77×    | 0.14   | 20      | 1.01% |
| CC                  | 8954.26 ±57.15       | 3.51×    | 0.11   | 20      | 0.89% |
| absl::flat_hash_map | 9795.55 ±19.70       | 3.84×    | 0.10   | 20      | 0.28% |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**256 keys**


| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | ------------------ | -------- | ------ | ------- | ------------- |
| hwire_table (4N)    | 9.85 ±0.08         | 1.00×    | 101.52 | 20      | 1.19%         |
| hwire_table (8N)    | 10.05 ±0.59        | 1.02×    | 99.50  | 100     | 3.31% (unmet) |
| hwire_table (2N)    | 10.14 ±0.79        | 1.03×    | 98.62  | 100     | 4.44% (unmet) |
| khashl              | 27.17 ±2.72        | 2.76×    | 36.81  | 100     | 5.68% (unmet) |
| CC                  | 34.56 ±0.13        | 3.51×    | 28.94  | 20      | 0.52%         |
| absl::flat_hash_map | 38.01 ±0.07        | 3.86×    | 26.31  | 20      | 0.27%         |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**256 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 12.36 ±0.21           | 1.00×    | 80.91  | 40      | 1.58% |
| khashl              | 12.67 ±0.36           | 1.03×    | 78.93  | 70      | 1.94% |
| CC                  | 12.71 ±0.05           | 1.03×    | 78.68  | 20      | 0.55% |
| hwire_table (4N)    | 12.93 ±0.10           | 1.05×    | 77.34  | 20      | 1.12% |
| hwire_table (2N)    | 15.44 ±0.20           | 1.25×    | 64.77  | 20      | 1.84% |
| absl::flat_hash_map | 16.36 ±0.06           | 1.32×    | 61.12  | 20      | 0.53% |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**256 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| khashl              | 7.74 ±0.17            | 1.00×    | 129.20 | 40      | 2.00% |
| CC                  | 7.96 ±0.07            | 1.03×    | 125.63 | 20      | 1.20% |
| absl::flat_hash_map | 9.79 ±0.06            | 1.26×    | 102.15 | 20      | 0.90% |
| hwire_table (8N)    | 9.86 ±0.13            | 1.27×    | 101.42 | 30      | 1.44% |
| hwire_table (4N)    | 11.57 ±0.12           | 1.49×    | 86.43  | 20      | 1.47% |
| hwire_table (2N)    | 16.99 ±0.41           | 2.20×    | 58.86  | 60      | 1.82% |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**256 keys**


| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (4N)    | 2.565              | 1.00×    | 12.93 ±0.10               | Baseline                 |
| hwire_table (2N)    | 2.600              | 1.01×    | 15.44 ±0.20               | No crossover             |
| hwire_table (8N)    | 2.614              | 1.02×    | 12.36 ±0.21               | 88                       |
| khashl              | 7.078              | 2.76×    | 12.67 ±0.36               | 17,360                   |
| CC                  | 8.967              | 3.50×    | 12.71 ±0.05               | 29,103                   |
| absl::flat_hash_map | 9.812              | 3.83×    | 16.36 ±0.06               | No crossover             |


#### Build + Miss

**256 keys**


| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (4N)    | 2.563               | 1.00×    | 11.57 ±0.12                | Baseline                 |
| hwire_table (2N)    | 2.601               | 1.01×    | 16.99 ±0.41                | No crossover             |
| hwire_table (8N)    | 2.611               | 1.02×    | 9.86 ±0.13                 | 30                       |
| khashl              | 7.073               | 2.76×    | 7.74 ±0.17                 | 1,179                    |
| CC                  | 8.962               | 3.50×    | 7.96 ±0.07                 | 1,774                    |
| absl::flat_hash_map | 9.805               | 3.83×    | 9.79 ±0.06                 | 4,070                    |


## Case-Insensitive Allocated Growth

ASCII case-insensitive comparisons. Grow from an initial reservation of 32 keys to 256 unique keys with three additional extensions. Build includes initial allocation, initialization, insertion and expansion work, including ordinary heap allocations and any copying or rehashing.

Cleanup runs outside all timed intervals.


### Memory

Final live container/storage bytes; borrowed key/value contents and allocator metadata are excluded.

**256 keys**


| Map                 | Memory (bytes) | Relative | Bytes/key | Slot load factor | Extensions |
| ------------------- | -------------- | -------- | --------- | ---------------- | ---------- |
| hwire_table (2N)    | 10560          | 1.00×    | 41.25     | 50.00%           | 3          |
| hwire_table (4N)    | 11584          | 1.10×    | 45.25     | 25.00%           | 3          |
| hwire_table (8N)    | 13632          | 1.29×    | 53.25     | 12.50%           | 3          |
| absl::flat_hash_map | 25096          | 2.38×    | 98.03     | 50.10%           | 3          |
| khashl              | 28784          | 2.73×    | 112.44    | 50.00%           | 3          |
| CC                  | 29760          | 2.82×    | 116.25    | 50.00%           | 3          |


### Build

Initial acquisition and initialization plus all insertions; any expansion is included.

**256 keys**


| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 3087.03 ±28.77       | 1.00×    | 0.32   | 20      | 1.30% |
| hwire_table (2N)    | 3134.08 ±22.60       | 1.02×    | 0.32   | 20      | 1.01% |
| hwire_table (8N)    | 3141.11 ±22.03       | 1.02×    | 0.32   | 20      | 0.98% |
| khashl              | 8387.78 ±40.63       | 2.72×    | 0.12   | 20      | 0.68% |
| CC                  | 9597.82 ±38.98       | 3.11×    | 0.10   | 20      | 0.57% |
| absl::flat_hash_map | 9687.17 ±52.50       | 3.14×    | 0.10   | 20      | 0.76% |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**256 keys**


| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 11.93 ±0.09        | 1.00×    | 83.82  | 20      | 1.05% |
| hwire_table (2N)    | 12.07 ±0.05        | 1.01×    | 82.85  | 20      | 0.56% |
| hwire_table (8N)    | 12.14 ±0.06        | 1.02×    | 82.37  | 20      | 0.67% |
| khashl              | 32.28 ±0.15        | 2.71×    | 30.98  | 20      | 0.63% |
| CC                  | 37.17 ±0.13        | 3.12×    | 26.90  | 20      | 0.50% |
| absl::flat_hash_map | 37.55 ±0.09        | 3.15×    | 26.63  | 20      | 0.33% |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**256 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| CC                  | 20.87 ±0.13           | 1.00×    | 47.92  | 20      | 0.85%         |
| khashl              | 21.30 ±0.15           | 1.02×    | 46.95  | 20      | 0.98%         |
| hwire_table (8N)    | 24.05 ±1.85           | 1.15×    | 41.58  | 100     | 4.35% (unmet) |
| absl::flat_hash_map | 24.09 ±0.15           | 1.15×    | 41.51  | 20      | 0.86%         |
| hwire_table (4N)    | 24.30 ±0.24           | 1.16×    | 41.15  | 20      | 1.36%         |
| hwire_table (2N)    | 27.60 ±2.44           | 1.32×    | 36.23  | 100     | 5.02% (unmet) |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**256 keys**


| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| CC                  | 8.11 ±0.03            | 1.00×    | 123.30 | 20      | 0.57%         |
| khashl              | 10.57 ±0.07           | 1.30×    | 94.61  | 20      | 0.96%         |
| absl::flat_hash_map | 11.51 ±0.31           | 1.42×    | 86.88  | 60      | 1.99%         |
| hwire_table (8N)    | 11.85 ±0.08           | 1.46×    | 84.39  | 20      | 0.93%         |
| hwire_table (4N)    | 13.23 ±1.39           | 1.63×    | 75.59  | 100     | 5.94% (unmet) |
| hwire_table (2N)    | 21.21 ±0.65           | 2.62×    | 47.15  | 80      | 1.95%         |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**256 keys**


| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (4N)    | 3.111              | 1.00×    | 24.30 ±0.24               | Baseline                 |
| hwire_table (2N) †  | 3.162              | 1.02×    | 27.60 ±2.44               | No crossover             |
| hwire_table (8N) †  | 3.165              | 1.02×    | 24.05 ±1.85               | 217                      |
| khashl              | 8.409              | 2.70×    | 21.30 ±0.15               | 1,767                    |
| CC                  | 9.619              | 3.09×    | 20.87 ±0.13               | 1,899                    |
| absl::flat_hash_map | 9.711              | 3.12×    | 24.09 ±0.15               | 31,430                   |


#### Build + Miss

**256 keys**


| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (4N) †  | 3.100               | 1.00×    | 13.23 ±1.39                | Baseline                 |
| hwire_table (8N)    | 3.153               | 1.02×    | 11.85 ±0.08                | 40                       |
| hwire_table (2N)    | 3.155               | 1.02×    | 21.21 ±0.65                | No crossover             |
| khashl              | 8.398               | 2.71×    | 10.57 ±0.07                | 1,993                    |
| CC                  | 9.606               | 3.10×    | 8.11 ±0.03                 | 1,272                    |
| absl::flat_hash_map | 9.699               | 3.13×    | 11.51 ±0.31                | 3,838                    |
