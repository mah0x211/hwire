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
date                 : 2026-10-08T08:16:19+09:00
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
hash                 : AES
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
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
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
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
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
| khashl              | 332.39 ±1.29         | 1.00×    | 3.01   | 20      | 0.54% |
| CC                  | 413.61 ±1.43         | 1.24×    | 2.42   | 20      | 0.48% |
| hwire_table (4N)    | 427.89 ±4.42         | 1.29×    | 2.34   | 20      | 1.44% |
| hwire_table (8N)    | 429.16 ±4.36         | 1.29×    | 2.33   | 20      | 1.42% |
| hwire_table (2N)    | 433.40 ±2.47         | 1.30×    | 2.31   | 20      | 0.80% |
| absl::flat_hash_map | 590.45 ±8.76         | 1.78×    | 1.69   | 30      | 1.62% |

**64 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 633.36 ±3.43         | 1.00×    | 1.58   | 20      | 0.76% |
| hwire_table (8N)    | 819.18 ±9.86         | 1.29×    | 1.22   | 20      | 1.68% |
| hwire_table (4N)    | 826.25 ±4.92         | 1.30×    | 1.21   | 20      | 0.83% |
| CC                  | 828.55 ±11.60        | 1.31×    | 1.21   | 30      | 1.53% |
| hwire_table (2N)    | 828.68 ±15.01        | 1.31×    | 1.21   | 30      | 1.98% |
| absl::flat_hash_map | 1207.55 ±6.57        | 1.91×    | 0.83   | 20      | 0.76% |

**128 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 1251.20 ±7.44        | 1.00×    | 0.80   | 20      | 0.83% |
| CC                  | 1596.03 ±6.43        | 1.28×    | 0.63   | 20      | 0.56% |
| hwire_table (2N)    | 1623.65 ±19.63       | 1.30×    | 0.62   | 20      | 1.69% |
| hwire_table (8N)    | 1628.54 ±19.80       | 1.30×    | 0.61   | 20      | 1.70% |
| hwire_table (4N)    | 1628.94 ±12.72       | 1.30×    | 0.61   | 20      | 1.09% |
| absl::flat_hash_map | 2395.13 ±14.68       | 1.91×    | 0.42   | 20      | 0.86% |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**32 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 8.31 ±0.07         | 1.00×    | 120.34 | 20      | 1.15% |
| CC                  | 11.90 ±0.04        | 1.43×    | 84.03  | 20      | 0.52% |
| hwire_table (8N)    | 12.37 ±0.25        | 1.49×    | 80.84  | 40      | 1.88% |
| hwire_table (4N)    | 12.48 ±0.35        | 1.50×    | 80.13  | 70      | 1.90% |
| hwire_table (2N)    | 12.62 ±0.26        | 1.52×    | 79.24  | 40      | 1.91% |
| absl::flat_hash_map | 17.09 ±0.24        | 2.06×    | 58.51  | 30      | 1.56% |

**64 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 8.72 ±0.04         | 1.00×    | 114.68 | 20      | 0.58% |
| hwire_table (8N)    | 12.32 ±0.16        | 1.41×    | 81.17  | 20      | 1.80% |
| CC                  | 12.37 ±0.05        | 1.42×    | 80.84  | 20      | 0.60% |
| hwire_table (2N)    | 12.40 ±0.24        | 1.42×    | 80.65  | 40      | 1.78% |
| hwire_table (4N)    | 12.47 ±0.07        | 1.43×    | 80.19  | 20      | 0.82% |
| absl::flat_hash_map | 18.10 ±0.11        | 2.08×    | 55.25  | 20      | 0.87% |

