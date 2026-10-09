# Benchmarking Production Usage

Measure request parsing, native header storage and string lookup. Input
preparation, system allocation and cleanup are excluded under the
preallocated-memory policy below.


## Workload

The production workload compares HTTP request parsing, native header storage
and header lookup with memory preallocated. It excludes routing, header-value
validation, body processing and query decomposition. Inputs are a browser GET
request forwarded through a CDN and a 4,201-byte
variant with a synthetic JWT-shaped Authorization value and a large session
Cookie. Both contain fewer than 32 headers; artificial header-growth inputs
are excluded.

The large input is measured complete and in two calls, split after 50% or 90%
of its bytes. Each split operation measures all parser attempts and final
storage construction together. This is a supplemental comparison of retry
behavior; it does not model TCP segment boundaries, arrival delays or the
frequency of incomplete reads in a server.

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

An untimed parse warms up lazy runtime and thread-local initialization before
the request arena is activated. Per-request context initialization remains timed.

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

Each implementation owns its `setup.sh`, `fetch.sh`, `config.mk` and dependency directory.

- Optional `setup.sh check|install` prepares system dependencies before `fetch.sh`.
  `check` reports missing dependencies without installing; `install` is invoked
  only with `INSTALL_DEPS=1`. Return nonzero on failure.
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
| `<name>_VARIANTS` | Supported variants; defaults to the requested variants when omitted |
| `<name>_NAME` | Report display name; defaults to the directory name |
| `<name>_BUILD_INFO` | Native toolchain/build description recorded in the measurement metadata |
| `<name>_BUILD_DEPS` | Native source/manifests that trigger rebuilding |
| `<name>_ENV` | Space-separated `NAME=value` assignments (shell quoting supported) for this adapter’s setup, fetch, build and execution |

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


### Optional split-input parsing

An adapter can register the entry point below. The same buffer is exposed
through `split_at` bytes first, then through `len` bytes. Use native continuation
or retry behavior, retaining completed storage where native APIs permit it.
Complete and split entry points contain their own parsing flow; avoid a shared
function that selects the input mode at runtime. Do not introduce custom
end-of-headers scans or omit repeated callbacks.

```c
/**
 * Initialize request state, process a partial prefix and finish with full input.
 * Called once per timed split operation; both attempts and final native storage
 * conversion are included. Cleanup follows the normal context contract.
 * @param split_at Prefix length, strictly between zero and len. The fixture
 *                 guarantees that the first call needs more input.
 * @return Zero after completed parsing; nonzero on native processing failure.
 */
int name_request_with_store_split(void **context, const unsigned char *data,
                                  size_t len, size_t header_capacity,
                                  size_t split_at);
```

Targets without this optional entry point contribute only complete-input results.


### Optional reusable queries

Adapters may add all three entry points below. Queries must contain only
request-independent information, own any copied name bytes, and remain reusable
across requests and threads while immutable. Query preparation cannot inspect a
request context. Entry pointers, matching positions and cached results are not
permitted. Targets without these entry points retain string-only measurements.

```c
/**
 * Prepare one immutable header-name query before lookup timing and before the
 * request arena is activated. Return an owned query or NULL on failure.
 * The query must remain usable with independently parsed request contexts.
 * Copy name bytes if retained; the caller may release the original key.
 */
void *name_header_query_new(const char *key, size_t len);

/**
 * Search the supplied completed request using a previously prepared query.
 * Called repeatedly inside lookup timing; include native search and access.
 * Do not cache request-specific entries or modify the shared query.
 * Return value length + 1 on hit and zero on miss.
 */
size_t name_header_lookup_prepared(const void *context, const void *query);

/**
 * Release an owned query after lookup timing and after the request arena ends.
 * Queries are independent of request/context lifetime. Accept NULL.
 */
void name_header_query_free(void *query);
```


# Benchmark

<!-- benchmark-environment -->
## Environment

```text
date                 : 2026-10-10T08:31:25+09:00
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
- C11 compiler and linker: Compile the driver, C adapters and arena helpers.
- Cargo and rustc 1.88+: Required when Rust adapters are enabled.
- `curl`, `tar` and network access: Fetch pinned dependencies during initial setup.
- OpenSSL development headers: Required when the H2O target is enabled (for example, `libssl-dev` on Debian/Ubuntu). `pkg-config` discovers nonstandard include paths; otherwise add the include path to `CFLAGS`.

- libuv development headers: Required for the H2O target on Linux and macOS (`libuv1-dev` on Debian/Ubuntu); include paths are discovered with `pkg-config`.

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

The default run measures scalar builds and each target's supported SIMD/native
builds. `nosimd` disables explicit parser SIMD and compiler loop vectorization;
`native` uses `-march=native` on x86-64 and `-mcpu=native` on ARM.
Use `VARIANTS=...` to select builds. Each adapter's `config.mk` declares its
supported variants; unsupported combinations are excluded. Target-specific
flags are listed below.

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
- Compiler: <!-- compiler:compiler -->`gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags -->
  - `nosimd`: `-DHWIRE_NO_SIMD -DHWIRE_NO_AES -fno-tree-vectorize`; scalar parser and SipHash-1-3.
  - `sse2`: Default x86-64 target; SSE2 parser and SipHash-1-3.
  - `sse42`: `-msse4.2`; SSE4.2 parser and SipHash-1-3.
  - `neon` (ARM): Default compiler target; NEON parser and the available hash backend.
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM; AES hash when enabled by the target.
  - `siphash`: Native flags plus `-DHWIRE_NO_AES`; selects the SipHash-1-3 backend.
- Build: `C`; scalar, supported SIMD and native CPU targets

The adapter acquires `app_request_t` and a 32-entry table together, with the parser
context on the stack. The table uses an 8N index and a deterministic key derived
from seed 42. `make VARIANTS=siphash` adds `-DHWIRE_NO_AES` to select the fallback.
String lookup calls `hwire_table_get_ci` directly. The adapter uses ordinary
case-insensitive table storage without reserved-name or prepared-query support.

</details>


### nginx

