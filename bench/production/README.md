# Benchmarking Production Usage

Measure request parsing, native header storage and string lookup. Input
preparation, system allocation and cleanup are excluded under the
preallocated-memory policy below.


## Workload

The production workload compares HTTP request parsing, native header storage
and header lookup with memory preallocated. It excludes routing, header-value
validation, body processing and query decomposition. The input is a browser
GET request forwarded through a CDN; artificial header-growth inputs are
excluded.

| Outside timing | Inside timing |
| --- | --- |
| Memory allocation for request objects, storage and pool backing | Acquisition of preallocated memory and object/storage initialization |
| Provisioning memory needed by storage expansion | Parsing, native name normalization, copying and header insertion |
| Input preparation and context cleanup | Lookup-index construction and container expansion using preallocated memory |

Every parse operation starts with empty header storage. Only allocated capacity may
be reused; previous headers, lookup indexes and cached lookup results must not
be reused. The preallocated-memory policy applies to request contexts and all header
storage, including storage created after parsing.
Lookup timing uses a completed context.


## Metrics

| Metric | Meaning |
| --- | --- |
| Mean ± SD | Mean time per operation ± sample standard deviation; each sample averages repeated operations |
| Relative | Mean / fastest mean for the same operation and fixture; 1.00× is the baseline and 1.20× means 20% more time |
| Throughput | Operations per second, calculated as 1 second / mean operation time |
| RCIW | Full confidence interval width / mean; describes uncertainty in the estimated mean |
| Message bytes | Input length through the header terminator, including CRLF and excluding the C string terminator |

Parse + Post-process includes initialization, parsing and native header storage.
Lookup measurements use completed contexts.


### Sampling

Adaptive sampling uses 20–100 samples, Target RCIW 2% and approximately 1 ms
per sample. Parse operations are timed individually; lookup samples use batches.
Empty timer intervals are subtracted. Progress reports samples and achieved
RCIW; unmet targets are marked.


## Adding a Benchmark Target

Add `<name>/request.c` under this suite. The directory
name must match `[A-Za-z][A-Za-z0-9_]*`. Directories prefixed with `_` are disabled.
The suite's registration script discovers exported functions and generates the
included registration table. C declarations of Rust-exported functions are also
supported. Adapters contain native processing and no timers or driver macros.

### Dependencies and build configuration

Each implementation owns its `fetch.sh`, `config.mk` and dependency directory.

- Optional `fetch.sh` downloads pinned native sources and returns nonzero on
  failure. It runs before compilation; repeated setup reuses fetched revisions.
- Optional `config.mk` provides the variables below, using paths relative to
  this suite and the implementation directory as the variable prefix.
- Optional `build.sh` builds a native library before linking the timing binary.
  It receives the build variant as its first argument and `CARGO` through the
  environment. Native Rust builds use locked dependencies and run offline.
- Ignore downloaded sources and native build caches. Disabled implementations
  contribute no sources, configuration, setup or native builds.

| Variable | Meaning |
| --- | --- |
| `<name>_SOURCES` | Additional native translation units; adapter `*.c` files are discovered automatically |
| `<name>_CPPFLAGS` | Include paths and preprocessor flags |
| `<name>_CFLAGS` | Compilation flags for sources under the implementation directory |
| `<name>_LDLIBS` | Link flags; `%` expands to the build variant |
| `<name>_NAME` | Report display name; defaults to the directory name |
| `<name>_BUILD_INFO` | Native toolchain/build description recorded in the measurement metadata |
| `<name>_BUILD_DEPS` | Native source/manifests that trigger rebuilding |

### Entry points

The caller supplies valid fixture bytes; omit argument validation and result
validation in timed adapters. Return zero on success and nonzero on failure.
The caller initializes the context pointer to NULL before invoking an entry
point and retains the input until context cleanup. Cleanup is outside timing.