**128 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 9.20 ±0.05         | 1.00×    | 108.70 | 20      | 0.72% |
| CC                  | 12.19 ±0.07        | 1.32×    | 82.03  | 20      | 0.79% |
| hwire_table (8N)    | 12.53 ±0.08        | 1.36×    | 79.81  | 20      | 0.95% |
| hwire_table (2N)    | 12.55 ±0.18        | 1.36×    | 79.68  | 20      | 1.95% |
| hwire_table (4N)    | 12.55 ±0.08        | 1.36×    | 79.68  | 20      | 0.88% |
| absl::flat_hash_map | 18.37 ±0.11        | 2.00×    | 54.44  | 20      | 0.81% |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 8.37 ±0.08            | 1.00×    | 119.47 | 20      | 1.27%         |
| hwire_table (2N)    | 8.72 ±0.09            | 1.04×    | 114.68 | 20      | 1.50%         |
| hwire_table (4N)    | 8.94 ±0.84            | 1.07×    | 111.86 | 100     | 5.33% (unmet) |
| khashl              | 9.54 ±0.16            | 1.14×    | 104.82 | 30      | 1.86%         |
| CC                  | 9.87 ±0.19            | 1.18×    | 101.32 | 40      | 1.79%         |
| absl::flat_hash_map | 13.46 ±0.16           | 1.61×    | 74.29  | 20      | 1.66%         |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (4N)    | 8.44 ±0.04            | 1.00×    | 118.48 | 20      | 0.61%         |
| hwire_table (2N)    | 8.61 ±0.04            | 1.02×    | 116.14 | 20      | 0.58%         |
| hwire_table (8N)    | 8.90 ±0.81            | 1.05×    | 112.36 | 100     | 5.19% (unmet) |
| khashl              | 10.68 ±1.10           | 1.27×    | 93.63  | 100     | 5.83% (unmet) |
| CC                  | 10.75 ±0.20           | 1.27×    | 93.02  | 30      | 1.99%         |
| absl::flat_hash_map | 14.06 ±0.17           | 1.67×    | 71.12  | 20      | 1.74%         |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| hwire_table (2N)    | 9.28 ±0.02            | 1.00×    | 107.76 | 20      | 0.33% |
| hwire_table (4N)    | 9.39 ±0.18            | 1.01×    | 106.50 | 40      | 1.83% |
| hwire_table (8N)    | 9.54 ±0.08            | 1.03×    | 104.82 | 20      | 1.16% |
| CC                  | 11.47 ±0.05           | 1.24×    | 87.18  | 20      | 0.55% |
| khashl              | 11.89 ±0.05           | 1.28×    | 84.10  | 20      | 0.61% |
| absl::flat_hash_map | 15.10 ±0.19           | 1.63×    | 66.23  | 20      | 1.74% |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 5.15 ±0.02            | 1.00×    | 194.17 | 20      | 0.67% |
| hwire_table (4N)    | 5.36 ±0.03            | 1.04×    | 186.57 | 20      | 0.70% |
| hwire_table (2N)    | 6.53 ±0.02            | 1.27×    | 153.14 | 20      | 0.46% |
| CC                  | 6.81 ±0.03            | 1.32×    | 146.84 | 20      | 0.63% |
| khashl              | 7.71 ±0.10            | 1.50×    | 129.70 | 20      | 1.79% |
| absl::flat_hash_map | 8.29 ±0.02            | 1.61×    | 120.63 | 20      | 0.37% |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 5.12 ±0.02            | 1.00×    | 195.31 | 20      | 0.46%         |
| hwire_table (4N)    | 5.80 ±0.78            | 1.13×    | 172.41 | 100     | 7.58% (unmet) |
| hwire_table (2N)    | 6.37 ±0.04            | 1.24×    | 156.99 | 20      | 0.78%         |
| CC                  | 6.70 ±0.67            | 1.31×    | 149.25 | 100     | 5.71% (unmet) |
| khashl              | 7.75 ±0.03            | 1.51×    | 129.03 | 20      | 0.50%         |
| absl::flat_hash_map | 8.70 ±0.04            | 1.70×    | 114.94 | 20      | 0.64%         |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 5.17 ±0.06            | 1.00×    | 193.42 | 20      | 1.59%         |
| hwire_table (4N)    | 5.46 ±0.40            | 1.06×    | 183.15 | 100     | 4.16% (unmet) |
| hwire_table (2N)    | 6.23 ±0.06            | 1.21×    | 160.51 | 20      | 1.30%         |
| CC                  | 7.30 ±0.04            | 1.41×    | 136.99 | 20      | 0.73%         |
| khashl              | 8.08 ±0.11            | 1.56×    | 123.76 | 20      | 1.83%         |
| absl::flat_hash_map | 9.34 ±1.05            | 1.81×    | 107.07 | 100     | 6.38% (unmet) |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**32 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 0.342              | 1.00×    | 9.54 ±0.16                | Baseline                 |
| CC                  | 0.423              | 1.24×    | 9.87 ±0.19                | No crossover             |
| hwire_table (4N) †  | 0.437              | 1.28×    | 8.94 ±0.84                | 160                      |
| hwire_table (8N)    | 0.438              | 1.28×    | 8.37 ±0.08                | 83                       |
| hwire_table (2N)    | 0.442              | 1.29×    | 8.72 ±0.09                | 124                      |
| absl::flat_hash_map | 0.604              | 1.77×    | 13.46 ±0.16               | No crossover             |

