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
date                 : 2026-10-07T09:59:53+09:00
uname                : Linux 6.8.0-110-generic x86_64
os                   : Ubuntu 24.04.3 LTS
cpu                  : AMD Ryzen 7 PRO 4750GE with Radeon Graphics
clock                : 3.09 GHz
cores                : 1
memory               : 887 MiB
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
| khashl              | 351.15 ±3.15         | 1.00×    | 2.85   | 20      | 1.25% |
| hwire_table (8N)    | 397.81 ±3.85         | 1.13×    | 2.51   | 20      | 1.35% |
| hwire_table (4N)    | 400.40 ±4.47         | 1.14×    | 2.50   | 20      | 1.56% |
| hwire_table (2N)    | 409.19 ±3.96         | 1.17×    | 2.44   | 20      | 1.35% |
| CC                  | 417.46 ±2.99         | 1.19×    | 2.40   | 20      | 1.00% |
| absl::flat_hash_map | 591.10 ±4.31         | 1.68×    | 1.69   | 20      | 1.02% |

**64 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 649.66 ±2.20         | 1.00×    | 1.54   | 20      | 0.47% |
| hwire_table (4N)    | 806.13 ±10.72        | 1.24×    | 1.24   | 30      | 1.45% |
| hwire_table (8N)    | 812.57 ±3.30         | 1.25×    | 1.23   | 20      | 0.57% |
| hwire_table (2N)    | 824.55 ±6.96         | 1.27×    | 1.21   | 20      | 1.18% |
| CC                  | 828.93 ±3.17         | 1.28×    | 1.21   | 20      | 0.53% |
| absl::flat_hash_map | 1189.70 ±10.40       | 1.83×    | 0.84   | 20      | 1.22% |

**128 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 1278.32 ±10.10       | 1.00×    | 0.78   | 20      | 1.10% |
| CC                  | 1588.28 ±4.95        | 1.24×    | 0.63   | 20      | 0.44% |
| hwire_table (4N)    | 1599.16 ±5.77        | 1.25×    | 0.63   | 20      | 0.50% |
| hwire_table (8N)    | 1622.36 ±7.69        | 1.27×    | 0.62   | 20      | 0.66% |
| hwire_table (2N)    | 1623.38 ±38.89       | 1.27×    | 0.62   | 60      | 1.78% |
| absl::flat_hash_map | 2441.12 ±59.09       | 1.91×    | 0.41   | 60      | 1.80% |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**32 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 8.75 ±0.06         | 1.00×    | 114.29 | 20      | 0.92% |
| hwire_table (8N)    | 11.58 ±0.17        | 1.32×    | 86.36  | 30      | 1.60% |
| hwire_table (4N)    | 11.72 ±0.10        | 1.34×    | 85.32  | 20      | 1.18% |
| hwire_table (2N)    | 11.92 ±0.10        | 1.36×    | 83.89  | 20      | 1.16% |
| CC                  | 12.20 ±0.23        | 1.39×    | 81.97  | 40      | 1.76% |
| absl::flat_hash_map | 17.14 ±0.09        | 1.96×    | 58.34  | 20      | 0.71% |

**64 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 9.04 ±0.05         | 1.00×    | 110.62 | 20      | 0.70% |
| hwire_table (4N)    | 11.99 ±0.10        | 1.33×    | 83.40  | 20      | 1.17% |
| hwire_table (8N)    | 12.15 ±0.07        | 1.34×    | 82.30  | 20      | 0.81% |
| hwire_table (2N)    | 12.38 ±0.08        | 1.37×    | 80.78  | 20      | 0.92% |
| CC                  | 12.43 ±0.08        | 1.38×    | 80.45  | 20      | 0.88% |
| absl::flat_hash_map | 17.82 ±0.11        | 1.97×    | 56.12  | 20      | 0.87% |

