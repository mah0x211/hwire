# Benchmarking Hashmaps

Compare `hwire_table`, CC, `absl::flat_hash_map`, and khashl as storage for
HTTP header and query key/value slices. Case-sensitive and ASCII
case-insensitive maps are measured separately. HTTP parsing is excluded.

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

For `Q` lookups and hit fraction `p`, compare total map work as
`build + Q * (p * hit + (1-p) * miss)`. If construction is slower but lookup is
faster, the break-even count is the extra build time divided by the per-lookup
saving, rounded up. Means give an estimate; small differences and unmet RCIW
targets limit its precision.

Memory excludes borrowed key/value contents and system allocator metadata.

## Configuration

All maps use the same seeded hwire hash backend. Native x86 builds enable
AES via `-march=native`; `make VARIANTS=siphash` selects SipHash-1-3 with
`-DHWIRE_NO_AES`. khashl consumes the low 32 bits according to its native
32-bit hash interface. External maps call a shared hash bridge; hwire may
inline its hash. This difference is included in timings.

CI comparisons in all adapters fold and compare eight bytes at a time,
with bytewise handling of the remainder. CC retains its comparison ordering
when keys differ.

Release builds use `-O2 -DNDEBUG`. The platform file records the compiler,
flags and hash backend. Published measurements use the x86 reference system.

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

hwire is measured with 2N, 4N and 8N slots per segment. All three use the
same implementation and segment capacities; only index slot capacity differs.
At full pair capacity their slot occupancies are 50%, 25% and 12.5%. Linked
segments report the aggregate load: total stored keys / total hash slots.
Adapters compute load from the populated containers; the driver does not
substitute the input key count. Growth segments are 32+32+64+128: initial
storage plus three extensions. CC, Abseil and khashl reserve enough space for
32 keys, then grow naturally during insertion. Extension counts are reported
from native capacity transitions. Equal extension counts do not imply equal
allocation call counts, resize points, memory use, or rehash work.

hwire can fill every entry in its configured pair arrays while retaining spare
hash slots for probing. All workloads here use 100% of its entry capacity,
including every segment of the 32+32+64+128 growth chain. Entry-capacity usage
and slot load are separate: full entry arrays correspond to 50%, 25% and
12.5% slot load for 2N, 4N and 8N respectively.

CC, Abseil and khashl store entries within their hash slots. The tables compare
slot load and allocated bytes; entry-capacity usage is described here as a
property of hwire's separate pair arrays.

CC and khashl use a key descriptor plus a copied KV value; Abseil stores the
same KV value with its key. hwire stores each KV once in its pair array.
These native representation costs are included in the memory measurements.

Adapter code contains storage operations only, with no
timers, samples or result validation.

Sampling starts with 20 samples and checks every 10 up to 100. Target RCIW
is 2% (full interval width / mean), using Student-t intervals with Bonferroni
correction over nine stopping points. Iterations are calibrated to about
1 ms measured work per sample, up to one million key operations. Progress and
achieved precision appear on stderr; unmet targets are marked in the report.
The mean cost of an empty timer pair is calibrated before measurement and
subtracted from each measured interval. Build/insert time a complete population; hit/miss time repeated complete
traversals, amortizing timer calls. No fixed lookup-count scenario is added.

## Commands

| Command | Action |
|---|---|
| `make` / `make run` | Build and measure all four scenarios |
| `make setup` | Fetch the pinned dependencies of registered adapters |
| `make build` | Fetch dependencies and build the timing binaries |
| `make growth` | Measure only the expansion scenario |
| `make report` | Render saved CSVs as Markdown tables |
| `python3 scripts/report_hashmaps.py --write-readme` | Replace the Benchmark section with saved results |
| `make adapters` | Build adapter archives for the parser suite |
| `make check` | Build and check only the statistical helper |
| `make list` | List registered maps |
| `make clean` | Remove build artifacts, retain results |

The first setup requires network access, `curl` and `tar`. Subsequent setup
runs reuse the fetched revisions. Builds also require Make, Python 3, a C99
compiler and a C++17 compiler. Dependency acquisition completes before timing.

Results are saved locally in `results/storage/<map>-<variant>.csv` and excluded
from version control. The generated tables are published in this README.
`--quick` runs a short development measurement, not publication sampling.
`--map ID` selects one registered adapter. `--metadata` outputs actual load factors
without timing or replacing saved results. Directory prefixes `_` disable
adapters and their configurations, including dependencies, without changing
the driver.

## Registration

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
  hash. Initialize the shared hash seed to 42 in external-map constructors;
  hwire constructors initialize their own table key with the same seed.
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
 * otherwise be ambiguous, for example "hwire_table (4N)". Use a name without
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
 * the initial reservation. Pushes use native automatic growth; hwire links
 * additional segments of 32, 64 and 128 entries. Do not reserve the full
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
 * transitions or appended hwire segments. Doubling implementations may derive
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
lookup, insertion and cleanup functions. hwire uses `4n` and `8n`; the base
configuration is 2N. No implementation names are embedded in registration.

# Benchmark

## Implementations