**64 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl †            | 0.644              | 1.00×    | 10.68 ±1.10               | Baseline                 |
| hwire_table (8N) †  | 0.828              | 1.29×    | 8.90 ±0.81                | 105                      |
| hwire_table (4N)    | 0.835              | 1.30×    | 8.44 ±0.04                | 87                       |
| hwire_table (2N)    | 0.837              | 1.30×    | 8.61 ±0.04                | 95                       |
| CC                  | 0.839              | 1.30×    | 10.75 ±0.20               | No crossover             |
| absl::flat_hash_map | 1.222              | 1.90×    | 14.06 ±0.17               | No crossover             |

**128 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 1.263              | 1.00×    | 11.89 ±0.05               | Baseline                 |
| CC                  | 1.607              | 1.27×    | 11.47 ±0.05               | 822                      |
| hwire_table (2N)    | 1.633              | 1.29×    | 9.28 ±0.02                | 143                      |
| hwire_table (8N)    | 1.638              | 1.30×    | 9.54 ±0.08                | 161                      |
| hwire_table (4N)    | 1.638              | 1.30×    | 9.39 ±0.18                | 152                      |
| absl::flat_hash_map | 2.410              | 1.91×    | 15.10 ±0.19               | No crossover             |


#### Build + Miss

**32 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.340               | 1.00×    | 7.71 ±0.10                 | Baseline                 |
| CC                  | 0.420               | 1.24×    | 6.81 ±0.03                 | 91                       |
| hwire_table (4N)    | 0.433               | 1.27×    | 5.36 ±0.03                 | 41                       |
| hwire_table (8N)    | 0.434               | 1.28×    | 5.15 ±0.02                 | 38                       |
| hwire_table (2N)    | 0.440               | 1.29×    | 6.53 ±0.02                 | 86                       |
| absl::flat_hash_map | 0.599               | 1.76×    | 8.29 ±0.02                 | No crossover             |

**64 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.641               | 1.00×    | 7.75 ±0.03                 | Baseline                 |
| hwire_table (8N)    | 0.824               | 1.29×    | 5.12 ±0.02                 | 71                       |
| hwire_table (4N) †  | 0.832               | 1.30×    | 5.80 ±0.78                 | 99                       |
| hwire_table (2N)    | 0.835               | 1.30×    | 6.37 ±0.04                 | 142                      |
| CC †                | 0.835               | 1.30×    | 6.70 ±0.67                 | 186                      |
| absl::flat_hash_map | 1.216               | 1.90×    | 8.70 ±0.04                 | No crossover             |

**128 keys**

| Map                   | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| --------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl                | 1.259               | 1.00×    | 8.08 ±0.11                 | Baseline                 |
| CC                    | 1.603               | 1.27×    | 7.30 ±0.04                 | 443                      |
| hwire_table (2N)      | 1.630               | 1.29×    | 6.23 ±0.06                 | 202                      |
| hwire_table (8N)      | 1.634               | 1.30×    | 5.17 ±0.06                 | 130                      |
| hwire_table (4N) †    | 1.634               | 1.30×    | 5.46 ±0.40                 | 145                      |
| absl::flat_hash_map † | 2.404               | 1.91×    | 9.34 ±1.05                 | No crossover             |


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
| khashl              | 401.23 ±22.97        | 1.00×    | 2.49   | 100     | 3.25% (unmet) |
| hwire_table (4N)    | 438.25 ±5.24         | 1.09×    | 2.28   | 20      | 1.67%         |
| hwire_table (8N)    | 439.89 ±2.93         | 1.10×    | 2.27   | 20      | 0.93%         |
| hwire_table (2N)    | 450.96 ±7.30         | 1.12×    | 2.22   | 40      | 1.50%         |
| CC                  | 485.52 ±17.42        | 1.21×    | 2.06   | 100     | 2.03% (unmet) |
| absl::flat_hash_map | 641.33 ±4.68         | 1.60×    | 1.56   | 20      | 1.02%         |