**128 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 9.45 ±0.18         | 1.00×    | 105.82 | 40      | 1.75% |
| CC                  | 12.17 ±0.13        | 1.29×    | 82.17  | 20      | 1.50% |
| hwire_table (4N)    | 12.20 ±0.07        | 1.29×    | 81.97  | 20      | 0.77% |
| hwire_table (2N)    | 12.28 ±0.08        | 1.30×    | 81.43  | 20      | 0.86% |
| hwire_table (8N)    | 12.29 ±0.05        | 1.30×    | 81.37  | 20      | 0.61% |
| absl::flat_hash_map | 18.53 ±0.06        | 1.96×    | 53.97  | 20      | 0.44% |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| khashl              | 9.96 ±0.21            | 1.00×    | 100.40 | 50      | 1.76% |
| CC                  | 10.24 ±0.08           | 1.03×    | 97.66  | 20      | 1.10% |
| hwire_table (8N)    | 11.34 ±0.09           | 1.14×    | 88.18  | 20      | 1.05% |
| hwire_table (4N)    | 11.56 ±0.20           | 1.16×    | 86.51  | 40      | 1.61% |
| absl::flat_hash_map | 13.62 ±0.11           | 1.37×    | 73.42  | 20      | 1.11% |
| hwire_table (2N)    | 16.31 ±0.38           | 1.64×    | 61.31  | 60      | 1.72% |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| khashl              | 10.78 ±0.29           | 1.00×    | 92.76  | 60      | 1.97%         |
| CC                  | 11.24 ±0.23           | 1.04×    | 88.97  | 50      | 1.71%         |
| hwire_table (4N)    | 11.86 ±0.08           | 1.10×    | 84.32  | 20      | 0.94%         |
| hwire_table (8N)    | 12.04 ±0.32           | 1.12×    | 83.06  | 60      | 2.00%         |
| hwire_table (2N)    | 12.82 ±1.56           | 1.19×    | 78.00  | 100     | 6.90% (unmet) |
| absl::flat_hash_map | 14.63 ±0.11           | 1.36×    | 68.35  | 20      | 1.02%         |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| CC                  | 11.75 ±0.04           | 1.00×    | 85.11  | 20      | 0.42%         |
| khashl              | 12.30 ±0.04           | 1.05×    | 81.30  | 20      | 0.49%         |
| hwire_table (8N)    | 12.41 ±0.06           | 1.06×    | 80.58  | 20      | 0.62%         |
| hwire_table (2N)    | 12.54 ±0.37           | 1.07×    | 79.74  | 90      | 1.79%         |
| hwire_table (4N)    | 12.85 ±0.87           | 1.09×    | 77.82  | 100     | 3.83% (unmet) |
| absl::flat_hash_map | 15.71 ±0.08           | 1.34×    | 63.65  | 20      | 0.74%         |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.42 ±0.02            | 1.00×    | 134.77 | 20      | 0.42% |
| hwire_table (8N)    | 8.32 ±0.25            | 1.12×    | 120.19 | 90      | 1.81% |
| khashl              | 8.42 ±0.03            | 1.13×    | 118.76 | 20      | 0.57% |
| hwire_table (4N)    | 8.56 ±0.04            | 1.15×    | 116.82 | 20      | 0.70% |
| absl::flat_hash_map | 8.70 ±0.03            | 1.17×    | 114.94 | 20      | 0.54% |
| hwire_table (2N)    | 9.86 ±0.11            | 1.33×    | 101.42 | 20      | 1.53% |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| CC                  | 7.36 ±0.13            | 1.00×    | 135.87 | 40      | 1.64%         |
| khashl              | 8.85 ±0.11            | 1.20×    | 112.99 | 20      | 1.69%         |
| hwire_table (8N)    | 9.16 ±0.05            | 1.24×    | 109.17 | 20      | 0.74%         |
| hwire_table (4N)    | 9.20 ±0.45            | 1.25×    | 108.70 | 100     | 2.77% (unmet) |
| absl::flat_hash_map | 9.68 ±0.13            | 1.32×    | 103.31 | 20      | 1.93%         |
| hwire_table (2N)    | 9.68 ±0.04            | 1.32×    | 103.31 | 20      | 0.56%         |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.52 ±0.05            | 1.00×    | 132.98 | 20      | 0.95% |
| khashl              | 9.19 ±0.10            | 1.22×    | 108.81 | 20      | 1.47% |
| hwire_table (4N)    | 9.31 ±0.07            | 1.24×    | 107.41 | 20      | 1.10% |
| hwire_table (8N)    | 9.46 ±0.21            | 1.26×    | 105.71 | 50      | 1.83% |
| absl::flat_hash_map | 9.96 ±0.11            | 1.32×    | 100.40 | 20      | 1.59% |
| hwire_table (2N)    | 9.98 ±0.03            | 1.33×    | 100.20 | 20      | 0.39% |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**32 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 0.361              | 1.00×    | 9.96 ±0.21                | Baseline                 |
| hwire_table (8N)    | 0.409              | 1.13×    | 11.34 ±0.09               | No crossover             |
| hwire_table (4N)    | 0.412              | 1.14×    | 11.56 ±0.20               | No crossover             |
| hwire_table (2N)    | 0.425              | 1.18×    | 16.31 ±0.38               | No crossover             |
| CC                  | 0.428              | 1.18×    | 10.24 ±0.08               | No crossover             |
| absl::flat_hash_map | 0.605              | 1.67×    | 13.62 ±0.11               | No crossover             |