```c
/**
 * Initialize native request/storage state and parse a request into that storage.
 * Called once per timed operation during warmup, calibration and sampling.
 * Native allocation uses the driver's preallocated arena on Linux and macOS; include
 * initialization, conversion, storage insertion and necessary growth here.
 * @param context Owned output used by later lookup; caller sets it to NULL.
 * @param data Valid writable input retained until context cleanup.
 * @param len Input byte length.
 * @param header_capacity Application header limit where the native API supports it.
 * @return Zero on success; nonzero on acquisition or parsing failure.
 */
int name_request_with_store(void **context, const unsigned char *data,
                            size_t len, size_t header_capacity);

/**
 * Search completed native storage from the original header-name string.
 * Called repeatedly inside lookup timing. Include required native name
 * conversion, hashing and temporary allocation/release. Do not cache converted
 * identifiers or matching entries across calls.
 * @param context Completed output from request_with_store.
 * @param key Valid ASCII name bytes, not necessarily NUL-terminated.
 * @param len Name byte length.
 * @return Value length + 1 on hit, zero on miss; empty values return 1.
 */
size_t name_header_lookup(const void *context, const char *key, size_t len);

/**
 * Release the native context and its storage after timing; accept NULL.
 * Release native recycling caches before the driver resets its arena.
 */
void name_context_free(void *context);
```


# Benchmark

<!-- benchmark-environment -->
## Environment

```text
date                 : 2026-10-07T10:00:05+09:00
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
```
<!-- /benchmark-environment -->

## Requirements

- GNU Make: Build and run the suite.
- Python 3.10+: Register adapters, generate inputs and render reports.
- C11 compiler and linker: Compile the driver, C adapters and arena helpers.
- Cargo and rustc 1.88+: Required when Rust adapters are enabled.
- `curl`, `tar` and network access: Fetch pinned dependencies during initial setup.
- OpenSSL development headers: Required when the H2O target is enabled (for example, `libssl-dev` on Debian/Ubuntu). `pkg-config` discovers nonstandard include paths; otherwise add the include path to `CFLAGS`.

- libuv development headers: Required for the H2O target on macOS; include paths are discovered with `pkg-config`.

Dependencies are fetched before timing. Repeated setup reuses downloaded
revisions; each target owns its `fetch.sh` and `config.mk`.


## Commands

Run from `bench/production/`.

| Command | Action |
| --- | --- |
| `make` / `make run` | Build, measure and report native request/header processing and lookup |
| `make setup` | Fetch pinned dependencies for active implementations |
| `make build` | Build the active production variants |
| `make VARIANTS=siphash` | Measure the SipHash fallback |
| `make report` | Render saved measurements |
| `make update-readme` | Publish saved measurements in this README |
| `make check` | Run arena regression and adapter checks outside timing |
| `make list` | List implementations and variants |
| `make clean` | Remove timing binaries/results; retain dependency caches |

Downloads live in each
implementation's ignored dependency directories. Raw results and metadata live in `results/`;
binaries and generated registration tables live in `bin/`. These files are not
committed. Timing binaries support `--quick` for development and `--check` for
untimed success checks; `--quick` is not publication sampling.


### Configuration

The default `native` build uses `-march=native` on x86-64 and `-mcpu=native`
on ARM. Additional variants and native-library flags are documented under
Benchmark.

Linux and macOS production builds redirect malloc, calloc, realloc,
aligned_alloc, posix_memalign and free to the same reusable aligned arena.
Linux uses linker wrapping; macOS links a driver-owned interposing dylib,
loaded relative to the timing binary without additional environment variables.
Arena acquisition and per-operation reset are outside timing. Allocation
bookkeeping, initialization, insertion and table linking remain timed.
Other platforms use ordinary allocation, including its cost in timing;
reports identify the allocation policy. Pool hooks belong to the driver.


Release C builds use `-O2 -DNDEBUG -std=c11`.


## Benchmark Targets

Compiler and flag entries below record the published measurements.

These targets compare direct header-map construction, native pool/list storage
and conversion from stack-parsed headers into application storage. They model
header-processing integrations, not complete server throughput.

### hwire + hwire_table