**64 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 735.27 ±1.99         | 1.00×    | 1.36   | 20      | 0.38% |
| hwire_table (4N)    | 825.51 ±8.78         | 1.12×    | 1.21   | 20      | 1.49% |
| hwire_table (8N)    | 826.48 ±5.25         | 1.12×    | 1.21   | 20      | 0.89% |
| hwire_table (2N)    | 849.51 ±15.38        | 1.16×    | 1.18   | 40      | 1.68% |
| CC                  | 900.37 ±4.17         | 1.22×    | 1.11   | 20      | 0.65% |
| absl::flat_hash_map | 1285.45 ±10.83       | 1.75×    | 0.78   | 20      | 1.18% |

**128 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | -------------------- | -------- | ------ | ------- | ------------- |
| khashl              | 1462.85 ±80.67       | 1.00×    | 0.68   | 100     | 3.13% (unmet) |
| hwire_table (4N)    | 1649.76 ±8.42        | 1.13×    | 0.61   | 20      | 0.71%         |
| hwire_table (2N)    | 1650.46 ±6.51        | 1.13×    | 0.61   | 20      | 0.55%         |
| hwire_table (8N)    | 1656.43 ±12.69       | 1.13×    | 0.60   | 20      | 1.07%         |
| CC                  | 1721.25 ±16.44       | 1.18×    | 0.58   | 20      | 1.34%         |
| absl::flat_hash_map | 2653.72 ±11.71       | 1.81×    | 0.38   | 20      | 0.62%         |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**32 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | ------------------ | -------- | ------ | ------- | ------------- |
| khashl              | 10.42 ±0.06        | 1.00×    | 95.97  | 20      | 0.84%         |
| hwire_table (8N)    | 12.84 ±0.14        | 1.23×    | 77.88  | 20      | 1.56%         |
| hwire_table (4N)    | 12.90 ±0.07        | 1.24×    | 77.52  | 20      | 0.78%         |
| hwire_table (2N)    | 13.19 ±0.07        | 1.27×    | 75.82  | 20      | 0.74%         |
| CC                  | 13.84 ±0.59        | 1.33×    | 72.25  | 100     | 2.40% (unmet) |
| absl::flat_hash_map | 18.66 ±0.17        | 1.79×    | 53.59  | 20      | 1.29%         |

**64 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | ------------------ | -------- | ------ | ------- | ------------- |
| khashl              | 10.56 ±0.05        | 1.00×    | 94.70  | 20      | 0.64%         |
| hwire_table (8N)    | 12.38 ±0.13        | 1.17×    | 80.78  | 20      | 1.42%         |
| hwire_table (4N)    | 12.54 ±0.45        | 1.19×    | 79.74  | 100     | 2.04% (unmet) |
| hwire_table (2N)    | 12.85 ±0.20        | 1.22×    | 77.82  | 30      | 1.74%         |
| CC                  | 13.44 ±0.09        | 1.27×    | 74.40  | 20      | 0.99%         |
| absl::flat_hash_map | 19.51 ±0.14        | 1.85×    | 51.26  | 20      | 0.97%         |