**64 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 0.660              | 1.00×    | 10.78 ±0.29               | Baseline                 |
| hwire_table (4N)    | 0.818              | 1.24×    | 11.86 ±0.08               | No crossover             |
| hwire_table (8N)    | 0.825              | 1.25×    | 12.04 ±0.32               | No crossover             |
| hwire_table (2N) †  | 0.837              | 1.27×    | 12.82 ±1.56               | No crossover             |
| CC                  | 0.840              | 1.27×    | 11.24 ±0.23               | No crossover             |
| absl::flat_hash_map | 1.204              | 1.82×    | 14.63 ±0.11               | No crossover             |

**128 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 1.291              | 1.00×    | 12.30 ±0.04               | Baseline                 |
| CC                  | 1.600              | 1.24×    | 11.75 ±0.04               | 564                      |
| hwire_table (4N) †  | 1.612              | 1.25×    | 12.85 ±0.87               | No crossover             |
| hwire_table (8N)    | 1.635              | 1.27×    | 12.41 ±0.06               | No crossover             |
| hwire_table (2N)    | 1.636              | 1.27×    | 12.54 ±0.37               | No crossover             |
| absl::flat_hash_map | 2.457              | 1.90×    | 15.71 ±0.08               | No crossover             |


#### Build + Miss

**32 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.360               | 1.00×    | 8.42 ±0.03                 | Baseline                 |
| hwire_table (8N)    | 0.406               | 1.13×    | 8.32 ±0.25                 | 467                      |
| hwire_table (4N)    | 0.409               | 1.14×    | 8.56 ±0.04                 | No crossover             |
| hwire_table (2N)    | 0.419               | 1.17×    | 9.86 ±0.11                 | No crossover             |
| CC                  | 0.425               | 1.18×    | 7.42 ±0.02                 | 67                       |
| absl::flat_hash_map | 0.600               | 1.67×    | 8.70 ±0.03                 | No crossover             |

**64 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.659               | 1.00×    | 8.85 ±0.11                 | Baseline                 |
| hwire_table (4N) †  | 0.815               | 1.24×    | 9.20 ±0.45                 | No crossover             |
| hwire_table (8N)    | 0.822               | 1.25×    | 9.16 ±0.05                 | No crossover             |
| hwire_table (2N)    | 0.834               | 1.27×    | 9.68 ±0.04                 | No crossover             |
| CC                  | 0.836               | 1.27×    | 7.36 ±0.13                 | 121                      |
| absl::flat_hash_map | 1.199               | 1.82×    | 9.68 ±0.13                 | No crossover             |

**128 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 1.288               | 1.00×    | 9.19 ±0.10                 | Baseline                 |
| CC                  | 1.596               | 1.24×    | 7.52 ±0.05                 | 186                      |
| hwire_table (4N)    | 1.608               | 1.25×    | 9.31 ±0.07                 | No crossover             |
| hwire_table (8N)    | 1.632               | 1.27×    | 9.46 ±0.21                 | No crossover             |
| hwire_table (2N)    | 1.633               | 1.27×    | 9.98 ±0.03                 | No crossover             |
| absl::flat_hash_map | 2.451               | 1.90×    | 9.96 ±0.11                 | No crossover             |


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

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 403.77 ±1.56         | 1.00×    | 2.48   | 20      | 0.54% |
| hwire_table (8N)    | 434.14 ±1.96         | 1.08×    | 2.30   | 20      | 0.63% |
| hwire_table (4N)    | 437.24 ±2.41         | 1.08×    | 2.29   | 20      | 0.77% |
| hwire_table (2N)    | 450.10 ±2.23         | 1.11×    | 2.22   | 20      | 0.69% |
| CC                  | 477.56 ±1.68         | 1.18×    | 2.09   | 20      | 0.49% |
| absl::flat_hash_map | 644.97 ±2.52         | 1.60×    | 1.55   | 20      | 0.55% |