| Map | Source |
|---|---|
| hwire_table | [hwire/hashmap.c](hwire/hashmap.c), current library |
| CC | [cc/hashmap.cpp](cc/hashmap.cpp), CC v1.4.3 (fixed revision in `cc/fetch.sh`) |
| absl::flat_hash_map | [abseil/hashmap.cpp](abseil/hashmap.cpp), Abseil 20260817.0 |
| khashl | [khashl/hashmap.c](khashl/hashmap.c), revision pinned in [UPSTREAM.md](khashl/UPSTREAM.md) |

## Measurements

### Environment and build

| Setting | Value |
|---|---|
| Measurement date | 2026-10-06 |
| CPU | AMD Ryzen 7 PRO 4750GE |
| OS | Ubuntu 24.04.3 LTS |
| Virtualization | KVM, one virtual CPU |
| Compilers | GCC / G++ 13.3 |
| C flags | `-O2 -DNDEBUG -std=c99 -march=native` |
| C++ flags | `-O2 -DNDEBUG -std=c++17 -march=native` |
| Hash backend | Native AES |

### Sampling and precision

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

Insert excludes initial setup and includes any expansion triggered by insertion. Hit and miss measure the fully populated maps.


### Memory

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

**32 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 368.36 ±3.57         | 1.00×    | 2.71   | 20      | 1.35% |
| hwire_table (8N)    | 397.08 ±3.42         | 1.08×    | 2.52   | 20      | 1.21% |
| hwire_table (4N)    | 399.62 ±3.40         | 1.08×    | 2.50   | 20      | 1.19% |
| hwire_table (2N)    | 409.19 ±2.99         | 1.11×    | 2.44   | 20      | 1.02% |
| CC                  | 412.59 ±2.28         | 1.12×    | 2.42   | 20      | 0.77% |
| absl::flat_hash_map | 600.38 ±3.45         | 1.63×    | 1.67   | 20      | 0.80% |

**64 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 669.91 ±4.79         | 1.00×    | 1.49   | 20      | 1.00% |
| hwire_table (4N)    | 807.91 ±4.47         | 1.21×    | 1.24   | 20      | 0.77% |
| hwire_table (8N)    | 814.44 ±3.74         | 1.22×    | 1.23   | 20      | 0.64% |
| hwire_table (2N)    | 814.90 ±10.50        | 1.22×    | 1.23   | 20      | 1.80% |
| CC                  | 835.52 ±4.34         | 1.25×    | 1.20   | 20      | 0.73% |
| absl::flat_hash_map | 1192.88 ±6.19        | 1.78×    | 0.84   | 20      | 0.73% |

**128 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 1293.90 ±4.38        | 1.00×    | 0.77   | 20      | 0.47% |
| CC                  | 1598.19 ±22.20       | 1.24×    | 0.63   | 30      | 1.52% |
| hwire_table (4N)    | 1616.14 ±5.49        | 1.25×    | 0.62   | 20      | 0.48% |
| hwire_table (2N)    | 1625.63 ±8.09        | 1.26×    | 0.62   | 20      | 0.70% |
| hwire_table (8N)    | 1633.45 ±5.21        | 1.26×    | 0.61   | 20      | 0.45% |
| absl::flat_hash_map | 2440.47 ±9.09        | 1.89×    | 0.41   | 20      | 0.52% |


### Insert

**32 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 9.44 ±0.15         | 1.00×    | 105.88 | 30      | 1.70% |
| hwire_table (8N)    | 11.56 ±0.06        | 1.22×    | 86.51  | 20      | 0.77% |
| hwire_table (4N)    | 11.75 ±0.05        | 1.24×    | 85.07  | 20      | 0.63% |
| hwire_table (2N)    | 11.94 ±0.06        | 1.26×    | 83.77  | 20      | 0.69% |
| CC                  | 12.09 ±0.11        | 1.28×    | 82.73  | 20      | 1.31% |
| absl::flat_hash_map | 17.52 ±0.36        | 1.86×    | 57.08  | 40      | 1.89% |

**64 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 9.37 ±0.05         | 1.00×    | 106.69 | 20      | 0.82% |
| hwire_table (8N)    | 12.16 ±0.09        | 1.30×    | 82.21  | 20      | 1.02% |
| hwire_table (4N)    | 12.16 ±0.05        | 1.30×    | 82.20  | 20      | 0.52% |
| hwire_table (2N)    | 12.32 ±0.05        | 1.31×    | 81.19  | 20      | 0.57% |
| CC                  | 12.56 ±0.06        | 1.34×    | 79.63  | 20      | 0.66% |
| absl::flat_hash_map | 17.93 ±0.09        | 1.91×    | 55.77  | 20      | 0.71% |

**128 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | ------------------ | -------- | ------ | ------- | ------------- |
| khashl              | 10.12 ±1.13        | 1.00×    | 98.79  | 100     | 6.35% (unmet) |
| CC                  | 12.23 ±0.06        | 1.21×    | 81.75  | 20      | 0.63%         |
| hwire_table (2N)    | 12.39 ±0.07        | 1.22×    | 80.71  | 20      | 0.82%         |
| hwire_table (4N)    | 12.42 ±0.07        | 1.23×    | 80.52  | 20      | 0.76%         |
| hwire_table (8N)    | 12.50 ±0.05        | 1.23×    | 80.00  | 20      | 0.54%         |
| absl::flat_hash_map | 18.74 ±0.06        | 1.85×    | 53.37  | 20      | 0.44%         |