**128 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 10.80 ±0.10        | 1.00×    | 92.59  | 20      | 1.25% |
| hwire_table (4N)    | 12.60 ±0.10        | 1.17×    | 79.37  | 20      | 1.07% |
| hwire_table (8N)    | 12.60 ±0.09        | 1.17×    | 79.37  | 20      | 1.01% |
| hwire_table (2N)    | 12.72 ±0.17        | 1.18×    | 78.62  | 20      | 1.83% |
| CC                  | 15.13 ±0.07        | 1.40×    | 66.09  | 20      | 0.65% |
| absl::flat_hash_map | 20.36 ±0.19        | 1.89×    | 49.12  | 20      | 1.33% |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| khashl              | 17.81 ±0.06           | 1.00×    | 56.15  | 20      | 0.51% |
| CC                  | 18.37 ±0.25           | 1.03×    | 54.44  | 20      | 1.93% |
| hwire_table (8N)    | 18.43 ±0.11           | 1.03×    | 54.26  | 20      | 0.82% |
| hwire_table (4N)    | 18.56 ±0.14           | 1.04×    | 53.88  | 20      | 1.06% |
| hwire_table (2N)    | 19.01 ±0.14           | 1.07×    | 52.60  | 20      | 1.01% |
| absl::flat_hash_map | 21.37 ±0.13           | 1.20×    | 46.79  | 20      | 0.84% |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| khashl              | 17.95 ±0.07           | 1.00×    | 55.71  | 20      | 0.54% |
| CC                  | 17.97 ±0.09           | 1.00×    | 55.65  | 20      | 0.68% |
| hwire_table (8N)    | 18.13 ±0.09           | 1.01×    | 55.16  | 20      | 0.73% |
| hwire_table (4N)    | 18.15 ±0.06           | 1.01×    | 55.10  | 20      | 0.50% |
| hwire_table (2N)    | 18.74 ±0.53           | 1.04×    | 53.36  | 80      | 1.82% |
| absl::flat_hash_map | 21.18 ±0.09           | 1.18×    | 47.21  | 20      | 0.60% |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 19.35 ±0.25           | 1.00×    | 51.68  | 30      | 1.42% |
| hwire_table (2N)    | 19.42 ±0.14           | 1.00×    | 51.49  | 20      | 1.02% |
| hwire_table (8N)    | 19.42 ±0.09           | 1.00×    | 51.49  | 20      | 0.66% |
| CC                  | 19.71 ±0.09           | 1.02×    | 50.74  | 20      | 0.60% |
| khashl              | 19.96 ±0.47           | 1.03×    | 50.10  | 60      | 1.74% |
| absl::flat_hash_map | 22.74 ±0.13           | 1.18×    | 43.98  | 20      | 0.82% |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 7.04 ±0.06            | 1.00×    | 142.05 | 20      | 1.25% |
| hwire_table (4N)    | 7.69 ±0.04            | 1.09×    | 130.04 | 20      | 0.70% |
| CC                  | 7.80 ±0.03            | 1.11×    | 128.21 | 20      | 0.55% |
| khashl              | 8.21 ±0.03            | 1.17×    | 121.80 | 20      | 0.53% |
| absl::flat_hash_map | 10.63 ±0.07           | 1.51×    | 94.07  | 20      | 0.88% |
| hwire_table (2N)    | 11.37 ±0.04           | 1.62×    | 87.95  | 20      | 0.49% |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 7.15 ±0.03            | 1.00×    | 139.86 | 20      | 0.64%         |
| hwire_table (4N)    | 7.48 ±0.59            | 1.05×    | 133.69 | 100     | 4.44% (unmet) |
| CC                  | 7.63 ±0.07            | 1.07×    | 131.06 | 20      | 1.36%         |
| hwire_table (2N)    | 9.37 ±0.04            | 1.31×    | 106.72 | 20      | 0.66%         |
| khashl              | 9.72 ±0.07            | 1.36×    | 102.88 | 20      | 0.98%         |
| absl::flat_hash_map | 10.97 ±0.11           | 1.53×    | 91.16  | 20      | 1.39%         |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| hwire_table (8N)    | 7.31 ±0.06            | 1.00×    | 136.80 | 20      | 1.13%         |
| CC                  | 7.70 ±0.26            | 1.05×    | 129.87 | 100     | 1.94%         |
| hwire_table (4N)    | 7.72 ±0.09            | 1.06×    | 129.53 | 20      | 1.64%         |
| hwire_table (2N)    | 8.90 ±0.08            | 1.22×    | 112.36 | 20      | 1.23%         |
| khashl              | 9.23 ±0.34            | 1.26×    | 108.34 | 100     | 2.09% (unmet) |
| absl::flat_hash_map | 10.92 ±0.11           | 1.49×    | 91.58  | 20      | 1.40%         |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**32 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl †            | 0.419              | 1.00×    | 17.81 ±0.06               | Baseline                 |
| hwire_table (4N)    | 0.457              | 1.09×    | 18.56 ±0.14               | No crossover             |
| hwire_table (8N)    | 0.458              | 1.09×    | 18.43 ±0.11               | No crossover             |
| hwire_table (2N)    | 0.470              | 1.12×    | 19.01 ±0.14               | No crossover             |
| CC †                | 0.504              | 1.20×    | 18.37 ±0.25               | No crossover             |
| absl::flat_hash_map | 0.663              | 1.58×    | 21.37 ±0.13               | No crossover             |