Callbacks retain request slices and construct a case-insensitive header map
during parsing.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](hwire/request.c)
- Library: Current sources in `../../src/`
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags -->
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
  - `siphash`: Native flags plus `-DHWIRE_NO_AES`; selects the SipHash-1-3 backend.
- Build: `C`; native CPU target; AES hash when available, otherwise SipHash-1-3

The adapter acquires `app_request_t` and a 32-entry table together, with the parser
context on the stack. The table uses an 8N index and a deterministic key derived
from seed 42. `make VARIANTS=siphash` adds `-DHWIRE_NO_AES` to select the fallback.
Names are general string keys, without a predefined header-name registry.

</details>


### nginx

Native request structures retain parsed headers in pool-backed list blocks,
with dedicated references for known header names.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](nginx/request.c)
- Library: [nginx 1.31.6](https://github.com/nginx/nginx/tree/release-1.31.6), revision `45a318d05a0fd23f57ffe9579f7f0969c0fe402a`
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags --> `-ffunction-sections -fdata-sections`
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
- Build: `C`; native CPU target; `--with-compat`

The adapter initializes the full request structure, a 4096-byte pool, incoming
and outgoing header lists and trailers. List blocks hold 20 headers; the
21-field input adds a second block. The build enables 31 known-name definitions.
Metadata retains upstream feature guards; value-validation handlers and routing
state are excluded.

</details>


### H2O

picohttpparser produces stack header slices, which are converted into the native
H2O request/header representation.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](h2o/request.c)
- Library: [H2O](https://github.com/h2o/h2o/tree/5da50541a4b6a038c9cea493f740747cf964f4a8), revision `5da50541a4b6a038c9cea493f740747cf964f4a8`; uses the bundled picohttpparser
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags --> `-D_GNU_SOURCE -ffunction-sections -fdata-sections`
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
- Build: `C`; native CPU target; OpenSSL development headers required

The adapter uses `h2o_req_t`, `h2o_init_request` and unmodified header, token,
string and memory functions. It reproduces HTTP/1 header population from a
100-field stack array, preserving original names and retaining method, path,
version and authority. Host is stored as authority. The registry defines 79
normal-header tokens and 6 pseudo-header tokens. `H2O_MAX_TOKENS` limits the
token-definition array, not the number of stored headers. TLS processing is
outside the workload.

</details>


### Actix Web

httparse produces stack header slices, which are converted into native Request,
RequestHead and HeaderMap objects.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](actix_web/request.c), [parser.rs](actix_web/parser.rs)
- Library: [Actix Web](https://github.com/actix/actix-web/tree/714572c5eca7a012b771dd9a978767e7573697a8), revision `714572c5eca7a012b771dd9a978767e7573697a8`; actix-http 3.18.12; httparse 1.10.1 fetched by Cargo and pinned in `Cargo.lock`
- Compiler: <!-- compiler:actix_web -->`rustc 1.93.1 (01f6ddf75 2026-02-11)`<!-- /compiler -->; C ABI shim: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags --> (C ABI shim)
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
- RUSTFLAGS: `-C target-cpu=native` for both variants.
- Build: <!-- build:actix_web -->opt-level=2; panic=abort; native CPU target; SIMD enabled at compile time<!-- /build -->

The adapter uses a 96-field stack array and native HeaderMap reservation for
16 headers. It reproduces the decoder's private HeaderIndex span-recording step,
then converts and retains method, URI, version and headers. Borrowed input has
shared Bytes ownership metadata for native HeaderValue construction. The
`http` 0.2.12 registry defines 81 standard header names.

The thread-local request-head pool is initialized outside timing and kept empty
during operations. Completed arena-backed heads are reclaimed by arena reset,
instead of being returned to that pool. Measurements reconstruct native objects
rather than retaining container capacity between requests.

</details>


## Production Usage Benchmark

This compares the adapters' HTTP parsing and native header-storage paths.
Request/storage memory is preallocated outside timing; the initialization,
parsing and storage-construction steps below are included in Parse + Post-process.

### Memory

Linux and macOS integrations use a common arena preallocated once outside timing. Other platforms include system allocation, as described under Configuration. Native
allocation calls use this backing memory; system malloc-family allocation
costs are excluded.

- Before each operation, reset the arena's allocation position outside timing.
- Reconstruct fresh request objects and header storage for each operation;
  reuse backing memory, not previously constructed objects or container state.
- Include region bookkeeping, structure/index initialization, calloc zeroing
  and realloc copying in timing.
- Run context cleanup and release native recycling caches outside timing,
  before resetting the arena for the next operation.

Each operation receives a fresh writable copy of the input, prepared outside
timing for native lowercase conversion and NUL termination. The receive buffer
stays alive until context cleanup. Temporary lookup-name allocations reuse arena space on last-in,
first-out release; other allocations are reclaimed by request cleanup and arena
reset. System malloc/free costs are excluded, but region acquisition and release
bookkeeping are timed. This models fresh header-storage construction using
preallocated memory.


### Header Storage

| Implementation | Capacity | Data structure | Lookup |
| --- | --- | --- | --- |
| hwire + hwire_table | During and after parsing: 32 headers; 256 slots (8N) | During and after parsing: hash map supporting duplicate values | Case-insensitive hash lookup |
| nginx | During and after parsing: 20 headers per array block | During and after parsing: linked array blocks | Resolve known-header references from the string; linear search for other headers |
| H2O | During parsing: 100 headers<br>After parsing: parsed header count, rounded up to a power of two | During parsing: stack array<br>After parsing: dynamic array | Linear search using header tokens or names |
| Actix Web | During parsing: 96 headers<br>After parsing: initial reservation for 16 headers, with growth as needed | During parsing: stack array<br>After parsing: hash map supporting duplicate values | Native case-insensitive hash lookup |

Capacity describes entry storage or the requested reservation. Hash-map
implementations may round reservations and grow internally. nginx links
additional array blocks when a block fills; individual headers are not linked
list nodes.


### Processing flow

| Phase | hwire + hwire_table | nginx | H2O | Actix Web |
| --- | --- | --- | --- | --- |
| Initialize (use pre-allocated memory) | Initialize the request parsing context and header storage | Initialize the request parsing context, memory pool and header storage | Initialize the native request context and memory pool; prepare a stack header array | Initialize the request parsing context and a stack header array |
| Parse | Parse and retain the request line; callbacks store each header immediately | Parse and retain the request line; store each header immediately | Parse the request line into the request context and headers into a stack array | Parse the request line and headers into a temporary request context and stack array |
| Post-process | None; headers are stored during parsing | None; headers are stored during parsing | Prepare storage and convert the stack-parsed headers into the native representation | Initialize the native request and header storage; convert and retain method, URI, version and headers |
| Lookup | Hash and search the original header-name string with `hwire_table_get_ci` | Lowercase and hash the string; resolve a known-header reference or search the header list | Lowercase the string and resolve its token; search by token or name, with Host read from authority | Pass the string to `HeaderMap::get`; convert it to `HeaderName` and search the map |

Initialize, Parse and Post-process are included in the measured
Parse + Post-process operation. Native container initialization may occur after parsing, as indicated
above. Implementations that store headers during parsing require no separate
post-processing step. Lookup is measured separately on the completed context;
each call starts from the original header-name string and includes the native
conversion and search shown above.

These flows cover header processing; server routing, body processing and
header-value validation handlers are excluded.

<!-- production-results -->

### Browser GET via CDN

Constructed CDN-to-origin browser navigation with 21 header fields, Cookie and forwarded-client metadata. Query text is parsed only as part of the request target.

<details>
<summary>Message (998 bytes)</summary>

```http
GET /search?q=cache%20locality&page=2&sort=recent&lang=ja HTTP/1.1
Host: app.example.com
Connection: keep-alive
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8
Accept-Encoding: br, gzip
Accept-Language: ja,en-US;q=0.9,en;q=0.8
Sec-Fetch-Site: same-origin
Sec-Fetch-Mode: navigate
Sec-Fetch-User: ?1
Sec-Fetch-Dest: document
Sec-CH-UA: "Chromium";v="131", "Not_A Brand";v="24"
Sec-CH-UA-Mobile: ?0
Sec-CH-UA-Platform: "macOS"
Referer: https://app.example.com/
Priority: u=0, i
Cookie: session=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef; csrf=abcdef0123456789abcdef0123456789; locale=ja_JP; theme=dark; consent=%7B%22analytics%22%3Atrue%7D
CF-Connecting-IP: 192.0.2.1
X-Forwarded-For: 192.0.2.1
X-Forwarded-Proto: https
Cf-Ray: 8a1234567890abcd-NRT

```

</details>

native CPU build; memory preallocated (system allocation excluded); application header limit 128; headers only.

Parse + Post-process includes initialization, HTTP parsing and native header storage, including any growth. Query decomposition, decoding and storage are excluded. Input preparation and context cleanup are outside timing.

† Target RCIW was not reached; calculated totals inherit the marker from either component.


#### Parse + Post-process

| Implementation      | Mean ± SD (ns/request) | Relative | Throughput   | RCIW  |
| ------------------- | ---------------------- | -------- | ------------ | ----- |
| hwire + hwire_table | 534.18 ±5.42           | 1.00×    | 1.87 M req/s | 1.42% |
| H2O                 | 817.78 ±27.96          | 1.53×    | 1.22 M req/s | 1.94% |
| nginx               | 1210.38 ±17.02         | 2.27×    | 0.83 M req/s | 1.97% |
| Actix Web           | 1362.86 ±12.34         | 2.55×    | 0.73 M req/s | 1.27% |

Known/unknown describes native header-name definitions. Unknown hit names are present in the message but absent from those definitions; hwire_table treats all names as strings. Key lengths differ between groups, so costs include both name representation and key length. Each lookup starts from the original string; native name conversion is timed. Time is the mean per lookup for these keys on a completed warm context.


#### Lookup Hit — Known Headers

| Implementation      | Mean ± SD (ns/lookup) | Relative | Throughput        | RCIW   |
| ------------------- | --------------------- | -------- | ----------------- | ------ |
| H2O                 | 16.02 ±0.43           | 1.00×    | 62.42 M lookups/s | 1.83%  |
| nginx               | 18.47 ±6.58 †         | 1.15×    | 54.14 M lookups/s | 20.21% |
| hwire + hwire_table | 18.67 ±1.80 †         | 1.17×    | 53.56 M lookups/s | 5.46%  |
| Actix Web           | 19.39 ±0.14           | 1.21×    | 51.57 M lookups/s | 0.97%  |

Searches Host, Accept, Cookie, User-Agent, Connection and Referer, in that order, repeated with equal frequency.


#### Lookup Hit — Unknown Headers

| Implementation      | Mean ± SD (ns/lookup) | Relative | Throughput        | RCIW   |
| ------------------- | --------------------- | -------- | ----------------- | ------ |
| hwire + hwire_table | 19.71 ±0.18           | 1.00×    | 50.74 M lookups/s | 1.28%  |
| H2O                 | 26.18 ±0.17           | 1.33×    | 38.20 M lookups/s | 0.88%  |
| nginx               | 42.88 ±18.09 †        | 2.18×    | 23.32 M lookups/s | 23.92% |
| Actix Web           | 50.87 ±0.14           | 2.58×    | 19.66 M lookups/s | 0.39%  |

Searches Sec-Fetch-Site, Sec-Fetch-Mode, Sec-Fetch-User, Sec-Fetch-Dest, Sec-CH-UA and Sec-CH-UA-Platform, in that order, repeated with equal frequency.


#### Lookup Hit — Mixed Headers

| Implementation      | Mean ± SD (ns/lookup) | Relative | Throughput        | RCIW  |
| ------------------- | --------------------- | -------- | ----------------- | ----- |
| hwire + hwire_table | 18.77 ±0.07           | 1.00×    | 53.28 M lookups/s | 0.49% |
| H2O                 | 20.79 ±0.15           | 1.11×    | 48.10 M lookups/s | 1.04% |
| nginx               | 21.88 ±0.11           | 1.17×    | 45.70 M lookups/s | 0.69% |
| Actix Web           | 33.60 ±0.09           | 1.79×    | 29.76 M lookups/s | 0.38% |

Searches Host, Sec-Fetch-Site, Cookie, Sec-Fetch-Mode, Connection and Sec-CH-UA-Platform, in that order, repeated with equal frequency.


#### Lookup Miss

| Implementation      | Mean ± SD (ns/lookup) | Relative | Throughput        | RCIW  |
| ------------------- | --------------------- | -------- | ----------------- | ----- |
| hwire + hwire_table | 10.47 ±0.03           | 1.00×    | 95.51 M lookups/s | 0.36% |
| nginx               | 27.59 ±0.69           | 2.64×    | 36.25 M lookups/s | 1.86% |
| H2O                 | 32.71 ±0.17           | 3.12×    | 30.57 M lookups/s | 0.72% |
| Actix Web           | 44.21 ±0.15           | 4.22×    | 22.62 M lookups/s | 0.49% |

Searches Hots, Accpet, Cooxie, User-Agend, Sec-CH-UA-Platforn and Referef, in that order, repeated with equal frequency.


### First Header Lookup Cost and Break-even

Estimate the total time to parse and store a request and perform its first header lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher parsing and storage cost.

Totals use the displayed means: `Parse + Post-process mean + Q × lookup mean`. Lookup costs are measured on a completed warm context; the first-lookup total is estimated, not timed immediately after parsing. The crossover is the first integer Q that beats the fastest Parse + Post-process + 1 lookup implementation.


#### Parse + Post-process + Lookup Hit — Known Headers (calculated)

| Implementation        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| --------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table † | 552.85                            | 1.00×    | 18.67 ±1.80               | Baseline                 |
| H2O                   | 833.80                            | 1.51×    | 16.02 ±0.43               | 108                      |
| nginx †               | 1228.85                           | 2.22×    | 18.47 ±6.58               | 3382                     |
| Actix Web             | 1382.25                           | 2.50×    | 19.39 ±0.14               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table. Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


#### Parse + Post-process + Lookup Hit — Unknown Headers (calculated)

| Implementation      | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table | 553.89                            | 1.00×    | 19.71 ±0.18               | Baseline                 |
| H2O                 | 843.96                            | 1.52×    | 26.18 ±0.17               | No crossover             |
| nginx †             | 1253.26                           | 2.26×    | 42.88 ±18.09              | No crossover             |
| Actix Web           | 1413.73                           | 2.55×    | 50.87 ±0.14               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table. Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


#### Parse + Post-process + Lookup Hit — Mixed Headers (calculated)

| Implementation      | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table | 552.95                            | 1.00×    | 18.77 ±0.07               | Baseline                 |
| H2O                 | 838.57                            | 1.52×    | 20.79 ±0.15               | No crossover             |
| nginx               | 1232.26                           | 2.23×    | 21.88 ±0.11               | No crossover             |
| Actix Web           | 1396.46                           | 2.53×    | 33.60 ±0.09               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table. Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


#### Parse + Post-process + Lookup Miss (calculated)

| Implementation      | Parse + Post-process + 1 Miss (ns) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------- | ---------------------------------- | -------- | -------------------------- | ------------------------ |
| hwire + hwire_table | 544.65                             | 1.00×    | 10.47 ±0.03                | Baseline                 |
| H2O                 | 850.49                             | 1.56×    | 32.71 ±0.17                | No crossover             |
| nginx               | 1237.97                            | 2.27×    | 27.59 ±0.69                | No crossover             |
| Actix Web           | 1407.07                            | 2.58×    | 44.21 ±0.15                | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table. Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.