### Hit

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| khashl              | 10.04 ±0.06           | 1.00×    | 99.59  | 20      | 0.90% |
| CC                  | 10.32 ±0.03           | 1.03×    | 96.91  | 20      | 0.47% |
| hwire_table (8N)    | 11.44 ±0.16           | 1.14×    | 87.44  | 30      | 1.50% |
| hwire_table (4N)    | 11.55 ±0.08           | 1.15×    | 86.58  | 20      | 1.00% |
| absl::flat_hash_map | 13.58 ±0.07           | 1.35×    | 73.64  | 20      | 0.71% |
| hwire_table (2N)    | 16.36 ±0.22           | 1.63×    | 61.11  | 20      | 1.85% |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 11.29 ±0.14           | 1.00×    | 88.56  | 20      | 1.71% |
| hwire_table (4N)    | 12.24 ±0.11           | 1.08×    | 81.71  | 20      | 1.25% |
| hwire_table (8N)    | 12.39 ±0.06           | 1.10×    | 80.72  | 20      | 0.67% |
| hwire_table (2N)    | 12.60 ±0.07           | 1.12×    | 79.38  | 20      | 0.75% |
| khashl              | 14.15 ±0.03           | 1.25×    | 70.66  | 20      | 0.34% |
| absl::flat_hash_map | 14.49 ±0.10           | 1.28×    | 69.02  | 20      | 0.99% |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| CC                  | 11.76 ±0.03           | 1.00×    | 85.07  | 20      | 0.37%         |
| khashl              | 12.48 ±0.17           | 1.06×    | 80.14  | 20      | 1.94%         |
| hwire_table (8N)    | 12.73 ±0.05           | 1.08×    | 78.54  | 20      | 0.58%         |
| hwire_table (2N)    | 13.29 ±1.27           | 1.13×    | 75.22  | 100     | 5.41% (unmet) |
| absl::flat_hash_map | 15.51 ±0.08           | 1.32×    | 64.46  | 20      | 0.72%         |
| hwire_table (4N)    | 16.57 ±0.05           | 1.41×    | 60.36  | 20      | 0.46%         |


### Miss

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| CC                  | 7.81 ±0.03            | 1.00×    | 128.10 | 20      | 0.57%         |
| khashl              | 8.55 ±0.09            | 1.10×    | 116.91 | 20      | 1.42%         |
| absl::flat_hash_map | 8.69 ±0.02            | 1.11×    | 115.04 | 20      | 0.36%         |
| hwire_table (8N)    | 9.43 ±0.05            | 1.21×    | 106.05 | 20      | 0.79%         |
| hwire_table (4N)    | 9.74 ±0.04            | 1.25×    | 102.63 | 20      | 0.58%         |
| hwire_table (2N)    | 10.52 ±0.40           | 1.35×    | 95.04  | 100     | 2.17% (unmet) |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| CC                  | 7.40 ±0.04            | 1.00×    | 135.22 | 20      | 0.75%         |
| khashl              | 8.55 ±0.04            | 1.16×    | 116.94 | 20      | 0.61%         |
| absl::flat_hash_map | 9.58 ±0.13            | 1.30×    | 104.42 | 20      | 1.95%         |
| hwire_table (4N)    | 9.95 ±0.37            | 1.34×    | 100.54 | 100     | 2.09% (unmet) |
| hwire_table (8N)    | 10.07 ±0.05           | 1.36×    | 99.26  | 20      | 0.63%         |
| hwire_table (2N)    | 10.87 ±0.08           | 1.47×    | 91.97  | 20      | 1.09%         |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.38 ±0.06            | 1.00×    | 135.54 | 20      | 1.23% |
| absl::flat_hash_map | 9.91 ±0.07            | 1.34×    | 100.91 | 20      | 1.03% |
| hwire_table (4N)    | 10.60 ±0.15           | 1.44×    | 94.32  | 20      | 1.99% |
| hwire_table (8N)    | 10.67 ±0.03           | 1.45×    | 93.71  | 20      | 0.43% |
| hwire_table (2N)    | 10.88 ±0.38           | 1.48×    | 91.88  | 100     | 1.99% |
| khashl              | 11.63 ±0.03           | 1.58×    | 86.01  | 20      | 0.34% |


### Build + Lookup Total Cost

Estimated from measured means as Build + Q × lookup time, rather than a timed first lookup immediately after construction. Each table uses the fastest Build + 1 lookup as its baseline. The crossover column is the minimum total lookup count Q that makes a map faster than that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**32 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 0.378              | 1.00×    | 10.04 ±0.06               | Baseline                 |
| hwire_table (8N)    | 0.409              | 1.08×    | 11.44 ±0.16               | No crossover             |
| hwire_table (4N)    | 0.411              | 1.09×    | 11.55 ±0.08               | No crossover             |
| CC                  | 0.423              | 1.12×    | 10.32 ±0.03               | No crossover             |
| hwire_table (2N)    | 0.426              | 1.12×    | 16.36 ±0.22               | No crossover             |
| absl::flat_hash_map | 0.614              | 1.62×    | 13.58 ±0.07               | No crossover             |