**64 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 0.753              | 1.00×    | 17.95 ±0.07               | Baseline                 |
| hwire_table (4N)    | 0.844              | 1.12×    | 18.15 ±0.06               | No crossover             |
| hwire_table (8N)    | 0.845              | 1.12×    | 18.13 ±0.09               | No crossover             |
| hwire_table (2N)    | 0.868              | 1.15×    | 18.74 ±0.53               | No crossover             |
| CC                  | 0.918              | 1.22×    | 17.97 ±0.09               | No crossover             |
| absl::flat_hash_map | 1.307              | 1.73×    | 21.18 ±0.09               | No crossover             |

**128 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl †            | 1.483              | 1.00×    | 19.96 ±0.47               | Baseline                 |
| hwire_table (4N)    | 1.669              | 1.13×    | 19.35 ±0.25               | 307                      |
| hwire_table (2N)    | 1.670              | 1.13×    | 19.42 ±0.14               | 348                      |
| hwire_table (8N)    | 1.676              | 1.13×    | 19.42 ±0.09               | 359                      |
| CC                  | 1.741              | 1.17×    | 19.71 ±0.09               | 1,034                    |
| absl::flat_hash_map | 2.676              | 1.80×    | 22.74 ±0.13               | No crossover             |


#### Build + Miss

**32 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl †            | 0.409               | 1.00×    | 8.21 ±0.03                 | Baseline                 |
| hwire_table (4N)    | 0.446               | 1.09×    | 7.69 ±0.04                 | 72                       |
| hwire_table (8N)    | 0.447               | 1.09×    | 7.04 ±0.06                 | 34                       |
| hwire_table (2N)    | 0.462               | 1.13×    | 11.37 ±0.04                | No crossover             |
| CC †                | 0.493               | 1.20×    | 7.80 ±0.03                 | 206                      |
| absl::flat_hash_map | 0.652               | 1.59×    | 10.63 ±0.07                | No crossover             |

**64 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.745               | 1.00×    | 9.72 ±0.07                 | Baseline                 |
| hwire_table (4N) †  | 0.833               | 1.12×    | 7.48 ±0.59                 | 41                       |
| hwire_table (8N)    | 0.834               | 1.12×    | 7.15 ±0.03                 | 36                       |
| hwire_table (2N)    | 0.859               | 1.15×    | 9.37 ±0.04                 | 327                      |
| CC                  | 0.908               | 1.22×    | 7.63 ±0.07                 | 79                       |
| absl::flat_hash_map | 1.296               | 1.74×    | 10.97 ±0.11                | No crossover             |