Native request structures retain parsed headers in pool-backed list blocks,
with dedicated references for known header names.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](nginx/request.c)
- Library: [nginx 1.31.6](https://github.com/nginx/nginx/tree/release-1.31.6), revision `45a318d05a0fd23f57ffe9579f7f0969c0fe402a`
- Compiler: <!-- compiler:compiler -->`gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags --> `-ffunction-sections -fdata-sections`
  - `nosimd`: `-fno-tree-vectorize`; no explicit parser SIMD path.
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
- Build: `C`; scalar and native CPU targets; `--with-compat`

The adapter initializes the full request structure, a 4096-byte pool, incoming
and outgoing header lists and trailers. List blocks hold 20 headers; the
21- and 22-field inputs add a second block. The build enables 31 known-name definitions.
Metadata retains upstream feature guards; value-validation handlers and routing
state are excluded.

Prepared lookup stores the native known-header field offset resolved at
preparation time. nginx has no general prepared-query object; the adapter owns
an immutable selector containing that offset and a lowercase name. It accesses
the supplied request's dedicated field for known names and scans native list
blocks for unknown names. No request or matching-entry pointer is cached.

</details>


### H2O

picohttpparser produces stack header slices, which are converted into the native
H2O request/header representation.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](h2o/request.c)
- Library: [H2O](https://github.com/h2o/h2o/tree/5da50541a4b6a038c9cea493f740747cf964f4a8), revision `5da50541a4b6a038c9cea493f740747cf964f4a8`; uses the bundled picohttpparser
- Compiler: <!-- compiler:compiler -->`gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags --> `-D_GNU_SOURCE -ffunction-sections -fdata-sections`
  - `nosimd`: `-fno-tree-vectorize`; baseline picohttpparser without SSE4.2.
  - `sse42` (x86-64): `-msse4.2`; enables picohttpparser's SSE4.2 path.
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
- Build: `C`; scalar, SSE4.2 and native CPU targets; OpenSSL development headers required

The adapter uses `h2o_req_t`, `h2o_init_request` and unmodified header, token,
string and memory functions. It reproduces HTTP/1 header population from a
100-field stack array, preserving original names and retaining method, path,
version and authority. Host is stored as authority. The registry defines 79
normal-header tokens and 6 pseudo-header tokens. `H2O_MAX_TOKENS` limits the
token-definition array, not the number of stored headers. TLS processing is
outside the workload.

Prepared lookup retains H2O's shared static token when available, otherwise
an owned lowercase name. Token lookup still scans the request's header array
using token identity; Host uses the native authority field. Unknown names use
native string search. Neither representation retains a request or result.

</details>


### Actix Web

httparse produces stack header slices, which are converted into native Request,
RequestHead and HeaderMap objects.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](actix_web/request.c), [parser.rs](actix_web/parser.rs)
- Library: [Actix Web](https://github.com/actix/actix-web/tree/714572c5eca7a012b771dd9a978767e7573697a8), revision `714572c5eca7a012b771dd9a978767e7573697a8`; actix-http 3.18.12; httparse 1.10.1 fetched by Cargo and pinned in `Cargo.lock`
- Compiler: <!-- compiler:actix_web -->`rustc 1.93.1 (01f6ddf75 2026-02-11)`<!-- /compiler -->; C ABI shim: <!-- compiler:compiler -->`gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags --> (C ABI shim)
  - `native`: `-march=native` on x86-64; `-mcpu=native` on ARM.
- RUSTFLAGS:
  - `nosimd`: `-C no-vectorize-loops -C no-vectorize-slp`; `HTTPARSE_DISABLE_SIMD=1` disables httparse SIMD.
  - `sse42`: `-C target-feature=+sse4.2,-avx2`; enables the SSE4.2 parser path.
  - `neon` (ARM): `-C target-feature=+neon`.
  - `native`: `-C target-cpu=native`; enables supported CPU paths, including AVX2 on this machine.
- Build: <!-- build:actix_web -->opt-level=2; panic=abort; scalar for nosimd; SSE4.2 for sse42; NEON for neon; native CPU including AVX2<!-- /build -->

The adapter uses a 96-field stack array and native HeaderMap reservation for
16 headers. It reproduces the decoder's private HeaderIndex span-recording step,
then converts and retains method, URI, version and headers. Borrowed input has
shared Bytes ownership metadata for native HeaderValue construction. The
`http` 0.2.12 registry defines 81 standard header names.

The thread-local request-head pool is initialized outside timing and kept empty
during operations. Completed arena-backed heads are reclaimed by arena reset,
instead of being returned to that pool. Measurements reconstruct native objects
rather than retaining container capacity between requests.

Prepared lookup owns a native `HeaderName` and passes a borrowed reference to
`HeaderMap::get`. It skips string-to-name conversion while native map hashing
and lookup remain timed. Immutable `HeaderName` values are reusable across
requests and threads; no matched entry is cached.

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
post-processing step. Lookup is measured separately on the completed context.
The Lookup phase above describes the string-search path.

These flows cover header processing; server routing, body processing and
header-value validation handlers are excluded.


### String and Prepared Lookup Comparison

String lookup starts from the original header-name string and includes native
name conversion and search. Prepared lookup uses immutable queries reusable
across requests and threads; query preparation and destruction are outside
timing. Native search and access to the current request remain timed.

Rows marked `(string)` and `(prepared)` use the same names and parsing/storage
path. Relative uses the fastest lookup in each table as its baseline.

Known/unknown describes native header-name definitions. Unknown hit names are
present in the message but absent from those definitions; hwire_table treats
all names as strings. Key lengths differ between groups, so costs include both
name representation and key length. Lookup time is the mean per search on a
completed warm context.

<!-- production-results -->

## Browser GET via CDN

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

Supported CPU builds; memory preallocated (system allocation excluded); application header limit 128; headers only.

Parse + Post-process includes initialization, HTTP parsing and native header storage, including any growth. Query decomposition, decoding and storage are excluded. Input preparation and context cleanup are outside timing.

† Target RCIW was not reached; calculated totals inherit the marker from either component.


### Parse + Post-process — Complete Input

| Implementation               | Mean ± SD (ns/request) | Relative | Throughput   | RCIW  |
| ---------------------------- | ---------------------- | -------- | ------------ | ----- |
| hwire + hwire_table (native) | 483.18 ±4.13           | 1.00×    | 2.07 M req/s | 1.19% |
| hwire + hwire_table (SSE4.2) | 637.13 ±6.73           | 1.32×    | 1.57 M req/s | 1.48% |
| hwire + hwire_table (SSE2)   | 740.58 ±5.91           | 1.53×    | 1.35 M req/s | 1.12% |
| H2O (native)                 | 750.66 ±6.67           | 1.55×    | 1.33 M req/s | 1.24% |
| H2O (SSE4.2)                 | 834.28 ±15.45          | 1.73×    | 1.20 M req/s | 1.72% |
| hwire + hwire_table (scalar) | 888.99 ±3.17           | 1.84×    | 1.12 M req/s | 0.50% |
| H2O (scalar)                 | 1019.38 ±13.58         | 2.11×    | 0.98 M req/s | 1.86% |
| nginx (scalar)               | 1138.18 ±10.21         | 2.36×    | 0.88 M req/s | 1.25% |
| nginx (native)               | 1211.02 ±13.40         | 2.51×    | 0.83 M req/s | 1.55% |
| Actix Web (native)           | 1403.19 ±11.80         | 2.90×    | 0.71 M req/s | 1.18% |
| Actix Web (scalar)           | 1478.04 ±15.24         | 3.06×    | 0.68 M req/s | 1.44% |
| Actix Web (SSE4.2)           | 1527.85 ±20.73         | 3.16×    | 0.65 M req/s | 1.90% |


### Lookup Hit — Known Headers

| Implementation                        | Mean ± SD (ns/lookup) | Relative | Throughput         | RCIW  |
| ------------------------------------- | --------------------- | -------- | ------------------ | ----- |
| nginx (scalar) (prepared)             | 2.61 ±0.02            | 1.00×    | 383.14 M lookups/s | 1.07% |
| nginx (native) (prepared)             | 2.63 ±0.04            | 1.01×    | 380.23 M lookups/s | 1.66% |
| H2O (native) (prepared)               | 4.33 ±0.12            | 1.66×    | 230.95 M lookups/s | 1.84% |
| H2O (scalar) (prepared)               | 4.96 ±0.27 †          | 1.90×    | 201.61 M lookups/s | 3.04% |
| H2O (SSE4.2) (prepared)               | 6.05 ±0.17            | 2.32×    | 165.29 M lookups/s | 1.78% |
| Actix Web (native) (prepared)         | 6.42 ±0.03            | 2.46×    | 155.76 M lookups/s | 0.74% |
| Actix Web (scalar) (prepared)         | 6.48 ±0.03            | 2.48×    | 154.32 M lookups/s | 0.65% |
| Actix Web (SSE4.2) (prepared)         | 6.48 ±0.05            | 2.48×    | 154.32 M lookups/s | 1.04% |
| H2O (native) (string)                 | 14.99 ±0.59 †         | 5.74×    | 66.71 M lookups/s  | 2.23% |
| nginx (scalar) (string)               | 15.46 ±0.14           | 5.92×    | 64.68 M lookups/s  | 1.30% |
| nginx (native) (string)               | 15.53 ±0.16           | 5.95×    | 64.39 M lookups/s  | 1.42% |
| hwire + hwire_table (native) (string) | 16.82 ±0.16           | 6.44×    | 59.45 M lookups/s  | 1.36% |
| H2O (SSE4.2) (string)                 | 17.42 ±0.89 †         | 6.67×    | 57.41 M lookups/s  | 2.88% |
| H2O (scalar) (string)                 | 17.49 ±0.40           | 6.70×    | 57.18 M lookups/s  | 1.87% |
| Actix Web (native) (string)           | 20.32 ±0.14           | 7.79×    | 49.21 M lookups/s  | 0.93% |
| Actix Web (scalar) (string)           | 21.20 ±0.07           | 8.12×    | 47.17 M lookups/s  | 0.45% |
| Actix Web (SSE4.2) (string)           | 21.27 ±0.13           | 8.15×    | 47.01 M lookups/s  | 0.85% |
| hwire + hwire_table (SSE4.2) (string) | 21.34 ±0.18           | 8.18×    | 46.86 M lookups/s  | 1.18% |
| hwire + hwire_table (SSE2) (string)   | 21.42 ±0.13           | 8.21×    | 46.69 M lookups/s  | 0.84% |
| hwire + hwire_table (scalar) (string) | 21.96 ±0.18           | 8.41×    | 45.54 M lookups/s  | 1.16% |

Searches Host, Accept, Cookie, User-Agent, Connection and Referer, in that order, repeated with equal frequency.


### Lookup Hit — Unknown Headers

| Implementation                        | Mean ± SD (ns/lookup) | Relative | Throughput         | RCIW  |
| ------------------------------------- | --------------------- | -------- | ------------------ | ----- |
| Actix Web (native) (prepared)         | 9.83 ±0.14            | 1.00×    | 101.73 M lookups/s | 1.53% |
| Actix Web (scalar) (prepared)         | 9.86 ±0.05            | 1.00×    | 101.42 M lookups/s | 0.77% |
| Actix Web (SSE4.2) (prepared)         | 9.87 ±0.05            | 1.00×    | 101.32 M lookups/s | 0.69% |
| nginx (scalar) (prepared)             | 12.05 ±0.15           | 1.23×    | 82.99 M lookups/s  | 1.79% |
| nginx (native) (prepared)             | 12.89 ±0.10           | 1.31×    | 77.58 M lookups/s  | 1.05% |
| H2O (native) (prepared)               | 12.98 ±0.25           | 1.32×    | 77.04 M lookups/s  | 1.79% |
| H2O (SSE4.2) (prepared)               | 13.28 ±0.18           | 1.35×    | 75.30 M lookups/s  | 1.48% |
| H2O (scalar) (prepared)               | 13.31 ±0.34           | 1.35×    | 75.13 M lookups/s  | 1.90% |
| hwire + hwire_table (native) (string) | 19.51 ±0.13           | 1.98×    | 51.26 M lookups/s  | 0.96% |
| H2O (SSE4.2) (string)                 | 25.31 ±0.24           | 2.57×    | 39.51 M lookups/s  | 1.32% |
| hwire + hwire_table (SSE2) (string)   | 25.82 ±0.14           | 2.63×    | 38.73 M lookups/s  | 0.76% |
| hwire + hwire_table (SSE4.2) (string) | 25.83 ±0.16           | 2.63×    | 38.71 M lookups/s  | 0.88% |
| hwire + hwire_table (scalar) (string) | 26.57 ±0.26           | 2.70×    | 37.64 M lookups/s  | 1.37% |
| H2O (native) (string)                 | 27.74 ±0.28           | 2.82×    | 36.05 M lookups/s  | 1.40% |
| nginx (scalar) (string)               | 28.21 ±0.21           | 2.87×    | 35.45 M lookups/s  | 1.03% |
| nginx (native) (string)               | 28.25 ±0.21           | 2.87×    | 35.40 M lookups/s  | 1.03% |
| H2O (scalar) (string)                 | 29.99 ±0.42           | 3.05×    | 33.34 M lookups/s  | 1.53% |
| Actix Web (native) (string)           | 52.09 ±0.24           | 5.30×    | 19.20 M lookups/s  | 0.65% |
| Actix Web (scalar) (string)           | 52.96 ±0.39           | 5.39×    | 18.88 M lookups/s  | 1.03% |
| Actix Web (SSE4.2) (string)           | 53.41 ±0.44           | 5.43×    | 18.72 M lookups/s  | 1.15% |

Searches Sec-Fetch-Site, Sec-Fetch-Mode, Sec-Fetch-User, Sec-Fetch-Dest, Sec-CH-UA and Sec-CH-UA-Platform, in that order, repeated with equal frequency.


### Lookup Hit — Mixed Headers

| Implementation                        | Mean ± SD (ns/lookup) | Relative | Throughput         | RCIW  |
| ------------------------------------- | --------------------- | -------- | ------------------ | ----- |
| nginx (scalar) (prepared)             | 5.21 ±0.04            | 1.00×    | 191.94 M lookups/s | 1.10% |
| nginx (native) (prepared)             | 6.44 ±0.06            | 1.24×    | 155.28 M lookups/s | 1.18% |
| H2O (native) (prepared)               | 7.15 ±0.20            | 1.37×    | 139.86 M lookups/s | 1.87% |
| H2O (scalar) (prepared)               | 7.80 ±0.14            | 1.50×    | 128.21 M lookups/s | 1.90% |
| H2O (SSE4.2) (prepared)               | 7.82 ±0.21            | 1.50×    | 127.88 M lookups/s | 1.99% |
| Actix Web (scalar) (prepared)         | 8.34 ±0.04            | 1.60×    | 119.90 M lookups/s | 0.72% |
| Actix Web (native) (prepared)         | 8.42 ±0.04            | 1.62×    | 118.76 M lookups/s | 0.71% |
| Actix Web (SSE4.2) (prepared)         | 9.05 ±0.10            | 1.74×    | 110.50 M lookups/s | 1.51% |
| hwire + hwire_table (native) (string) | 18.09 ±0.10           | 3.47×    | 55.28 M lookups/s  | 0.76% |
| H2O (SSE4.2) (string)                 | 20.23 ±0.26           | 3.88×    | 49.43 M lookups/s  | 1.80% |
| nginx (native) (string)               | 21.77 ±0.18           | 4.18×    | 45.93 M lookups/s  | 1.13% |
| nginx (scalar) (string)               | 21.85 ±0.21           | 4.19×    | 45.77 M lookups/s  | 1.38% |
| H2O (scalar) (string)                 | 23.64 ±0.41           | 4.54×    | 42.30 M lookups/s  | 1.63% |
| hwire + hwire_table (SSE2) (string)   | 23.73 ±0.17           | 4.55×    | 42.14 M lookups/s  | 1.01% |
| hwire + hwire_table (SSE4.2) (string) | 23.75 ±0.25           | 4.56×    | 42.11 M lookups/s  | 1.46% |
| H2O (native) (string)                 | 24.08 ±0.16           | 4.62×    | 41.53 M lookups/s  | 0.94% |
| hwire + hwire_table (scalar) (string) | 24.41 ±0.40           | 4.69×    | 40.97 M lookups/s  | 1.81% |
| Actix Web (scalar) (string)           | 35.39 ±0.24           | 6.79×    | 28.26 M lookups/s  | 0.97% |
| Actix Web (SSE4.2) (string)           | 35.41 ±0.28           | 6.80×    | 28.24 M lookups/s  | 1.10% |
| Actix Web (native) (string)           | 35.81 ±0.29           | 6.87×    | 27.93 M lookups/s  | 1.15% |

Searches Host, Sec-Fetch-Site, Cookie, Sec-Fetch-Mode, Connection and Sec-CH-UA-Platform, in that order, repeated with equal frequency.


### Lookup Miss

| Implementation                        | Mean ± SD (ns/lookup) | Relative | Throughput         | RCIW  |
| ------------------------------------- | --------------------- | -------- | ------------------ | ----- |
| Actix Web (SSE4.2) (prepared)         | 4.92 ±0.10            | 1.00×    | 203.25 M lookups/s | 1.64% |
| Actix Web (native) (prepared)         | 5.04 ±0.04            | 1.02×    | 198.41 M lookups/s | 0.99% |
| Actix Web (scalar) (prepared)         | 5.61 ±0.33 †          | 1.14×    | 178.25 M lookups/s | 3.33% |
| hwire + hwire_table (native) (string) | 7.68 ±0.03            | 1.56×    | 130.21 M lookups/s | 0.52% |
| hwire + hwire_table (SSE2) (string)   | 14.20 ±0.14           | 2.89×    | 70.42 M lookups/s  | 1.33% |
| hwire + hwire_table (SSE4.2) (string) | 14.39 ±0.26           | 2.92×    | 69.49 M lookups/s  | 1.68% |
| H2O (SSE4.2) (prepared)               | 14.75 ±0.95 †         | 3.00×    | 67.80 M lookups/s  | 3.66% |
| hwire + hwire_table (scalar) (string) | 15.31 ±0.18           | 3.11×    | 65.32 M lookups/s  | 1.64% |
| H2O (scalar) (prepared)               | 15.43 ±0.21           | 3.14×    | 64.81 M lookups/s  | 1.49% |
| H2O (native) (prepared)               | 15.49 ±0.18           | 3.15×    | 64.56 M lookups/s  | 1.64% |
| nginx (scalar) (prepared)             | 18.08 ±0.67 †         | 3.67×    | 55.31 M lookups/s  | 2.09% |
| nginx (native) (prepared)             | 21.47 ±0.38           | 4.36×    | 46.58 M lookups/s  | 1.93% |
| nginx (native) (string)               | 27.11 ±0.21           | 5.51×    | 36.89 M lookups/s  | 1.07% |
| H2O (SSE4.2) (string)                 | 28.65 ±0.22           | 5.82×    | 34.90 M lookups/s  | 1.09% |
| H2O (scalar) (string)                 | 31.74 ±0.28           | 6.45×    | 31.51 M lookups/s  | 1.23% |
| nginx (scalar) (string)               | 31.87 ±0.32           | 6.48×    | 31.38 M lookups/s  | 1.41% |
| H2O (native) (string)                 | 34.16 ±0.21           | 6.94×    | 29.27 M lookups/s  | 0.88% |
| Actix Web (SSE4.2) (string)           | 43.13 ±1.12           | 8.77×    | 23.19 M lookups/s  | 1.78% |
| Actix Web (native) (string)           | 44.74 ±0.30           | 9.09×    | 22.35 M lookups/s  | 0.93% |
| Actix Web (scalar) (string)           | 44.85 ±0.61           | 9.12×    | 22.30 M lookups/s  | 1.50% |

Searches Hots, Accpet, Cooxie, User-Agend, Sec-CH-UA-Platforn and Referef, in that order, repeated with equal frequency.


### First Header Lookup Cost and Break-even — Complete input

Estimate the total time to parse and store a request and perform its first header lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher parsing and storage cost.

Totals use the displayed means: `Parse + Post-process mean + Q × lookup mean`. Lookup costs are measured on a completed warm context; the first-lookup total is estimated, not timed immediately after parsing. The crossover is the first integer Q that beats the fastest Parse + Post-process + 1 lookup implementation.


### Parse + Post-process + Lookup Hit — Known Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 500.00                            | 1.00×    | 16.82 ±0.16               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 658.47                            | 1.32×    | 21.34 ±0.18               | No crossover             |
| H2O (native) (prepared)               | 754.99                            | 1.51×    | 4.33 ±0.12                | 22                       |
| hwire + hwire_table (SSE2) (string)   | 762.00                            | 1.52×    | 21.42 ±0.13               | No crossover             |
| H2O (native) (string) †               | 765.65                            | 1.53×    | 14.99 ±0.59               | 147                      |
| H2O (SSE4.2) (prepared)               | 840.33                            | 1.68×    | 6.05 ±0.17                | 33                       |
| H2O (SSE4.2) (string) †               | 851.70                            | 1.70×    | 17.42 ±0.89               | No crossover             |
| hwire + hwire_table (scalar) (string) | 910.95                            | 1.82×    | 21.96 ±0.18               | No crossover             |
| H2O (scalar) (prepared) †             | 1024.34                           | 2.05×    | 4.96 ±0.27                | 46                       |
| H2O (scalar) (string)                 | 1036.87                           | 2.07×    | 17.49 ±0.40               | No crossover             |
| nginx (scalar) (prepared)             | 1140.79                           | 2.28×    | 2.61 ±0.02                | 47                       |
| nginx (scalar) (string)               | 1153.64                           | 2.31×    | 15.46 ±0.14               | 482                      |
| nginx (native) (prepared)             | 1213.65                           | 2.43×    | 2.63 ±0.04                | 52                       |
| nginx (native) (string)               | 1226.55                           | 2.45×    | 15.53 ±0.16               | 565                      |
| Actix Web (native) (prepared)         | 1409.61                           | 2.82×    | 6.42 ±0.03                | 89                       |
| Actix Web (native) (string)           | 1423.51                           | 2.85×    | 20.32 ±0.14               | No crossover             |
| Actix Web (scalar) (prepared)         | 1484.52                           | 2.97×    | 6.48 ±0.03                | 97                       |
| Actix Web (scalar) (string)           | 1499.24                           | 3.00×    | 21.20 ±0.07               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 1534.33                           | 3.07×    | 6.48 ±0.05                | 102                      |
| Actix Web (SSE4.2) (string)           | 1549.12                           | 3.10×    | 21.27 ±0.13               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### Parse + Post-process + Lookup Hit — Unknown Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 502.69                            | 1.00×    | 19.51 ±0.13               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 662.96                            | 1.32×    | 25.83 ±0.16               | No crossover             |
| H2O (native) (prepared)               | 763.64                            | 1.52×    | 12.98 ±0.25               | 41                       |
| hwire + hwire_table (SSE2) (string)   | 766.40                            | 1.52×    | 25.82 ±0.14               | No crossover             |
| H2O (native) (string)                 | 778.40                            | 1.55×    | 27.74 ±0.28               | No crossover             |
| H2O (SSE4.2) (prepared)               | 847.56                            | 1.69×    | 13.28 ±0.18               | 57                       |
| H2O (SSE4.2) (string)                 | 859.59                            | 1.71×    | 25.31 ±0.24               | No crossover             |
| hwire + hwire_table (scalar) (string) | 915.56                            | 1.82×    | 26.57 ±0.26               | No crossover             |
| H2O (scalar) (prepared)               | 1032.69                           | 2.05×    | 13.31 ±0.34               | 87                       |
| H2O (scalar) (string)                 | 1049.37                           | 2.09×    | 29.99 ±0.42               | No crossover             |
| nginx (scalar) (prepared)             | 1150.23                           | 2.29×    | 12.05 ±0.15               | 88                       |
| nginx (scalar) (string)               | 1166.39                           | 2.32×    | 28.21 ±0.21               | No crossover             |
| nginx (native) (prepared)             | 1223.91                           | 2.43×    | 12.89 ±0.10               | 110                      |
| nginx (native) (string)               | 1239.27                           | 2.47×    | 28.25 ±0.21               | No crossover             |
| Actix Web (native) (prepared)         | 1413.02                           | 2.81×    | 9.83 ±0.14                | 96                       |
| Actix Web (native) (string)           | 1455.28                           | 2.89×    | 52.09 ±0.24               | No crossover             |
| Actix Web (scalar) (prepared)         | 1487.90                           | 2.96×    | 9.86 ±0.05                | 104                      |
| Actix Web (scalar) (string)           | 1531.00                           | 3.05×    | 52.96 ±0.39               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 1537.72                           | 3.06×    | 9.87 ±0.05                | 109                      |
| Actix Web (SSE4.2) (string)           | 1581.26                           | 3.15×    | 53.41 ±0.44               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### Parse + Post-process + Lookup Hit — Mixed Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 501.27                            | 1.00×    | 18.09 ±0.10               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 660.88                            | 1.32×    | 23.75 ±0.25               | No crossover             |
| H2O (native) (prepared)               | 757.81                            | 1.51×    | 7.15 ±0.20                | 25                       |
| hwire + hwire_table (SSE2) (string)   | 764.31                            | 1.52×    | 23.73 ±0.17               | No crossover             |
| H2O (native) (string)                 | 774.74                            | 1.55×    | 24.08 ±0.16               | No crossover             |
| H2O (SSE4.2) (prepared)               | 842.10                            | 1.68×    | 7.82 ±0.21                | 35                       |
| H2O (SSE4.2) (string)                 | 854.51                            | 1.70×    | 20.23 ±0.26               | No crossover             |
| hwire + hwire_table (scalar) (string) | 913.40                            | 1.82×    | 24.41 ±0.40               | No crossover             |
| H2O (scalar) (prepared)               | 1027.18                           | 2.05×    | 7.80 ±0.14                | 53                       |
| H2O (scalar) (string)                 | 1043.02                           | 2.08×    | 23.64 ±0.41               | No crossover             |
| nginx (scalar) (prepared)             | 1143.39                           | 2.28×    | 5.21 ±0.04                | 51                       |
| nginx (scalar) (string)               | 1160.03                           | 2.31×    | 21.85 ±0.21               | No crossover             |
| nginx (native) (prepared)             | 1217.46                           | 2.43×    | 6.44 ±0.06                | 63                       |
| nginx (native) (string)               | 1232.79                           | 2.46×    | 21.77 ±0.18               | No crossover             |
| Actix Web (native) (prepared)         | 1411.61                           | 2.82×    | 8.42 ±0.04                | 96                       |
| Actix Web (native) (string)           | 1439.00                           | 2.87×    | 35.81 ±0.29               | No crossover             |
| Actix Web (scalar) (prepared)         | 1486.38                           | 2.97×    | 8.34 ±0.04                | 103                      |
| Actix Web (scalar) (string)           | 1513.43                           | 3.02×    | 35.39 ±0.24               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 1536.90                           | 3.07×    | 9.05 ±0.10                | 116                      |
| Actix Web (SSE4.2) (string)           | 1563.26                           | 3.12×    | 35.41 ±0.28               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### Parse + Post-process + Lookup Miss (calculated)

| Implementation                        | Parse + Post-process + 1 Miss (ns) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | ---------------------------------- | -------- | -------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 490.86                             | 1.00×    | 7.68 ±0.03                 | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 651.52                             | 1.33×    | 14.39 ±0.26                | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 754.78                             | 1.54×    | 14.20 ±0.14                | No crossover             |
| H2O (native) (prepared)               | 766.15                             | 1.56×    | 15.49 ±0.18                | No crossover             |
| H2O (native) (string)                 | 784.82                             | 1.60×    | 34.16 ±0.21                | No crossover             |
| H2O (SSE4.2) (prepared) †             | 849.03                             | 1.73×    | 14.75 ±0.95                | No crossover             |
| H2O (SSE4.2) (string)                 | 862.93                             | 1.76×    | 28.65 ±0.22                | No crossover             |
| hwire + hwire_table (scalar) (string) | 904.30                             | 1.84×    | 15.31 ±0.18                | No crossover             |
| H2O (scalar) (prepared)               | 1034.81                            | 2.11×    | 15.43 ±0.21                | No crossover             |
| H2O (scalar) (string)                 | 1051.12                            | 2.14×    | 31.74 ±0.28                | No crossover             |
| nginx (scalar) (prepared) †           | 1156.26                            | 2.36×    | 18.08 ±0.67                | No crossover             |
| nginx (scalar) (string)               | 1170.05                            | 2.38×    | 31.87 ±0.32                | No crossover             |
| nginx (native) (prepared)             | 1232.49                            | 2.51×    | 21.47 ±0.38                | No crossover             |
| nginx (native) (string)               | 1238.13                            | 2.52×    | 27.11 ±0.21                | No crossover             |
| Actix Web (native) (prepared)         | 1408.23                            | 2.87×    | 5.04 ±0.04                 | 349                      |
| Actix Web (native) (string)           | 1447.93                            | 2.95×    | 44.74 ±0.30                | No crossover             |
| Actix Web (scalar) (prepared) †       | 1483.65                            | 3.02×    | 5.61 ±0.33                 | 481                      |
| Actix Web (scalar) (string)           | 1522.89                            | 3.10×    | 44.85 ±0.61                | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 1532.77                            | 3.12×    | 4.92 ±0.10                 | 379                      |
| Actix Web (SSE4.2) (string)           | 1570.98                            | 3.20×    | 43.13 ±1.12                | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


---


## Browser GET with Authorization and Session Cookie

Synthetic browser/CDN request with a JWT-shaped Authorization value and a large session Cookie. Authentication and signature verification are not measured.

<details>
<summary>Message (4201 bytes)</summary>

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
Authorization: Bearer eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJodHRwczovL2xvZ2luLmV4YW1wbGUuY29tIiwic3ViIjoidXNlci0xMjMiLCJhdWQiOiJhcHAuZXhhbXBsZS5jb20iLCJleHAiOjIwMDAwMDAwMDAsInJvbGVzIjpbInVzZXIiXSwiY2xhaW1zIjoiYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhIn0.c3ludGhldGljLXNpZ25hdHVyZXN5bnRoZXRpYy1zaWduYXR1cmVzeW50aGV0aWMtc2lnbmF0dXJlc3ludGhldGljLXNpZ25hdHVyZXN5bnRoZXRpYy1zaWduYXR1cmVzeW50aGV0aWMtc2lnbmF0dXJlc3ludGhldGljLXNpZ25hdHVyZXN5bnRoZXRpYy1zaWduYXR1cmVzeW50aGV0aWMtc2lnbmF0dXJlc3ludGhldGljLXNpZ25hdHVyZXN5bnRoZXRpYy1zaWduYXR1cmVzeW50aGV0aWMtc2lnbmF0dXJlc3ludGhldGljLXNpZ25hdHVyZXN5bnRoZXRpYy1zaWduYXR1cmVzeW50aGV0aWMtc2lnbmF0dXJl
Cookie: session=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef; csrf=abcdef0123456789; locale=ja_JP
CF-Connecting-IP: 192.0.2.1
X-Forwarded-For: 192.0.2.1
X-Forwarded-Proto: https
Cf-Ray: 8a1234567890abcd-NRT

```

</details>

Supported CPU builds; memory preallocated (system allocation excluded); application header limit 128; headers only.

Parse + Post-process includes initialization, HTTP parsing and native header storage, including any growth. Query decomposition, decoding and storage are excluded. Input preparation and context cleanup are outside timing.

† Target RCIW was not reached; calculated totals inherit the marker from either component.


### Parse + Post-process — Complete Input

| Implementation               | Mean ± SD (ns/request) | Relative | Throughput   | RCIW  |
| ---------------------------- | ---------------------- | -------- | ------------ | ----- |
| hwire + hwire_table (native) | 744.61 ±17.10          | 1.00×    | 1.34 M req/s | 1.88% |
| hwire + hwire_table (SSE4.2) | 873.35 ±7.91           | 1.17×    | 1.15 M req/s | 1.27% |
| hwire + hwire_table (SSE2)   | 981.05 ±10.73          | 1.32×    | 1.02 M req/s | 1.53% |
| H2O (native)                 | 1010.44 ±7.99          | 1.36×    | 0.99 M req/s | 1.11% |
| H2O (SSE4.2)                 | 1103.99 ±21.86         | 1.48×    | 0.91 M req/s | 1.84% |
| Actix Web (native)           | 1884.76 ±14.63         | 2.53×    | 0.53 M req/s | 1.09% |
| H2O (scalar)                 | 1929.03 ±23.77         | 2.59×    | 0.52 M req/s | 1.72% |
| hwire + hwire_table (scalar) | 1945.85 ±10.29         | 2.61×    | 0.51 M req/s | 0.74% |
| Actix Web (scalar)           | 1982.31 ±18.48         | 2.66×    | 0.50 M req/s | 1.30% |
| Actix Web (SSE4.2)           | 2470.01 ±27.95         | 3.32×    | 0.40 M req/s | 1.58% |
| nginx (scalar)               | 2701.89 ±14.81         | 3.63×    | 0.37 M req/s | 0.77% |
| nginx (native)               | 2777.44 ±27.65         | 3.73×    | 0.36 M req/s | 1.39% |


### Incomplete-input handling

| Implementation | First call | Second call |
| --- | --- | --- |
| hwire + hwire_table | Parse the prefix and store completed headers through callbacks | Clear the partial table index and reparse accumulated input; callbacks run again |
| nginx | Retain parser state, buffer position and completed headers | Resume from the consumed position and append newly completed headers |
| H2O | Pass the prefix to picohttpparser; leave native header conversion until completion | Pass accumulated input and the previous length (`last_len`), then populate native storage once |
| Actix Web | Parse the prefix into temporary httparse state and stack headers | Recreate temporary parsing state, parse accumulated input and convert to native storage once |

Request initialization occurs once per message. Input copying, arena reset and
context cleanup remain outside timing; partial-index clearing and repeated
parsing are inside timing. The same complete-input baseline isolates the extra
cost of each retry strategy for this fixture. Authentication is not performed.


### Two-call Input Scenarios

The same 4201-byte request is exposed in two calls, first through byte 2100 (50%) or 3780 (90%), then through the end. Both attempts, required partial-storage reset and final post-processing are timed together. No arrival delay, socket I/O or receive-buffer copying is measured. Vs complete input compares each build with its own complete-input result. These controlled scenarios do not imply a real-world fragmentation frequency.


### Split at 50% — Parse + Post-process

| Implementation               | Mean ± SD (ns/request) | Relative | Vs complete input | RCIW  |
| ---------------------------- | ---------------------- | -------- | ----------------- | ----- |
| hwire + hwire_table (native) | 1244.32 ±6.30          | 1.00×    | 1.67×             | 0.71% |
| hwire + hwire_table (SSE4.2) | 1459.78 ±12.08         | 1.17×    | 1.67×             | 1.16% |
| hwire + hwire_table (SSE2)   | 1623.92 ±12.03         | 1.31×    | 1.66×             | 1.04% |
| Actix Web (native)           | 2345.45 ±17.45         | 1.88×    | 1.24×             | 1.04% |
| H2O (native)                 | 2363.77 ±12.42         | 1.90×    | 2.34×             | 0.73% |
| H2O (SSE4.2)                 | 2404.16 ±14.73         | 1.93×    | 2.18×             | 0.86% |
| Actix Web (scalar)           | 2440.54 ±16.47         | 1.96×    | 1.23×             | 0.94% |
| nginx (scalar)               | 2746.03 ±15.98         | 2.21×    | 1.02×             | 0.81% |
| nginx (native)               | 2812.45 ±18.09         | 2.26×    | 1.01×             | 0.90% |
| hwire + hwire_table (scalar) | 3050.02 ±13.66         | 2.45×    | 1.57×             | 0.63% |
| Actix Web (SSE4.2)           | 3142.11 ±22.78         | 2.53×    | 1.27×             | 1.01% |
| H2O (scalar)                 | 3700.77 ±36.41         | 2.97×    | 1.92×             | 1.38% |


### Split at 90% — Parse + Post-process

| Implementation               | Mean ± SD (ns/request) | Relative | Vs complete input | RCIW  |
| ---------------------------- | ---------------------- | -------- | ----------------- | ----- |
| hwire + hwire_table (native) | 1390.34 ±4.91          | 1.00×    | 1.87×             | 0.49% |
| hwire + hwire_table (SSE4.2) | 1592.17 ±11.01         | 1.15×    | 1.82×             | 0.97% |
| H2O (native)                 | 1675.61 ±12.48         | 1.21×    | 1.66×             | 1.04% |
| H2O (SSE4.2)                 | 1708.37 ±15.32         | 1.23×    | 1.55×             | 1.25% |
| hwire + hwire_table (SSE2)   | 1761.88 ±14.14         | 1.27×    | 1.80×             | 1.12% |
| Actix Web (native)           | 2604.67 ±77.52         | 1.87×    | 1.38×             | 1.78% |
| Actix Web (scalar)           | 2684.09 ±18.73         | 1.93×    | 1.35×             | 0.98% |
| nginx (scalar)               | 2751.97 ±19.37         | 1.98×    | 1.02×             | 0.98% |
| nginx (native)               | 2816.88 ±18.81         | 2.03×    | 1.01×             | 0.93% |
| H2O (scalar)                 | 3344.74 ±19.36         | 2.41×    | 1.73×             | 0.81% |
| hwire + hwire_table (scalar) | 3606.07 ±11.46         | 2.59×    | 1.85×             | 0.44% |
| Actix Web (SSE4.2)           | 3630.75 ±43.68         | 2.61×    | 1.47×             | 1.68% |


### First Header Lookup Cost and Break-even — 50% split

Estimate the total time to parse and store a request and perform its first header lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher parsing and storage cost.

The parsing component uses the displayed 50% split Parse + Post-process mean: the cumulative time for both attempts, required storage reset and final conversion. Lookup costs reuse the displayed completed-context lookup means for this same input.

Totals use the displayed means: `Parse + Post-process mean + Q × lookup mean`. Lookup costs are measured on a completed warm context; the first-lookup total is estimated, not timed immediately after parsing. The crossover is the first integer Q that beats the fastest Parse + Post-process + 1 lookup implementation.


### 50% split — Parse + Post-process + Lookup Hit — Known Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1261.21                           | 1.00×    | 16.89 ±0.07               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1481.33                           | 1.17×    | 21.55 ±0.24               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1645.47                           | 1.30×    | 21.55 ±0.53               | No crossover             |
| Actix Web (native) (prepared)         | 2351.87                           | 1.86×    | 6.42 ±0.03                | 106                      |
| Actix Web (native) (string)           | 2366.12                           | 1.88×    | 20.67 ±0.38               | No crossover             |
| H2O (native) (prepared) †             | 2368.50                           | 1.88×    | 4.73 ±0.19                | 93                       |
| H2O (native) (string)                 | 2378.55                           | 1.89×    | 14.78 ±0.25               | 531                      |
| H2O (SSE4.2) (prepared)               | 2410.20                           | 1.91×    | 6.04 ±0.07                | 107                      |
| H2O (SSE4.2) (string)                 | 2420.49                           | 1.92×    | 16.33 ±0.17               | 2072                     |
| Actix Web (scalar) (prepared)         | 2447.07                           | 1.94×    | 6.53 ±0.03                | 116                      |
| Actix Web (scalar) (string)           | 2461.90                           | 1.95×    | 21.36 ±0.09               | No crossover             |
| nginx (scalar) (prepared)             | 2748.64                           | 2.18×    | 2.61 ±0.01                | 106                      |
| nginx (scalar) (string)               | 2761.62                           | 2.19×    | 15.59 ±0.19               | 1156                     |
| nginx (native) (prepared)             | 2815.06                           | 2.23×    | 2.61 ±0.01                | 110                      |
| nginx (native) (string)               | 2828.01                           | 2.24×    | 15.56 ±0.11               | 1180                     |
| hwire + hwire_table (scalar) (string) | 3072.17                           | 2.44×    | 22.15 ±0.27               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3148.63                           | 2.50×    | 6.52 ±0.04                | 184                      |
| Actix Web (SSE4.2) (string)           | 3163.62                           | 2.51×    | 21.51 ±0.21               | No crossover             |
| H2O (scalar) (prepared)               | 3705.44                           | 2.94×    | 4.67 ±0.15                | 202                      |
| H2O (scalar) (string)                 | 3718.67                           | 2.95×    | 17.90 ±0.33               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 50% split — Parse + Post-process + Lookup Hit — Unknown Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1263.85                           | 1.00×    | 19.53 ±0.10               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1485.71                           | 1.18×    | 25.93 ±0.16               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1649.81                           | 1.31×    | 25.89 ±0.15               | No crossover             |
| Actix Web (native) (prepared)         | 2355.27                           | 1.86×    | 9.82 ±0.09                | 114                      |
| H2O (native) (prepared)               | 2376.25                           | 1.88×    | 12.48 ±0.09               | 159                      |
| H2O (native) (string)                 | 2394.68                           | 1.89×    | 30.91 ±0.10               | No crossover             |
| Actix Web (native) (string)           | 2397.96                           | 1.90×    | 52.51 ±0.24               | No crossover             |
| H2O (SSE4.2) (prepared)               | 2417.03                           | 1.91×    | 12.87 ±0.34               | 175                      |
| H2O (SSE4.2) (string)                 | 2427.80                           | 1.92×    | 23.64 ±0.29               | No crossover             |
| Actix Web (scalar) (prepared)         | 2450.38                           | 1.94×    | 9.84 ±0.07                | 124                      |
| Actix Web (scalar) (string)           | 2493.95                           | 1.97×    | 53.41 ±0.54               | No crossover             |
| nginx (scalar) (prepared)             | 2758.02                           | 2.18×    | 11.99 ±0.28               | 200                      |
| nginx (scalar) (string)               | 2774.49                           | 2.20×    | 28.46 ±0.19               | No crossover             |
| nginx (native) (prepared)             | 2825.04                           | 2.24×    | 12.59 ±0.10               | 226                      |
| nginx (native) (string)               | 2840.77                           | 2.25×    | 28.32 ±0.30               | No crossover             |
| hwire + hwire_table (scalar) (string) | 3076.75                           | 2.43×    | 26.73 ±0.17               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3152.12                           | 2.49×    | 10.01 ±0.09               | 200                      |
| Actix Web (SSE4.2) (string)           | 3195.06                           | 2.53×    | 52.95 ±0.42               | No crossover             |
| H2O (scalar) (prepared)               | 3713.53                           | 2.94×    | 12.76 ±0.17               | 363                      |
| H2O (scalar) (string)                 | 3731.57                           | 2.95×    | 30.80 ±0.72               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 50% split — Parse + Post-process + Lookup Hit — Mixed Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1262.49                           | 1.00×    | 18.17 ±0.36               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1483.65                           | 1.18×    | 23.87 ±0.22               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1647.66                           | 1.31×    | 23.74 ±0.17               | No crossover             |
| Actix Web (native) (prepared)         | 2353.89                           | 1.86×    | 8.44 ±0.07                | 114                      |
| H2O (native) (prepared)               | 2371.24                           | 1.88×    | 7.47 ±0.09                | 105                      |
| Actix Web (native) (string)           | 2381.03                           | 1.89×    | 35.58 ±0.25               | No crossover             |
| H2O (native) (string)                 | 2387.99                           | 1.89×    | 24.22 ±0.12               | No crossover             |
| H2O (SSE4.2) (prepared)               | 2412.04                           | 1.91×    | 7.88 ±0.17                | 113                      |
| H2O (SSE4.2) (string)                 | 2423.49                           | 1.92×    | 19.33 ±0.49               | No crossover             |
| Actix Web (scalar) (prepared)         | 2448.88                           | 1.94×    | 8.34 ±0.06                | 122                      |
| Actix Web (scalar) (string)           | 2475.90                           | 1.96×    | 35.36 ±0.26               | No crossover             |
| nginx (scalar) (prepared)             | 2751.53                           | 2.18×    | 5.50 ±0.09                | 119                      |
| nginx (scalar) (string)               | 2767.87                           | 2.19×    | 21.84 ±0.18               | No crossover             |
| nginx (native) (prepared)             | 2818.87                           | 2.23×    | 6.42 ±0.08                | 134                      |
| nginx (native) (string)               | 2834.35                           | 2.25×    | 21.90 ±0.44               | No crossover             |
| hwire + hwire_table (scalar) (string) | 3074.69                           | 2.44×    | 24.67 ±0.48               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3151.18                           | 2.50×    | 9.07 ±0.10                | 209                      |
| Actix Web (SSE4.2) (string)           | 3177.57                           | 2.52×    | 35.46 ±0.39               | No crossover             |
| H2O (scalar) (prepared)               | 3708.35                           | 2.94×    | 7.58 ±0.12                | 232                      |
| H2O (scalar) (string)                 | 3725.31                           | 2.95×    | 24.54 ±0.33               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 50% split — Parse + Post-process + Lookup Miss (calculated)

| Implementation                        | Parse + Post-process + 1 Miss (ns) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | ---------------------------------- | -------- | -------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1252.12                            | 1.00×    | 7.80 ±0.15                 | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1474.16                            | 1.18×    | 14.38 ±0.26                | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1638.29                            | 1.31×    | 14.37 ±0.26                | No crossover             |
| Actix Web (native) (prepared)         | 2350.47                            | 1.88×    | 5.02 ±0.13                 | 397                      |
| H2O (native) (prepared)               | 2379.37                            | 1.90×    | 15.60 ±0.32                | No crossover             |
| Actix Web (native) (string)           | 2390.84                            | 1.91×    | 45.39 ±0.34                | No crossover             |
| H2O (native) (string)                 | 2398.11                            | 1.92×    | 34.34 ±0.20                | No crossover             |
| H2O (SSE4.2) (prepared)               | 2419.44                            | 1.93×    | 15.28 ±0.19                | No crossover             |
| H2O (SSE4.2) (string)                 | 2431.70                            | 1.94×    | 27.54 ±0.37                | No crossover             |
| Actix Web (scalar) (prepared)         | 2445.97                            | 1.95×    | 5.43 ±0.08                 | 505                      |
| Actix Web (scalar) (string)           | 2485.50                            | 1.99×    | 44.96 ±0.46                | No crossover             |
| nginx (scalar) (prepared)             | 2763.68                            | 2.21×    | 17.65 ±0.31                | No crossover             |
| nginx (scalar) (string)               | 2778.61                            | 2.22×    | 32.58 ±0.41                | No crossover             |
| nginx (native) (prepared)             | 2834.04                            | 2.26×    | 21.59 ±0.50                | No crossover             |
| nginx (native) (string)               | 2840.21                            | 2.27×    | 27.76 ±0.31                | No crossover             |
| hwire + hwire_table (scalar) (string) | 3064.33                            | 2.45×    | 14.31 ±0.03                | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3147.39                            | 2.51×    | 5.28 ±0.04                 | 754                      |
| Actix Web (SSE4.2) (string)           | 3186.50                            | 2.54×    | 44.39 ±0.59                | No crossover             |
| H2O (scalar) (prepared)               | 3716.64                            | 2.97×    | 15.87 ±0.51                | No crossover             |
| H2O (scalar) (string)                 | 3733.22                            | 2.98×    | 32.45 ±0.30                | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### First Header Lookup Cost and Break-even — 90% split

Estimate the total time to parse and store a request and perform its first header lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher parsing and storage cost.

The parsing component uses the displayed 90% split Parse + Post-process mean: the cumulative time for both attempts, required storage reset and final conversion. Lookup costs reuse the displayed completed-context lookup means for this same input.

Totals use the displayed means: `Parse + Post-process mean + Q × lookup mean`. Lookup costs are measured on a completed warm context; the first-lookup total is estimated, not timed immediately after parsing. The crossover is the first integer Q that beats the fastest Parse + Post-process + 1 lookup implementation.


### 90% split — Parse + Post-process + Lookup Hit — Known Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1407.23                           | 1.00×    | 16.89 ±0.07               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1613.72                           | 1.15×    | 21.55 ±0.24               | No crossover             |
| H2O (native) (prepared) †             | 1680.34                           | 1.19×    | 4.73 ±0.19                | 24                       |
| H2O (native) (string)                 | 1690.39                           | 1.20×    | 14.78 ±0.25               | 136                      |
| H2O (SSE4.2) (prepared)               | 1714.41                           | 1.22×    | 6.04 ±0.07                | 30                       |
| H2O (SSE4.2) (string)                 | 1724.70                           | 1.23×    | 16.33 ±0.17               | 568                      |
| hwire + hwire_table (SSE2) (string)   | 1783.43                           | 1.27×    | 21.55 ±0.53               | No crossover             |
| Actix Web (native) (prepared)         | 2611.09                           | 1.86×    | 6.42 ±0.03                | 116                      |
| Actix Web (native) (string)           | 2625.34                           | 1.87×    | 20.67 ±0.38               | No crossover             |
| Actix Web (scalar) (prepared)         | 2690.62                           | 1.91×    | 6.53 ±0.03                | 125                      |
| Actix Web (scalar) (string)           | 2705.45                           | 1.92×    | 21.36 ±0.09               | No crossover             |
| nginx (scalar) (prepared)             | 2754.58                           | 1.96×    | 2.61 ±0.01                | 96                       |
| nginx (scalar) (string)               | 2767.56                           | 1.97×    | 15.59 ±0.19               | 1048                     |
| nginx (native) (prepared)             | 2819.49                           | 2.00×    | 2.61 ±0.01                | 100                      |
| nginx (native) (string)               | 2832.44                           | 2.01×    | 15.56 ±0.11               | 1073                     |
| H2O (scalar) (prepared)               | 3349.41                           | 2.38×    | 4.67 ±0.15                | 160                      |
| H2O (scalar) (string)                 | 3362.64                           | 2.39×    | 17.90 ±0.33               | No crossover             |
| hwire + hwire_table (scalar) (string) | 3628.22                           | 2.58×    | 22.15 ±0.27               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3637.27                           | 2.58×    | 6.52 ±0.04                | 217                      |
| Actix Web (SSE4.2) (string)           | 3652.26                           | 2.60×    | 21.51 ±0.21               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 90% split — Parse + Post-process + Lookup Hit — Unknown Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1409.87                           | 1.00×    | 19.53 ±0.10               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1618.10                           | 1.15×    | 25.93 ±0.16               | No crossover             |
| H2O (native) (prepared)               | 1688.09                           | 1.20×    | 12.48 ±0.09               | 41                       |
| H2O (native) (string)                 | 1706.52                           | 1.21×    | 30.91 ±0.10               | No crossover             |
| H2O (SSE4.2) (prepared)               | 1721.24                           | 1.22×    | 12.87 ±0.34               | 48                       |
| H2O (SSE4.2) (string)                 | 1732.01                           | 1.23×    | 23.64 ±0.29               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1787.77                           | 1.27×    | 25.89 ±0.15               | No crossover             |
| Actix Web (native) (prepared)         | 2614.49                           | 1.85×    | 9.82 ±0.09                | 126                      |
| Actix Web (native) (string)           | 2657.18                           | 1.88×    | 52.51 ±0.24               | No crossover             |
| Actix Web (scalar) (prepared)         | 2693.93                           | 1.91×    | 9.84 ±0.07                | 134                      |
| Actix Web (scalar) (string)           | 2737.50                           | 1.94×    | 53.41 ±0.54               | No crossover             |
| nginx (scalar) (prepared)             | 2763.96                           | 1.96×    | 11.99 ±0.28               | 181                      |
| nginx (scalar) (string)               | 2780.43                           | 1.97×    | 28.46 ±0.19               | No crossover             |
| nginx (native) (prepared)             | 2829.47                           | 2.01×    | 12.59 ±0.10               | 206                      |
| nginx (native) (string)               | 2845.20                           | 2.02×    | 28.32 ±0.30               | No crossover             |
| H2O (scalar) (prepared)               | 3357.50                           | 2.38×    | 12.76 ±0.17               | 289                      |
| H2O (scalar) (string)                 | 3375.54                           | 2.39×    | 30.80 ±0.72               | No crossover             |
| hwire + hwire_table (scalar) (string) | 3632.80                           | 2.58×    | 26.73 ±0.17               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3640.76                           | 2.58×    | 10.01 ±0.09               | 236                      |
| Actix Web (SSE4.2) (string)           | 3683.70                           | 2.61×    | 52.95 ±0.42               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 90% split — Parse + Post-process + Lookup Hit — Mixed Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1408.51                           | 1.00×    | 18.17 ±0.36               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1616.04                           | 1.15×    | 23.87 ±0.22               | No crossover             |
| H2O (native) (prepared)               | 1683.08                           | 1.19×    | 7.47 ±0.09                | 27                       |
| H2O (native) (string)                 | 1699.83                           | 1.21×    | 24.22 ±0.12               | No crossover             |
| H2O (SSE4.2) (prepared)               | 1716.25                           | 1.22×    | 7.88 ±0.17                | 31                       |
| H2O (SSE4.2) (string)                 | 1727.70                           | 1.23×    | 19.33 ±0.49               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1785.62                           | 1.27×    | 23.74 ±0.17               | No crossover             |
| Actix Web (native) (prepared)         | 2613.11                           | 1.86×    | 8.44 ±0.07                | 125                      |
| Actix Web (native) (string)           | 2640.25                           | 1.87×    | 35.58 ±0.25               | No crossover             |
| Actix Web (scalar) (prepared)         | 2692.43                           | 1.91×    | 8.34 ±0.06                | 132                      |
| Actix Web (scalar) (string)           | 2719.45                           | 1.93×    | 35.36 ±0.26               | No crossover             |
| nginx (scalar) (prepared)             | 2757.47                           | 1.96×    | 5.50 ±0.09                | 108                      |
| nginx (scalar) (string)               | 2773.81                           | 1.97×    | 21.84 ±0.18               | No crossover             |
| nginx (native) (prepared)             | 2823.30                           | 2.00×    | 6.42 ±0.08                | 122                      |
| nginx (native) (string)               | 2838.78                           | 2.02×    | 21.90 ±0.44               | No crossover             |
| H2O (scalar) (prepared)               | 3352.32                           | 2.38×    | 7.58 ±0.12                | 185                      |
| H2O (scalar) (string)                 | 3369.28                           | 2.39×    | 24.54 ±0.33               | No crossover             |
| hwire + hwire_table (scalar) (string) | 3630.74                           | 2.58×    | 24.67 ±0.48               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3639.82                           | 2.58×    | 9.07 ±0.10                | 247                      |
| Actix Web (SSE4.2) (string)           | 3666.21                           | 2.60×    | 35.46 ±0.39               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 90% split — Parse + Post-process + Lookup Miss (calculated)

| Implementation                        | Parse + Post-process + 1 Miss (ns) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | ---------------------------------- | -------- | -------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1398.14                            | 1.00×    | 7.80 ±0.15                 | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1606.55                            | 1.15×    | 14.38 ±0.26                | No crossover             |
| H2O (native) (prepared)               | 1691.21                            | 1.21×    | 15.60 ±0.32                | No crossover             |
| H2O (native) (string)                 | 1709.95                            | 1.22×    | 34.34 ±0.20                | No crossover             |
| H2O (SSE4.2) (prepared)               | 1723.65                            | 1.23×    | 15.28 ±0.19                | No crossover             |
| H2O (SSE4.2) (string)                 | 1735.91                            | 1.24×    | 27.54 ±0.37                | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1776.25                            | 1.27×    | 14.37 ±0.26                | No crossover             |
| Actix Web (native) (prepared)         | 2609.69                            | 1.87×    | 5.02 ±0.13                 | 437                      |
| Actix Web (native) (string)           | 2650.06                            | 1.90×    | 45.39 ±0.34                | No crossover             |
| Actix Web (scalar) (prepared)         | 2689.52                            | 1.92×    | 5.43 ±0.08                 | 546                      |
| Actix Web (scalar) (string)           | 2729.05                            | 1.95×    | 44.96 ±0.46                | No crossover             |
| nginx (scalar) (prepared)             | 2769.62                            | 1.98×    | 17.65 ±0.31                | No crossover             |
| nginx (scalar) (string)               | 2784.55                            | 1.99×    | 32.58 ±0.41                | No crossover             |
| nginx (native) (prepared)             | 2838.47                            | 2.03×    | 21.59 ±0.50                | No crossover             |
| nginx (native) (string)               | 2844.64                            | 2.03×    | 27.76 ±0.31                | No crossover             |
| H2O (scalar) (prepared)               | 3360.61                            | 2.40×    | 15.87 ±0.51                | No crossover             |
| H2O (scalar) (string)                 | 3377.19                            | 2.42×    | 32.45 ±0.30                | No crossover             |
| hwire + hwire_table (scalar) (string) | 3620.38                            | 2.59×    | 14.31 ±0.03                | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3636.03                            | 2.60×    | 5.28 ±0.04                 | 890                      |
| Actix Web (SSE4.2) (string)           | 3675.14                            | 2.63×    | 44.39 ±0.59                | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.