**64 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 0.684              | 1.00×    | 14.15 ±0.03               | Baseline                 |
| hwire_table (4N)    | 0.820              | 1.20×    | 12.24 ±0.11               | 73                       |
| hwire_table (8N)    | 0.827              | 1.21×    | 12.39 ±0.06               | 82                       |
| hwire_table (2N)    | 0.828              | 1.21×    | 12.60 ±0.07               | 94                       |
| CC                  | 0.847              | 1.24×    | 11.29 ±0.14               | 58                       |
| absl::flat_hash_map | 1.207              | 1.76×    | 14.49 ±0.10               | No crossover             |

**128 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 1.306              | 1.00×    | 12.48 ±0.17               | Baseline                 |
| CC                  | 1.610              | 1.23×    | 11.76 ±0.03               | 421                      |
| hwire_table (4N)    | 1.633              | 1.25×    | 16.57 ±0.05               | No crossover             |
| hwire_table (2N) †  | 1.639              | 1.25×    | 13.29 ±1.27               | No crossover             |
| hwire_table (8N)    | 1.646              | 1.26×    | 12.73 ±0.05               | No crossover             |
| absl::flat_hash_map | 2.456              | 1.88×    | 15.51 ±0.08               | No crossover             |


#### Build + Miss

**32 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.377               | 1.00×    | 8.55 ±0.09                 | Baseline                 |
| hwire_table (8N)    | 0.407               | 1.08×    | 9.43 ±0.05                 | No crossover             |
| hwire_table (4N)    | 0.409               | 1.09×    | 9.74 ±0.04                 | No crossover             |
| hwire_table (2N) †  | 0.420               | 1.11×    | 10.52 ±0.40                | No crossover             |
| CC                  | 0.420               | 1.12×    | 7.81 ±0.03                 | 60                       |
| absl::flat_hash_map | 0.609               | 1.62×    | 8.69 ±0.02                 | No crossover             |

**64 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.678               | 1.00×    | 8.55 ±0.04                 | Baseline                 |
| hwire_table (4N) †  | 0.818               | 1.21×    | 9.95 ±0.37                 | No crossover             |
| hwire_table (8N)    | 0.825               | 1.22×    | 10.07 ±0.05                | No crossover             |
| hwire_table (2N)    | 0.826               | 1.22×    | 10.87 ±0.08                | No crossover             |
| CC                  | 0.843               | 1.24×    | 7.40 ±0.04                 | 144                      |
| absl::flat_hash_map | 1.202               | 1.77×    | 9.58 ±0.13                 | No crossover             |

**128 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 1.306               | 1.00×    | 11.63 ±0.03                | Baseline                 |
| CC                  | 1.606               | 1.23×    | 7.38 ±0.06                 | 72                       |
| hwire_table (4N)    | 1.627               | 1.25×    | 10.60 ±0.15                | 315                      |
| hwire_table (2N)    | 1.637               | 1.25×    | 10.88 ±0.38                | 447                      |
| hwire_table (8N)    | 1.644               | 1.26×    | 10.67 ±0.03                | 356                      |
| absl::flat_hash_map | 2.450               | 1.88×    | 9.91 ±0.07                 | 669                      |


## Case-Insensitive Reserved Capacity

ASCII case-insensitive comparisons. Store 32, 64 or 128 unique keys with storage reserved for the entire dataset. Build includes initial allocation, initialization and all insertions; no capacity expansion occurs.

Cleanup runs outside all timed intervals.

Insert excludes initial setup and includes any expansion triggered by insertion. Hit and miss measure the fully populated maps.


### Memory

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

**32 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 414.39 ±2.44         | 1.00×    | 2.41   | 20      | 0.82% |
| hwire_table (8N)    | 435.28 ±3.07         | 1.05×    | 2.30   | 20      | 0.99% |
| hwire_table (4N)    | 436.62 ±2.66         | 1.05×    | 2.29   | 20      | 0.85% |
| hwire_table (2N)    | 455.86 ±2.36         | 1.10×    | 2.19   | 20      | 0.72% |
| CC                  | 489.98 ±2.88         | 1.18×    | 2.04   | 20      | 0.82% |
| absl::flat_hash_map | 650.54 ±4.62         | 1.57×    | 1.54   | 20      | 0.99% |

**64 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 757.57 ±5.15         | 1.00×    | 1.32   | 20      | 0.95% |
| hwire_table (2N)    | 875.30 ±3.64         | 1.16×    | 1.14   | 20      | 0.58% |
| hwire_table (8N)    | 885.95 ±4.47         | 1.17×    | 1.13   | 20      | 0.70% |
| hwire_table (4N)    | 886.98 ±3.63         | 1.17×    | 1.13   | 20      | 0.57% |
| CC                  | 913.14 ±4.26         | 1.21×    | 1.10   | 20      | 0.65% |
| absl::flat_hash_map | 1294.48 ±4.83        | 1.71×    | 0.77   | 20      | 0.52% |

**128 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| khashl              | 1472.01 ±12.44       | 1.00×    | 0.68   | 20      | 1.18% |
| CC                  | 1716.14 ±10.21       | 1.17×    | 0.58   | 20      | 0.83% |
| hwire_table (2N)    | 1783.54 ±4.05        | 1.21×    | 0.56   | 20      | 0.32% |
| hwire_table (8N)    | 1786.24 ±8.15        | 1.21×    | 0.56   | 20      | 0.64% |
| hwire_table (4N)    | 1797.64 ±7.27        | 1.22×    | 0.56   | 20      | 0.57% |
| absl::flat_hash_map | 2625.31 ±7.49        | 1.78×    | 0.38   | 20      | 0.40% |