**128 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl †            | 1.472               | 1.00×    | 9.23 ±0.34                 | Baseline                 |
| hwire_table (4N)    | 1.657               | 1.13×    | 7.72 ±0.09                 | 124                      |
| hwire_table (2N)    | 1.659               | 1.13×    | 8.90 ±0.08                 | 569                      |
| hwire_table (8N)    | 1.664               | 1.13×    | 7.31 ±0.06                 | 101                      |
| CC                  | 1.729               | 1.17×    | 7.70 ±0.26                 | 169                      |
| absl::flat_hash_map | 2.665               | 1.81×    | 10.92 ±0.11                | No crossover             |


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
| hwire_table (8N)    | 3418.95 ±9.77        | 1.00×    | 0.29   | 20      | 0.40% |
| hwire_table (4N)    | 3438.29 ±14.61       | 1.01×    | 0.29   | 20      | 0.59% |
| hwire_table (2N)    | 3489.62 ±32.16       | 1.02×    | 0.29   | 20      | 1.29% |
| khashl              | 6827.20 ±46.77       | 2.00×    | 0.15   | 20      | 0.96% |
| CC                  | 8510.69 ±238.67      | 2.49×    | 0.12   | 70      | 1.92% |
| absl::flat_hash_map | 9665.52 ±22.54       | 2.83×    | 0.10   | 20      | 0.33% |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**256 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 13.25 ±0.07        | 1.00×    | 75.47  | 20      | 0.79% |
| hwire_table (4N)    | 13.36 ±0.07        | 1.01×    | 74.85  | 20      | 0.74% |
| hwire_table (2N)    | 13.47 ±0.05        | 1.02×    | 74.24  | 20      | 0.50% |
| khashl              | 26.38 ±0.43        | 1.99×    | 37.91  | 30      | 1.79% |
| CC                  | 32.69 ±0.72        | 2.47×    | 30.59  | 50      | 1.81% |
| absl::flat_hash_map | 37.59 ±0.20        | 2.84×    | 26.60  | 20      | 0.76% |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 11.58 ±0.06           | 1.00×    | 86.36  | 20      | 0.67% |
| hwire_table (4N)    | 11.92 ±0.09           | 1.03×    | 83.89  | 20      | 1.02% |
| khashl              | 12.57 ±0.30           | 1.09×    | 79.55  | 50      | 1.97% |
| CC                  | 12.60 ±0.11           | 1.09×    | 79.37  | 20      | 1.17% |
| hwire_table (2N)    | 14.65 ±0.33           | 1.27×    | 68.26  | 50      | 1.85% |
| absl::flat_hash_map | 16.23 ±0.15           | 1.40×    | 61.61  | 20      | 1.29% |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| CC                  | 7.71 ±0.47            | 1.00×    | 129.70 | 100     | 3.46% (unmet) |
| khashl              | 7.74 ±0.23            | 1.00×    | 129.20 | 80      | 1.90%         |
| hwire_table (8N)    | 8.53 ±0.60            | 1.11×    | 117.23 | 100     | 3.98% (unmet) |
| hwire_table (4N)    | 9.71 ±0.07            | 1.26×    | 102.99 | 20      | 1.05%         |
| absl::flat_hash_map | 9.77 ±0.05            | 1.27×    | 102.35 | 20      | 0.64%         |
| hwire_table (2N)    | 14.68 ±0.47           | 1.90×    | 68.12  | 90      | 1.93%         |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**256 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (8N)    | 3.431              | 1.00×    | 11.58 ±0.06               | Baseline                 |
| hwire_table (4N)    | 3.450              | 1.01×    | 11.92 ±0.09               | No crossover             |
| hwire_table (2N)    | 3.504              | 1.02×    | 14.65 ±0.33               | No crossover             |
| khashl              | 6.840              | 1.99×    | 12.57 ±0.30               | No crossover             |
| CC                  | 8.523              | 2.48×    | 12.60 ±0.11               | No crossover             |
| absl::flat_hash_map | 9.682              | 2.82×    | 16.23 ±0.15               | No crossover             |


#### Build + Miss