**64 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 741.66 ±2.95         | 1.00×    | 1.35   | 20      | 0.56% |
| hwire_table (4N)    | 875.68 ±4.56         | 1.18×    | 1.14   | 20      | 0.73% |
| hwire_table (2N)    | 880.25 ±14.96        | 1.19×    | 1.14   | 30      | 1.86% |
| hwire_table (8N)    | 885.21 ±4.71         | 1.19×    | 1.13   | 20      | 0.74% |
| CC                  | 897.34 ±12.58        | 1.21×    | 1.11   | 20      | 1.96% |
| absl::flat_hash_map | 1291.88 ±6.77        | 1.74×    | 0.77   | 20      | 0.73% |

**128 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | -------------------- | -------- | ------ | ------- | ------------- |
| khashl              | 1447.90 ±5.78        | 1.00×    | 0.69   | 20      | 0.56%         |
| CC                  | 1718.79 ±90.66       | 1.19×    | 0.58   | 100     | 2.99% (unmet) |
| hwire_table (8N)    | 1764.77 ±6.10        | 1.22×    | 0.57   | 20      | 0.48%         |
| hwire_table (2N)    | 1776.32 ±14.14       | 1.23×    | 0.56   | 20      | 1.11%         |
| hwire_table (4N)    | 1776.77 ±8.13        | 1.23×    | 0.56   | 20      | 0.64%         |
| absl::flat_hash_map | 2621.18 ±7.75        | 1.81×    | 0.38   | 20      | 0.41%         |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**32 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 10.40 ±0.05        | 1.00×    | 96.15  | 20      | 0.70% |
| hwire_table (8N)    | 12.79 ±0.06        | 1.23×    | 78.19  | 20      | 0.64% |
| hwire_table (4N)    | 12.96 ±0.17        | 1.25×    | 77.16  | 20      | 1.84% |
| hwire_table (2N)    | 13.30 ±0.06        | 1.28×    | 75.19  | 20      | 0.67% |
| CC                  | 14.37 ±0.33        | 1.38×    | 69.59  | 50      | 1.86% |
| absl::flat_hash_map | 18.74 ±0.09        | 1.80×    | 53.36  | 20      | 0.70% |

**64 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 10.49 ±0.04        | 1.00×    | 95.33  | 20      | 0.54% |
| hwire_table (8N)    | 13.26 ±0.05        | 1.26×    | 75.41  | 20      | 0.55% |
| hwire_table (4N)    | 13.31 ±0.17        | 1.27×    | 75.13  | 20      | 1.74% |
| hwire_table (2N)    | 13.35 ±0.06        | 1.27×    | 74.91  | 20      | 0.60% |
| CC                  | 13.57 ±0.08        | 1.29×    | 73.69  | 20      | 0.81% |
| absl::flat_hash_map | 19.47 ±0.15        | 1.86×    | 51.36  | 20      | 1.05% |