### Insert

**32 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 10.78 ±0.07        | 1.00×    | 92.77  | 20      | 0.88% |
| hwire_table (8N)    | 12.90 ±0.17        | 1.20×    | 77.50  | 20      | 1.86% |
| hwire_table (4N)    | 12.94 ±0.08        | 1.20×    | 77.27  | 20      | 0.88% |
| hwire_table (2N)    | 13.59 ±0.09        | 1.26×    | 73.60  | 20      | 0.96% |
| CC                  | 14.30 ±0.10        | 1.33×    | 69.92  | 20      | 1.01% |
| absl::flat_hash_map | 18.86 ±0.10        | 1.75×    | 53.03  | 20      | 0.73% |

**64 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 10.81 ±0.06        | 1.00×    | 92.47  | 20      | 0.80% |
| hwire_table (2N)    | 13.30 ±0.08        | 1.23×    | 75.20  | 20      | 0.81% |
| hwire_table (8N)    | 13.39 ±0.05        | 1.24×    | 74.67  | 20      | 0.50% |
| hwire_table (4N)    | 13.48 ±0.08        | 1.25×    | 74.17  | 20      | 0.80% |
| CC                  | 13.72 ±0.08        | 1.27×    | 72.89  | 20      | 0.82% |
| absl::flat_hash_map | 19.54 ±0.11        | 1.81×    | 51.19  | 20      | 0.80% |

**128 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| khashl              | 10.86 ±0.05        | 1.00×    | 92.08  | 20      | 0.65% |
| CC                  | 13.08 ±0.10        | 1.20×    | 76.44  | 20      | 1.07% |
| hwire_table (8N)    | 13.69 ±0.06        | 1.26×    | 73.05  | 20      | 0.63% |
| hwire_table (2N)    | 13.71 ±0.04        | 1.26×    | 72.95  | 20      | 0.45% |
| hwire_table (4N)    | 13.82 ±0.06        | 1.27×    | 72.36  | 20      | 0.59% |
| absl::flat_hash_map | 20.15 ±0.28        | 1.86×    | 49.63  | 30      | 1.53% |


### Hit

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| khashl              | 18.14 ±0.47           | 1.00×    | 55.13  | 70      | 1.79% |
| CC                  | 18.59 ±0.20           | 1.02×    | 53.80  | 20      | 1.54% |
| hwire_table (8N)    | 18.69 ±0.19           | 1.03×    | 53.52  | 20      | 1.46% |
| hwire_table (4N)    | 18.72 ±0.13           | 1.03×    | 53.43  | 20      | 1.00% |
| hwire_table (2N)    | 19.02 ±0.18           | 1.05×    | 52.57  | 20      | 1.29% |
| absl::flat_hash_map | 21.75 ±0.19           | 1.20×    | 45.99  | 20      | 1.20% |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| khashl              | 17.96 ±0.37           | 1.00×    | 55.69  | 40      | 1.92% |
| CC                  | 18.02 ±0.10           | 1.00×    | 55.50  | 20      | 0.76% |
| hwire_table (4N)    | 18.29 ±0.09           | 1.02×    | 54.67  | 20      | 0.71% |
| hwire_table (8N)    | 18.30 ±0.08           | 1.02×    | 54.64  | 20      | 0.60% |
| hwire_table (2N)    | 18.44 ±0.17           | 1.03×    | 54.22  | 20      | 1.32% |
| absl::flat_hash_map | 21.49 ±0.18           | 1.20×    | 46.54  | 20      | 1.16% |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 19.36 ±0.07           | 1.00×    | 51.64  | 20      | 0.53% |
| hwire_table (2N)    | 19.37 ±0.26           | 1.00×    | 51.62  | 30      | 1.47% |
| hwire_table (8N)    | 19.58 ±0.12           | 1.01×    | 51.08  | 20      | 0.87% |
| khashl              | 19.77 ±0.09           | 1.02×    | 50.58  | 20      | 0.61% |
| CC                  | 19.79 ±0.04           | 1.02×    | 50.52  | 20      | 0.29% |
| absl::flat_hash_map | 22.81 ±0.29           | 1.18×    | 43.84  | 20      | 1.80% |


### Miss

**32 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.86 ±0.10            | 1.00×    | 127.29 | 30      | 1.46% |
| khashl              | 8.29 ±0.03            | 1.06×    | 120.57 | 20      | 0.52% |
| hwire_table (8N)    | 9.66 ±0.12            | 1.23×    | 103.49 | 20      | 1.78% |
| hwire_table (4N)    | 9.84 ±0.02            | 1.25×    | 101.63 | 20      | 0.33% |
| absl::flat_hash_map | 10.55 ±0.14           | 1.34×    | 94.81  | 20      | 1.82% |
| hwire_table (2N)    | 12.71 ±0.04           | 1.62×    | 78.66  | 20      | 0.48% |