**256 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (8N) †  | 3.427               | 1.00×    | 8.53 ±0.60                 | Baseline                 |
| hwire_table (4N)    | 3.448               | 1.01×    | 9.71 ±0.07                 | No crossover             |
| hwire_table (2N)    | 3.504               | 1.02×    | 14.68 ±0.47                | No crossover             |
| khashl              | 6.835               | 1.99×    | 7.74 ±0.23                 | 4,315                    |
| CC †                | 8.518               | 2.49×    | 7.71 ±0.47                 | 6,210                    |
| absl::flat_hash_map | 9.675               | 2.82×    | 9.77 ±0.05                 | No crossover             |


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
| hwire_table (2N)    | 3589.58 ±44.56       | 1.00×    | 0.28   | 20      | 1.74% |
| hwire_table (8N)    | 3594.99 ±50.37       | 1.00×    | 0.28   | 30      | 1.53% |
| hwire_table (4N)    | 3601.57 ±87.67       | 1.00×    | 0.28   | 60      | 1.81% |
| khashl              | 8138.87 ±62.53       | 2.27×    | 0.12   | 20      | 1.07% |
| CC                  | 9353.74 ±162.03      | 2.61×    | 0.11   | 30      | 1.90% |
| absl::flat_hash_map | 9659.19 ±38.72       | 2.69×    | 0.10   | 20      | 0.56% |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**256 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 13.87 ±0.04        | 1.00×    | 72.10  | 20      | 0.42% |
| hwire_table (2N)    | 13.88 ±0.06        | 1.00×    | 72.05  | 20      | 0.63% |
| hwire_table (8N)    | 13.92 ±0.12        | 1.00×    | 71.84  | 20      | 1.18% |
| khashl              | 31.40 ±0.10        | 2.26×    | 31.85  | 20      | 0.46% |
| CC                  | 36.00 ±0.62        | 2.60×    | 27.78  | 30      | 1.88% |
| absl::flat_hash_map | 37.43 ±0.15        | 2.70×    | 26.72  | 20      | 0.55% |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| CC                  | 20.87 ±0.06           | 1.00×    | 47.92  | 20      | 0.43%         |
| khashl              | 21.05 ±0.24           | 1.01×    | 47.51  | 20      | 1.61%         |
| hwire_table (8N)    | 22.32 ±0.20           | 1.07×    | 44.80  | 20      | 1.24%         |
| hwire_table (4N)    | 22.58 ±0.24           | 1.08×    | 44.29  | 20      | 1.51%         |
| absl::flat_hash_map | 23.54 ±0.13           | 1.13×    | 42.48  | 20      | 0.78%         |
| hwire_table (2N)    | 26.33 ±1.13           | 1.26×    | 37.98  | 100     | 2.43% (unmet) |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 8.00 ±0.09            | 1.00×    | 125.00 | 20      | 1.60% |
| khashl              | 10.47 ±0.13           | 1.31×    | 95.51  | 20      | 1.77% |
| hwire_table (8N)    | 11.12 ±0.11           | 1.39×    | 89.93  | 20      | 1.33% |
| absl::flat_hash_map | 11.73 ±0.15           | 1.47×    | 85.25  | 20      | 1.74% |
| hwire_table (4N)    | 12.20 ±0.12           | 1.52×    | 81.97  | 20      | 1.43% |
| hwire_table (2N)    | 21.57 ±0.39           | 2.70×    | 46.36  | 30      | 1.97% |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**256 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (2N) †  | 3.616              | 1.00×    | 26.33 ±1.13               | Baseline                 |
| hwire_table (8N)    | 3.617              | 1.00×    | 22.32 ±0.20               | 2                        |
| hwire_table (4N)    | 3.624              | 1.00×    | 22.58 ±0.24               | 4                        |
| khashl              | 8.160              | 2.26×    | 21.05 ±0.24               | 862                      |
| CC                  | 9.375              | 2.59×    | 20.87 ±0.06               | 1,056                    |
| absl::flat_hash_map | 9.683              | 2.68×    | 23.54 ±0.13               | 2,176                    |


#### Build + Miss

**256 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (8N)    | 3.606               | 1.00×    | 11.12 ±0.11                | Baseline                 |
| hwire_table (2N)    | 3.611               | 1.00×    | 21.57 ±0.39                | No crossover             |
| hwire_table (4N)    | 3.614               | 1.00×    | 12.20 ±0.12                | No crossover             |
| khashl              | 8.149               | 2.26×    | 10.47 ±0.13                | 6,991                    |
| CC                  | 9.362               | 2.60×    | 8.00 ±0.09                 | 1,846                    |
| absl::flat_hash_map | 9.671               | 2.68×    | 11.73 ±0.15                | No crossover             |