**128 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 10.77 ±0.04        | 1.00×    | 92.85  | 20      | 0.50% |
| CC                  | 13.03 ±0.05        | 1.21×    | 76.75  | 20      | 0.56% |
| hwire_table (8N)    | 13.49 ±0.05        | 1.25×    | 74.13  | 20      | 0.54% |
| hwire_table (4N)    | 13.63 ±0.05        | 1.27×    | 73.37  | 20      | 0.51% |
| hwire_table (2N)    | 13.73 ±0.27        | 1.27×    | 72.83  | 50      | 1.60% |
| absl::flat_hash_map | 20.09 ±0.07        | 1.87×    | 49.78  | 20      | 0.52% |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| khashl              | 17.79 ±0.10           | 1.00×    | 56.21  | 20      | 0.77% |
| CC                  | 18.18 ±0.07           | 1.02×    | 55.01  | 20      | 0.51% |
| hwire_table (8N)    | 18.38 ±0.15           | 1.03×    | 54.41  | 20      | 1.12% |
| hwire_table (4N)    | 18.47 ±0.12           | 1.04×    | 54.14  | 20      | 0.93% |
| hwire_table (2N)    | 18.82 ±0.07           | 1.06×    | 53.13  | 20      | 0.49% |
| absl::flat_hash_map | 21.25 ±0.07           | 1.19×    | 47.06  | 20      | 0.47% |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| khashl              | 17.71 ±0.10           | 1.00×    | 56.47  | 20      | 0.76% |
| CC                  | 17.85 ±0.19           | 1.01×    | 56.02  | 20      | 1.49% |
| hwire_table (8N)    | 18.13 ±0.09           | 1.02×    | 55.16  | 20      | 0.66% |
| hwire_table (4N)    | 18.28 ±0.12           | 1.03×    | 54.70  | 20      | 0.92% |
| hwire_table (2N)    | 18.42 ±0.07           | 1.04×    | 54.29  | 20      | 0.55% |
| absl::flat_hash_map | 21.17 ±0.10           | 1.20×    | 47.24  | 20      | 0.69% |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 18.95 ±0.20           | 1.00×    | 52.77  | 20      | 1.46% |
| khashl              | 19.02 ±0.12           | 1.00×    | 52.58  | 20      | 0.89% |
| hwire_table (4N)    | 19.43 ±0.04           | 1.03×    | 51.47  | 20      | 0.32% |
| hwire_table (8N)    | 19.55 ±0.08           | 1.03×    | 51.15  | 20      | 0.54% |
| hwire_table (2N)    | 19.63 ±0.09           | 1.04×    | 50.94  | 20      | 0.61% |
| absl::flat_hash_map | 22.54 ±0.67           | 1.19×    | 44.37  | 90      | 1.79% |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.75 ±0.04            | 1.00×    | 129.03 | 20      | 0.64% |
| khashl              | 8.35 ±0.03            | 1.08×    | 119.76 | 20      | 0.53% |
| hwire_table (8N)    | 9.30 ±0.10            | 1.20×    | 107.53 | 20      | 1.47% |
| hwire_table (4N)    | 9.43 ±0.06            | 1.22×    | 106.04 | 20      | 0.89% |
| absl::flat_hash_map | 10.43 ±0.18           | 1.35×    | 95.88  | 30      | 1.91% |
| hwire_table (2N)    | 12.67 ±0.05           | 1.63×    | 78.93  | 20      | 0.53% |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.59 ±0.02            | 1.00×    | 131.75 | 20      | 0.30% |
| hwire_table (4N)    | 9.40 ±0.08            | 1.24×    | 106.38 | 20      | 1.22% |
| hwire_table (8N)    | 9.47 ±0.13            | 1.25×    | 105.60 | 20      | 1.85% |
| khashl              | 9.87 ±0.19            | 1.30×    | 101.32 | 50      | 1.62% |
| absl::flat_hash_map | 10.64 ±0.15           | 1.40×    | 93.98  | 20      | 2.00% |
| hwire_table (2N)    | 11.18 ±0.21           | 1.47×    | 89.45  | 40      | 1.71% |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.77 ±0.05            | 1.00×    | 128.70 | 20      | 0.85% |
| khashl              | 9.31 ±0.05            | 1.20×    | 107.41 | 20      | 0.81% |
| hwire_table (4N)    | 9.77 ±0.32            | 1.26×    | 102.35 | 100     | 1.84% |
| hwire_table (8N)    | 9.78 ±0.09            | 1.26×    | 102.25 | 20      | 1.31% |
| hwire_table (2N)    | 10.57 ±0.05           | 1.36×    | 94.61  | 20      | 0.66% |
| absl::flat_hash_map | 10.81 ±0.07           | 1.39×    | 92.51  | 20      | 0.94% |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**32 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 0.422              | 1.00×    | 17.79 ±0.10               | Baseline                 |
| hwire_table (8N)    | 0.453              | 1.07×    | 18.38 ±0.15               | No crossover             |
| hwire_table (4N)    | 0.456              | 1.08×    | 18.47 ±0.12               | No crossover             |
| hwire_table (2N)    | 0.469              | 1.11×    | 18.82 ±0.07               | No crossover             |
| CC                  | 0.496              | 1.18×    | 18.18 ±0.07               | No crossover             |
| absl::flat_hash_map | 0.666              | 1.58×    | 21.25 ±0.07               | No crossover             |