**64 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.67 ±0.07            | 1.00×    | 130.46 | 20      | 1.27% |
| hwire_table (4N)    | 9.94 ±0.21            | 1.30×    | 100.57 | 50      | 1.76% |
| hwire_table (8N)    | 9.98 ±0.07            | 1.30×    | 100.23 | 20      | 1.04% |
| khashl              | 10.01 ±0.10           | 1.31×    | 99.95  | 20      | 1.39% |
| absl::flat_hash_map | 10.68 ±0.10           | 1.39×    | 93.61  | 20      | 1.28% |
| hwire_table (2N)    | 11.29 ±0.11           | 1.47×    | 88.54  | 20      | 1.34% |

**128 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 7.88 ±0.05            | 1.00×    | 126.97 | 20      | 0.83% |
| khashl              | 9.37 ±0.14            | 1.19×    | 106.71 | 30      | 1.59% |
| hwire_table (4N)    | 9.75 ±0.26            | 1.24×    | 102.53 | 60      | 1.96% |
| hwire_table (8N)    | 9.99 ±0.05            | 1.27×    | 100.08 | 20      | 0.65% |
| hwire_table (2N)    | 10.61 ±0.11           | 1.35×    | 94.28  | 20      | 1.48% |
| absl::flat_hash_map | 10.82 ±0.09           | 1.37×    | 92.41  | 20      | 1.18% |


### Build + Lookup Total Cost

Estimated from measured means as Build + Q × lookup time, rather than a timed first lookup immediately after construction. Each table uses the fastest Build + 1 lookup as its baseline. The crossover column is the minimum total lookup count Q that makes a map faster than that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**32 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 0.433              | 1.00×    | 18.14 ±0.47               | Baseline                 |
| hwire_table (8N)    | 0.454              | 1.05×    | 18.69 ±0.19               | No crossover             |
| hwire_table (4N)    | 0.455              | 1.05×    | 18.72 ±0.13               | No crossover             |
| hwire_table (2N)    | 0.475              | 1.10×    | 19.02 ±0.18               | No crossover             |
| CC                  | 0.509              | 1.18×    | 18.59 ±0.20               | No crossover             |
| absl::flat_hash_map | 0.672              | 1.55×    | 21.75 ±0.19               | No crossover             |

**64 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 0.776              | 1.00×    | 17.96 ±0.37               | Baseline                 |
| hwire_table (2N)    | 0.894              | 1.15×    | 18.44 ±0.17               | No crossover             |
| hwire_table (8N)    | 0.904              | 1.17×    | 18.30 ±0.08               | No crossover             |
| hwire_table (4N)    | 0.905              | 1.17×    | 18.29 ±0.09               | No crossover             |
| CC                  | 0.931              | 1.20×    | 18.02 ±0.10               | No crossover             |
| absl::flat_hash_map | 1.316              | 1.70×    | 21.49 ±0.18               | No crossover             |

**128 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| khashl              | 1.492              | 1.00×    | 19.77 ±0.09               | Baseline                 |
| CC                  | 1.736              | 1.16×    | 19.79 ±0.04               | No crossover             |
| hwire_table (2N)    | 1.803              | 1.21×    | 19.37 ±0.26               | 782                      |
| hwire_table (8N)    | 1.806              | 1.21×    | 19.58 ±0.12               | 1,615                    |
| hwire_table (4N)    | 1.817              | 1.22×    | 19.36 ±0.07               | 798                      |
| absl::flat_hash_map | 2.648              | 1.78×    | 22.81 ±0.29               | No crossover             |


#### Build + Miss

**32 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.423               | 1.00×    | 8.29 ±0.03                 | Baseline                 |
| hwire_table (8N)    | 0.445               | 1.05×    | 9.66 ±0.12                 | No crossover             |
| hwire_table (4N)    | 0.446               | 1.06×    | 9.84 ±0.02                 | No crossover             |
| hwire_table (2N)    | 0.469               | 1.11×    | 12.71 ±0.04                | No crossover             |
| CC                  | 0.498               | 1.18×    | 7.86 ±0.10                 | 173                      |
| absl::flat_hash_map | 0.661               | 1.56×    | 10.55 ±0.14                | No crossover             |

**64 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 0.768               | 1.00×    | 10.01 ±0.10                | Baseline                 |
| hwire_table (2N)    | 0.887               | 1.16×    | 11.29 ±0.11                | No crossover             |
| hwire_table (8N)    | 0.896               | 1.17×    | 9.98 ±0.07                 | 4,608                    |
| hwire_table (4N)    | 0.897               | 1.17×    | 9.94 ±0.21                 | 2,102                    |
| CC                  | 0.921               | 1.20×    | 7.67 ±0.07                 | 67                       |
| absl::flat_hash_map | 1.305               | 1.70×    | 10.68 ±0.10                | No crossover             |

**128 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| khashl              | 1.481               | 1.00×    | 9.37 ±0.14                 | Baseline                 |
| CC                  | 1.724               | 1.16×    | 7.88 ±0.05                 | 164                      |
| hwire_table (2N)    | 1.794               | 1.21×    | 10.61 ±0.11                | No crossover             |
| hwire_table (8N)    | 1.796               | 1.21×    | 9.99 ±0.05                 | No crossover             |
| hwire_table (4N)    | 1.807               | 1.22×    | 9.75 ±0.26                 | No crossover             |
| absl::flat_hash_map | 2.636               | 1.78×    | 10.82 ±0.09                | No crossover             |