**64 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 0.759              | 1.00×    | 17.71 ±0.10               | Baseline                 |
| hwire_table (4N)    | 0.894              | 1.18×    | 18.28 ±0.12               | No crossover             |
| hwire_table (2N)    | 0.899              | 1.18×    | 18.42 ±0.07               | No crossover             |
| hwire_table (8N)    | 0.903              | 1.19×    | 18.13 ±0.09               | No crossover             |
| CC                  | 0.915              | 1.21×    | 17.85 ±0.19               | No crossover             |
| absl::flat_hash_map | 1.313              | 1.73×    | 21.17 ±0.10               | No crossover             |

**128 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 1.467              | 1.00×    | 19.02 ±0.12               | Baseline                 |
| CC †                | 1.738              | 1.18×    | 18.95 ±0.20               | 3,870                    |
| hwire_table (8N)    | 1.784              | 1.22×    | 19.55 ±0.08               | No crossover             |
| hwire_table (2N)    | 1.796              | 1.22×    | 19.63 ±0.09               | No crossover             |
| hwire_table (4N)    | 1.796              | 1.22×    | 19.43 ±0.04               | No crossover             |
| absl::flat_hash_map | 2.644              | 1.80×    | 22.54 ±0.67               | No crossover             |


#### Build + Miss

**32 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.412               | 1.00×    | 8.35 ±0.03                 | Baseline                 |
| hwire_table (8N)    | 0.443               | 1.08×    | 9.30 ±0.10                 | No crossover             |
| hwire_table (4N)    | 0.447               | 1.08×    | 9.43 ±0.06                 | No crossover             |
| hwire_table (2N)    | 0.463               | 1.12×    | 12.67 ±0.05                | No crossover             |
| CC                  | 0.485               | 1.18×    | 7.75 ±0.04                 | 123                      |
| absl::flat_hash_map | 0.655               | 1.59×    | 10.43 ±0.18                | No crossover             |

**64 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.752               | 1.00×    | 9.87 ±0.19                 | Baseline                 |
| hwire_table (4N)    | 0.885               | 1.18×    | 9.40 ±0.08                 | 286                      |
| hwire_table (2N)    | 0.891               | 1.19×    | 11.18 ±0.21                | No crossover             |
| hwire_table (8N)    | 0.895               | 1.19×    | 9.47 ±0.13                 | 359                      |
| CC                  | 0.905               | 1.20×    | 7.59 ±0.02                 | 69                       |
| absl::flat_hash_map | 1.303               | 1.73×    | 10.64 ±0.15                | No crossover             |

**128 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 1.457               | 1.00×    | 9.31 ±0.05                 | Baseline                 |
| CC †                | 1.727               | 1.18×    | 7.77 ±0.05                 | 176                      |
| hwire_table (8N)    | 1.775               | 1.22×    | 9.78 ±0.09                 | No crossover             |
| hwire_table (4N)    | 1.787               | 1.23×    | 9.77 ±0.32                 | No crossover             |
| hwire_table (2N)    | 1.787               | 1.23×    | 10.57 ±0.05                | No crossover             |
| absl::flat_hash_map | 2.632               | 1.81×    | 10.81 ±0.07                | No crossover             |


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
| hwire_table (8N)    | 3574.41 ±26.20       | 1.00×    | 0.28   | 20      | 1.02% |
| hwire_table (4N)    | 3577.88 ±27.40       | 1.00×    | 0.28   | 20      | 1.07% |
| hwire_table (2N)    | 3593.12 ±16.73       | 1.01×    | 0.28   | 20      | 0.65% |
| khashl              | 7151.93 ±36.57       | 2.00×    | 0.14   | 20      | 0.71% |
| CC                  | 9280.34 ±45.22       | 2.60×    | 0.11   | 20      | 0.68% |
| absl::flat_hash_map | 9722.75 ±97.62       | 2.72×    | 0.10   | 20      | 1.40% |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**256 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | ------------------ | -------- | ------ | ------- | ------------- |
| hwire_table (2N)    | 13.82 ±0.11        | 1.00×    | 72.36  | 20      | 1.10%         |
| hwire_table (8N)    | 13.87 ±0.04        | 1.00×    | 72.10  | 20      | 0.40%         |
| hwire_table (4N)    | 13.88 ±0.06        | 1.00×    | 72.05  | 20      | 0.58%         |
| khashl              | 27.13 ±1.04        | 1.96×    | 36.86  | 100     | 2.17% (unmet) |
| CC                  | 36.10 ±0.65        | 2.61×    | 27.70  | 40      | 1.68%         |
| absl::flat_hash_map | 37.67 ±0.09        | 2.73×    | 26.55  | 20      | 0.34%         |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| khashl              | 13.26 ±0.09           | 1.00×    | 75.41  | 20      | 0.98%         |
| CC                  | 13.34 ±0.11           | 1.01×    | 74.96  | 20      | 1.18%         |
| absl::flat_hash_map | 16.93 ±0.06           | 1.28×    | 59.07  | 20      | 0.53%         |
| hwire_table (8N)    | 26.04 ±0.14           | 1.96×    | 38.40  | 20      | 0.76%         |
| hwire_table (4N)    | 28.66 ±1.07           | 2.16×    | 34.89  | 100     | 2.11% (unmet) |
| hwire_table (2N)    | 30.79 ±0.33           | 2.32×    | 32.48  | 20      | 1.52%         |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 8.14 ±0.03            | 1.00×    | 122.85 | 20      | 0.55% |
| khashl              | 8.73 ±0.06            | 1.07×    | 114.55 | 20      | 0.97% |
| absl::flat_hash_map | 11.83 ±0.07           | 1.45×    | 84.53  | 20      | 0.83% |
| hwire_table (8N)    | 21.13 ±0.08           | 2.60×    | 47.33  | 20      | 0.52% |
| hwire_table (4N)    | 21.58 ±0.09           | 2.65×    | 46.34  | 20      | 0.57% |
| hwire_table (2N)    | 27.91 ±0.17           | 3.43×    | 35.83  | 20      | 0.85% |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**256 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (8N)    | 3.600              | 1.00×    | 26.04 ±0.14               | Baseline                 |
| hwire_table (4N) †  | 3.607              | 1.00×    | 28.66 ±1.07               | No crossover             |
| hwire_table (2N)    | 3.624              | 1.01×    | 30.79 ±0.33               | No crossover             |
| khashl              | 7.165              | 1.99×    | 13.26 ±0.09               | 280                      |
| CC                  | 9.294              | 2.58×    | 13.34 ±0.11               | 450                      |
| absl::flat_hash_map | 9.740              | 2.71×    | 16.93 ±0.06               | 675                      |


#### Build + Miss

**256 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (8N)    | 3.596               | 1.00×    | 21.13 ±0.08                | Baseline                 |
| hwire_table (4N)    | 3.599               | 1.00×    | 21.58 ±0.09                | No crossover             |
| hwire_table (2N)    | 3.621               | 1.01×    | 27.91 ±0.17                | No crossover             |
| khashl              | 7.161               | 1.99×    | 8.73 ±0.06                 | 289                      |
| CC                  | 9.288               | 2.58×    | 8.14 ±0.03                 | 440                      |
| absl::flat_hash_map | 9.735               | 2.71×    | 11.83 ±0.07                | 662                      |


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
| hwire_table (4N)    | 3784.77 ±30.12       | 1.00×    | 0.26   | 20      | 1.11% |
| hwire_table (2N)    | 3822.27 ±14.18       | 1.01×    | 0.26   | 20      | 0.52% |
| hwire_table (8N)    | 3840.36 ±68.38       | 1.01×    | 0.26   | 30      | 1.95% |
| khashl              | 8139.15 ±57.16       | 2.15×    | 0.12   | 20      | 0.98% |
| CC                  | 9293.50 ±46.97       | 2.46×    | 0.11   | 20      | 0.71% |
| absl::flat_hash_map | 9728.60 ±20.16       | 2.57×    | 0.10   | 20      | 0.29% |