## Case-Sensitive Allocated Growth

Exact byte comparisons. Grow from an initial reservation of 32 keys to 256 unique keys with three additional extensions. Build includes initial allocation, initialization, insertion and expansion work, including ordinary heap allocations and any copying or rehashing.

Cleanup runs outside all timed intervals.

Insert excludes initial setup and includes any expansion triggered by insertion. Hit and miss measure the fully populated maps.


### Memory

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

**256 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| hwire_table (8N)    | 3589.02 ±12.21       | 1.00×    | 0.28   | 20      | 0.48% |
| hwire_table (4N)    | 3611.67 ±9.26        | 1.01×    | 0.28   | 20      | 0.36% |
| hwire_table (2N)    | 3632.29 ±10.99       | 1.01×    | 0.28   | 20      | 0.42% |
| khashl              | 7377.95 ±34.28       | 2.06×    | 0.14   | 20      | 0.65% |
| CC                  | 9403.04 ±40.05       | 2.62×    | 0.11   | 20      | 0.60% |
| absl::flat_hash_map | 9660.72 ±22.33       | 2.69×    | 0.10   | 20      | 0.32% |


### Insert

**256 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 13.97 ±0.05        | 1.00×    | 71.58  | 20      | 0.50% |
| hwire_table (8N)    | 13.99 ±0.19        | 1.00×    | 71.50  | 20      | 1.86% |
| hwire_table (2N)    | 14.05 ±0.04        | 1.01×    | 71.20  | 20      | 0.36% |
| khashl              | 27.91 ±0.32        | 2.00×    | 35.83  | 20      | 1.62% |
| CC                  | 36.64 ±0.58        | 2.62×    | 27.29  | 30      | 1.72% |
| absl::flat_hash_map | 37.47 ±0.09        | 2.68×    | 26.69  | 20      | 0.33% |


### Hit

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 12.92 ±0.05           | 1.00×    | 77.42  | 20      | 0.50% |
| khashl              | 13.19 ±0.04           | 1.02×    | 75.82  | 20      | 0.42% |
| absl::flat_hash_map | 16.88 ±0.06           | 1.31×    | 59.22  | 20      | 0.54% |
| hwire_table (8N)    | 26.24 ±0.59           | 2.03×    | 38.11  | 60      | 1.66% |
| hwire_table (4N)    | 28.39 ±0.07           | 2.20×    | 35.22  | 20      | 0.34% |
| hwire_table (2N)    | 30.56 ±0.25           | 2.37×    | 32.72  | 20      | 1.14% |


### Miss

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW          |
| ------------------- | --------------------- | -------- | ------ | ------- | ------------- |
| CC                  | 8.03 ±0.07            | 1.00×    | 124.50 | 20      | 1.14%         |
| khashl              | 8.61 ±0.14            | 1.07×    | 116.14 | 30      | 1.81%         |
| absl::flat_hash_map | 11.43 ±0.06           | 1.42×    | 87.49  | 20      | 0.77%         |
| hwire_table (8N)    | 21.82 ±0.16           | 2.72×    | 45.83  | 20      | 1.02%         |
| hwire_table (4N)    | 22.27 ±0.28           | 2.77×    | 44.89  | 20      | 1.78%         |
| hwire_table (2N)    | 27.26 ±1.01           | 3.39×    | 36.68  | 100     | 2.11% (unmet) |


### Build + Lookup Total Cost

Estimated from measured means as Build + Q × lookup time, rather than a timed first lookup immediately after construction. Each table uses the fastest Build + 1 lookup as its baseline. The crossover column is the minimum total lookup count Q that makes a map faster than that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**256 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (8N)    | 3.615              | 1.00×    | 26.24 ±0.59               | Baseline                 |
| hwire_table (4N)    | 3.640              | 1.01×    | 28.39 ±0.07               | No crossover             |
| hwire_table (2N)    | 3.663              | 1.01×    | 30.56 ±0.25               | No crossover             |
| khashl              | 7.391              | 2.04×    | 13.19 ±0.04               | 291                      |
| CC                  | 9.416              | 2.60×    | 12.92 ±0.05               | 437                      |
| absl::flat_hash_map | 9.678              | 2.68×    | 16.88 ±0.06               | 650                      |


#### Build + Miss

**256 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (8N)    | 3.611               | 1.00×    | 21.82 ±0.16                | Baseline                 |
| hwire_table (4N)    | 3.634               | 1.01×    | 22.27 ±0.28                | No crossover             |
| hwire_table (2N) †  | 3.660               | 1.01×    | 27.26 ±1.01                | No crossover             |
| khashl              | 7.387               | 2.05×    | 8.61 ±0.14                 | 287                      |
| CC                  | 9.411               | 2.61×    | 8.03 ±0.07                 | 422                      |
| absl::flat_hash_map | 9.672               | 2.68×    | 11.43 ±0.06                | 585                      |


## Case-Insensitive Allocated Growth

ASCII case-insensitive comparisons. Grow from an initial reservation of 32 keys to 256 unique keys with three additional extensions. Build includes initial allocation, initialization, insertion and expansion work, including ordinary heap allocations and any copying or rehashing.

Cleanup runs outside all timed intervals.