### Insert

Insertions only; initial setup is excluded and insertion-triggered expansion is included.

**256 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 14.69 ±0.05        | 1.00×    | 68.07  | 20      | 0.51% |
| hwire_table (2N)    | 14.77 ±0.04        | 1.01×    | 67.70  | 20      | 0.38% |
| hwire_table (8N)    | 14.80 ±0.05        | 1.01×    | 67.57  | 20      | 0.45% |
| khashl              | 31.29 ±0.63        | 2.13×    | 31.96  | 50      | 1.66% |
| CC                  | 35.91 ±0.15        | 2.44×    | 27.85  | 20      | 0.58% |
| absl::flat_hash_map | 37.85 ±0.11        | 2.58×    | 26.42  | 20      | 0.42% |


### Hit

Successful searches in the fully populated map, with equal frequency for each selected key.

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 20.70 ±0.06           | 1.00×    | 48.31  | 20      | 0.39% |
| khashl              | 21.05 ±0.09           | 1.02×    | 47.51  | 20      | 0.57% |
| absl::flat_hash_map | 23.69 ±0.22           | 1.14×    | 42.21  | 20      | 1.29% |
| hwire_table (8N)    | 25.27 ±0.22           | 1.22×    | 39.57  | 20      | 1.20% |
| hwire_table (4N)    | 25.68 ±0.49           | 1.24×    | 38.94  | 40      | 1.77% |
| hwire_table (2N)    | 29.72 ±0.92           | 1.44×    | 33.65  | 90      | 1.86% |


### Miss

Unsuccessful searches in the fully populated map using the prepared miss keys.

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.96 ±0.02            | 1.00×    | 125.63 | 20      | 0.40% |
| khashl              | 10.36 ±0.25           | 1.30×    | 96.53  | 60      | 1.83% |
| absl::flat_hash_map | 11.12 ±0.26           | 1.40×    | 89.93  | 50      | 1.88% |
| hwire_table (8N)    | 20.57 ±0.58           | 2.58×    | 48.61  | 80      | 1.80% |
| hwire_table (4N)    | 21.42 ±0.13           | 2.69×    | 46.69  | 20      | 0.83% |
| hwire_table (2N)    | 28.25 ±0.77           | 3.55×    | 35.40  | 70      | 1.87% |


### First Lookup Cost and Break-even

Estimate the total time to build and populate a map and perform its first key lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher construction cost.

Totals use the displayed means: `Build + Q × lookup mean`. Hit and Miss are shown separately; for hit fraction p, the combined estimate is `Build + Q × (p × hit + (1 − p) × miss)`. Lookup costs are measured on a fully populated warm map; the first-lookup total is estimated, not timed immediately after construction.

Each table uses the fastest Build + 1 lookup as its baseline. The crossover is the first integer Q that beats that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**256 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (4N)    | 3.810              | 1.00×    | 25.68 ±0.49               | Baseline                 |
| hwire_table (2N)    | 3.852              | 1.01×    | 29.72 ±0.92               | No crossover             |
| hwire_table (8N)    | 3.866              | 1.01×    | 25.27 ±0.22               | 136                      |
| khashl              | 8.160              | 2.14×    | 21.05 ±0.09               | 941                      |
| CC                  | 9.314              | 2.44×    | 20.70 ±0.06               | 1,107                    |
| absl::flat_hash_map | 9.752              | 2.56×    | 23.69 ±0.22               | 2,987                    |


#### Build + Miss

**256 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (4N)    | 3.806               | 1.00×    | 21.42 ±0.13                | Baseline                 |
| hwire_table (2N)    | 3.851               | 1.01×    | 28.25 ±0.77                | No crossover             |
| hwire_table (8N)    | 3.861               | 1.01×    | 20.57 ±0.58                | 66                       |
| khashl              | 8.150               | 2.14×    | 10.36 ±0.25                | 394                      |
| CC                  | 9.301               | 2.44×    | 7.96 ±0.02                 | 410                      |
| absl::flat_hash_map | 9.740               | 2.56×    | 11.12 ±0.26                | 578                      |