Insert excludes initial setup and includes any expansion triggered by insertion. Hit and miss measure the fully populated maps.


### Memory

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

**256 keys**

| Map                 | Mean ± SD (ns/table) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | -------------------- | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 3810.06 ±44.45       | 1.00×    | 0.26   | 20      | 1.63% |
| hwire_table (2N)    | 3853.71 ±67.13       | 1.01×    | 0.26   | 30      | 1.91% |
| hwire_table (8N)    | 3854.70 ±11.81       | 1.01×    | 0.26   | 20      | 0.43% |
| khashl              | 8167.87 ±111.49      | 2.14×    | 0.12   | 30      | 1.49% |
| CC                  | 9536.86 ±61.44       | 2.50×    | 0.10   | 20      | 0.90% |
| absl::flat_hash_map | 9663.26 ±19.26       | 2.54×    | 0.10   | 20      | 0.28% |


### Insert

**256 keys**

| Map                 | Mean ± SD (ns/key) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | ------------------ | -------- | ------ | ------- | ----- |
| hwire_table (4N)    | 14.80 ±0.04        | 1.00×    | 67.55  | 20      | 0.41% |
| hwire_table (2N)    | 14.83 ±0.04        | 1.00×    | 67.45  | 20      | 0.37% |
| hwire_table (8N)    | 14.93 ±0.04        | 1.01×    | 66.98  | 20      | 0.41% |
| khashl              | 31.42 ±0.14        | 2.12×    | 31.83  | 20      | 0.63% |
| CC                  | 36.60 ±0.14        | 2.47×    | 27.32  | 20      | 0.52% |
| absl::flat_hash_map | 37.60 ±0.11        | 2.54×    | 26.59  | 20      | 0.39% |


### Hit

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 20.77 ±0.05           | 1.00×    | 48.14  | 20      | 0.32% |
| khashl              | 20.85 ±0.08           | 1.00×    | 47.96  | 20      | 0.57% |
| absl::flat_hash_map | 23.83 ±0.30           | 1.15×    | 41.97  | 30      | 1.40% |
| hwire_table (8N)    | 25.44 ±0.72           | 1.22×    | 39.30  | 70      | 1.95% |
| hwire_table (4N)    | 26.15 ±0.16           | 1.26×    | 38.24  | 20      | 0.83% |
| hwire_table (2N)    | 29.83 ±0.55           | 1.44×    | 33.53  | 40      | 1.71% |


### Miss

**256 keys**

| Map                 | Mean ± SD (ns/lookup) | Relative | Mops/s | Samples | RCIW  |
| ------------------- | --------------------- | -------- | ------ | ------- | ----- |
| CC                  | 8.03 ±0.08            | 1.00×    | 124.61 | 20      | 1.42% |
| khashl              | 10.43 ±0.07           | 1.30×    | 95.85  | 20      | 0.88% |
| absl::flat_hash_map | 11.28 ±0.05           | 1.41×    | 88.67  | 20      | 0.65% |
| hwire_table (4N)    | 20.66 ±0.11           | 2.57×    | 48.40  | 20      | 0.72% |
| hwire_table (8N)    | 20.98 ±0.29           | 2.61×    | 47.67  | 30      | 1.51% |
| hwire_table (2N)    | 30.27 ±0.31           | 3.77×    | 33.04  | 20      | 1.44% |


### Build + Lookup Total Cost

Estimated from measured means as Build + Q × lookup time, rather than a timed first lookup immediately after construction. Each table uses the fastest Build + 1 lookup as its baseline. The crossover column is the minimum total lookup count Q that makes a map faster than that baseline; No crossover means it cannot overtake under this model. † marks an input with unmet Target RCIW; small timing differences make crossover estimates uncertain.


#### Build + Hit

**256 keys**

| Map                 | Build + 1 Hit (µs) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------ | -------- | ------------------------- | ------------------------ |
| hwire_table (4N)    | 3.836              | 1.00×    | 26.15 ±0.16               | Baseline                 |
| hwire_table (8N)    | 3.880              | 1.01×    | 25.44 ±0.72               | 64                       |
| hwire_table (2N)    | 3.884              | 1.01×    | 29.83 ±0.55               | No crossover             |
| khashl              | 8.189              | 2.13×    | 20.85 ±0.08               | 823                      |
| CC                  | 9.558              | 2.49×    | 20.77 ±0.05               | 1,066                    |
| absl::flat_hash_map | 9.687              | 2.53×    | 23.83 ±0.30               | 2,523                    |


#### Build + Miss

**256 keys**

| Map                 | Build + 1 Miss (µs) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ------------------- | -------- | -------------------------- | ------------------------ |
| hwire_table (4N)    | 3.831               | 1.00×    | 20.66 ±0.11                | Baseline                 |
| hwire_table (8N)    | 3.876               | 1.01×    | 20.98 ±0.29                | No crossover             |
| hwire_table (2N)    | 3.884               | 1.01×    | 30.27 ±0.31                | No crossover             |
| khashl              | 8.178               | 2.13×    | 10.43 ±0.07                | 427                      |
| CC                  | 9.545               | 2.49×    | 8.03 ±0.08                 | 454                      |
| absl::flat_hash_map | 9.675               | 2.53×    | 11.28 ±0.05                | 624                      |
