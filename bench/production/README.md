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
date                 : 2026-10-08T10:06:50+09:00
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
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
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
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
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
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
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
- Compiler: <!-- compiler:actix_web -->`rustc 1.93.1 (01f6ddf75 2026-02-11)`<!-- /compiler -->; C ABI shim: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
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
| hwire + hwire_table (native) | 504.84 ±6.81           | 1.00×    | 1.98 M req/s | 1.89% |
| hwire + hwire_table (SSE4.2) | 680.74 ±7.37           | 1.35×    | 1.47 M req/s | 1.51% |
| hwire + hwire_table (SSE2)   | 722.36 ±8.02           | 1.43×    | 1.38 M req/s | 1.55% |
| H2O (native)                 | 767.97 ±10.64          | 1.52×    | 1.30 M req/s | 1.52% |
| H2O (SSE4.2)                 | 831.54 ±29.34 †        | 1.65×    | 1.20 M req/s | 2.00% |
| hwire + hwire_table (scalar) | 905.67 ±28.07          | 1.79×    | 1.10 M req/s | 1.98% |
| H2O (scalar)                 | 980.36 ±15.50          | 1.94×    | 1.02 M req/s | 1.73% |
| nginx (native)               | 1138.56 ±15.22         | 2.26×    | 0.88 M req/s | 1.46% |
| nginx (scalar)               | 1204.34 ±9.90          | 2.39×    | 0.83 M req/s | 1.15% |
| Actix Web (native)           | 1366.46 ±14.61         | 2.71×    | 0.73 M req/s | 1.50% |
| Actix Web (scalar)           | 1461.30 ±19.46         | 2.89×    | 0.68 M req/s | 1.46% |
| Actix Web (SSE4.2)           | 1492.34 ±22.98         | 2.96×    | 0.67 M req/s | 1.68% |


### Lookup Hit — Known Headers

| Implementation                        | Mean ± SD (ns/lookup) | Relative | Throughput         | RCIW  |
| ------------------------------------- | --------------------- | -------- | ------------------ | ----- |
| nginx (native) (prepared)             | 2.83 ±0.03            | 1.00×    | 353.36 M lookups/s | 1.22% |
| nginx (scalar) (prepared)             | 2.84 ±0.02            | 1.00×    | 352.11 M lookups/s | 0.96% |
| H2O (SSE4.2) (prepared)               | 4.58 ±0.06            | 1.62×    | 218.34 M lookups/s | 1.97% |
| H2O (native) (prepared)               | 4.73 ±0.08            | 1.67×    | 211.42 M lookups/s | 1.64% |
| H2O (scalar) (prepared)               | 4.83 ±0.06            | 1.71×    | 207.04 M lookups/s | 1.84% |
| Actix Web (native) (prepared)         | 6.66 ±0.04            | 2.35×    | 150.15 M lookups/s | 0.85% |
| Actix Web (scalar) (prepared)         | 6.84 ±0.12            | 2.42×    | 146.20 M lookups/s | 1.68% |
| Actix Web (SSE4.2) (prepared)         | 7.88 ±0.07            | 2.78×    | 126.90 M lookups/s | 1.30% |
| H2O (SSE4.2) (string)                 | 15.06 ±0.20           | 5.32×    | 66.40 M lookups/s  | 1.45% |
| H2O (native) (string)                 | 15.32 ±0.16           | 5.41×    | 65.27 M lookups/s  | 1.46% |
| H2O (scalar) (string)                 | 15.48 ±0.32           | 5.47×    | 64.60 M lookups/s  | 1.67% |
| nginx (scalar) (string)               | 15.54 ±0.08           | 5.49×    | 64.35 M lookups/s  | 0.74% |
| nginx (native) (string)               | 15.70 ±0.20           | 5.55×    | 63.69 M lookups/s  | 1.43% |
| hwire + hwire_table (native) (string) | 16.96 ±0.15           | 5.99×    | 58.96 M lookups/s  | 1.27% |
| Actix Web (native) (string)           | 20.79 ±0.17           | 7.35×    | 48.10 M lookups/s  | 1.13% |
| hwire + hwire_table (SSE2) (string)   | 21.37 ±0.14           | 7.55×    | 46.79 M lookups/s  | 0.91% |
| Actix Web (scalar) (string)           | 21.52 ±0.23           | 7.60×    | 46.47 M lookups/s  | 1.50% |
| hwire + hwire_table (scalar) (string) | 21.71 ±0.31           | 7.67×    | 46.06 M lookups/s  | 1.99% |
| Actix Web (SSE4.2) (string)           | 21.84 ±0.16           | 7.72×    | 45.79 M lookups/s  | 1.00% |
| hwire + hwire_table (SSE4.2) (string) | 22.22 ±0.10           | 7.85×    | 45.00 M lookups/s  | 0.65% |

Searches Host, Accept, Cookie, User-Agent, Connection and Referer, in that order, repeated with equal frequency.


### Lookup Hit — Unknown Headers

| Implementation                        | Mean ± SD (ns/lookup) | Relative | Throughput        | RCIW  |
| ------------------------------------- | --------------------- | -------- | ----------------- | ----- |
| Actix Web (native) (prepared)         | 10.32 ±0.24           | 1.00×    | 96.90 M lookups/s | 1.90% |
| Actix Web (SSE4.2) (prepared)         | 10.35 ±0.07           | 1.00×    | 96.62 M lookups/s | 0.99% |
| Actix Web (scalar) (prepared)         | 10.44 ±0.18           | 1.01×    | 95.79 M lookups/s | 1.62% |
| nginx (native) (prepared)             | 10.96 ±0.32           | 1.06×    | 91.24 M lookups/s | 1.98% |
| nginx (scalar) (prepared)             | 11.73 ±0.07           | 1.14×    | 85.25 M lookups/s | 0.81% |
| H2O (SSE4.2) (prepared)               | 12.41 ±0.17           | 1.20×    | 80.58 M lookups/s | 1.93% |
| H2O (native) (prepared)               | 12.82 ±0.23           | 1.24×    | 78.00 M lookups/s | 1.99% |
| H2O (scalar) (prepared)               | 15.02 ±0.20           | 1.46×    | 66.58 M lookups/s | 1.48% |
| hwire + hwire_table (native) (string) | 19.73 ±0.13           | 1.91×    | 50.68 M lookups/s | 0.93% |
| H2O (native) (string)                 | 23.83 ±0.32           | 2.31×    | 41.96 M lookups/s | 1.88% |
| hwire + hwire_table (scalar) (string) | 25.87 ±0.21           | 2.51×    | 38.65 M lookups/s | 1.12% |
| hwire + hwire_table (SSE2) (string)   | 25.87 ±0.13           | 2.51×    | 38.65 M lookups/s | 0.72% |
| hwire + hwire_table (SSE4.2) (string) | 25.95 ±0.19           | 2.51×    | 38.54 M lookups/s | 1.01% |
| H2O (SSE4.2) (string)                 | 26.25 ±0.35           | 2.54×    | 38.10 M lookups/s | 1.86% |
| H2O (scalar) (string)                 | 27.02 ±0.25           | 2.62×    | 37.01 M lookups/s | 1.32% |
| nginx (scalar) (string)               | 27.95 ±0.23           | 2.71×    | 35.78 M lookups/s | 1.15% |
| nginx (native) (string)               | 28.21 ±0.27           | 2.73×    | 35.45 M lookups/s | 1.33% |
| Actix Web (native) (string)           | 52.99 ±0.47           | 5.13×    | 18.87 M lookups/s | 1.25% |
| Actix Web (scalar) (string)           | 53.49 ±1.35           | 5.18×    | 18.70 M lookups/s | 1.88% |
| Actix Web (SSE4.2) (string)           | 54.08 ±0.58           | 5.24×    | 18.49 M lookups/s | 1.51% |

Searches Sec-Fetch-Site, Sec-Fetch-Mode, Sec-Fetch-User, Sec-Fetch-Dest, Sec-CH-UA and Sec-CH-UA-Platform, in that order, repeated with equal frequency.


### Lookup Hit — Mixed Headers

| Implementation                        | Mean ± SD (ns/lookup) | Relative | Throughput         | RCIW  |
| ------------------------------------- | --------------------- | -------- | ------------------ | ----- |
| nginx (native) (prepared)             | 5.34 ±0.14            | 1.00×    | 187.27 M lookups/s | 1.91% |
| nginx (scalar) (prepared)             | 5.61 ±0.10            | 1.05×    | 178.25 M lookups/s | 1.89% |
| H2O (native) (prepared)               | 7.67 ±0.12            | 1.44×    | 130.38 M lookups/s | 1.66% |
| H2O (SSE4.2) (prepared)               | 7.96 ±0.10            | 1.49×    | 125.63 M lookups/s | 1.81% |
| Actix Web (native) (prepared)         | 8.50 ±0.05            | 1.59×    | 117.65 M lookups/s | 0.88% |
| Actix Web (scalar) (prepared)         | 8.99 ±0.18            | 1.68×    | 111.23 M lookups/s | 1.69% |
| H2O (scalar) (prepared)               | 9.01 ±0.11            | 1.69×    | 110.99 M lookups/s | 1.35% |
| Actix Web (SSE4.2) (prepared)         | 9.49 ±0.05            | 1.78×    | 105.37 M lookups/s | 0.77% |
| hwire + hwire_table (native) (string) | 18.52 ±0.65           | 3.47×    | 54.00 M lookups/s  | 1.99% |
| H2O (native) (string)                 | 19.53 ±0.17           | 3.66×    | 51.20 M lookups/s  | 1.24% |
| H2O (SSE4.2) (string)                 | 20.47 ±0.27           | 3.83×    | 48.85 M lookups/s  | 1.42% |
| H2O (scalar) (string)                 | 21.05 ±0.21           | 3.94×    | 47.51 M lookups/s  | 1.37% |
| nginx (scalar) (string)               | 21.68 ±0.21           | 4.06×    | 46.13 M lookups/s  | 1.38% |
| nginx (native) (string)               | 21.97 ±0.18           | 4.11×    | 45.52 M lookups/s  | 1.16% |
| hwire + hwire_table (SSE2) (string)   | 23.78 ±0.21           | 4.45×    | 42.05 M lookups/s  | 1.25% |
| hwire + hwire_table (scalar) (string) | 25.26 ±0.18           | 4.73×    | 39.59 M lookups/s  | 1.01% |
| hwire + hwire_table (SSE4.2) (string) | 26.20 ±0.33           | 4.91×    | 38.17 M lookups/s  | 1.78% |
| Actix Web (SSE4.2) (string)           | 36.07 ±0.25           | 6.75×    | 27.72 M lookups/s  | 0.98% |
| Actix Web (scalar) (string)           | 36.22 ±0.39           | 6.78×    | 27.61 M lookups/s  | 1.52% |
| Actix Web (native) (string)           | 36.47 ±0.33           | 6.83×    | 27.42 M lookups/s  | 1.27% |

Searches Host, Sec-Fetch-Site, Cookie, Sec-Fetch-Mode, Connection and Sec-CH-UA-Platform, in that order, repeated with equal frequency.


### Lookup Miss

| Implementation                        | Mean ± SD (ns/lookup) | Relative | Throughput         | RCIW  |
| ------------------------------------- | --------------------- | -------- | ------------------ | ----- |
| Actix Web (scalar) (prepared)         | 5.73 ±0.07            | 1.00×    | 174.52 M lookups/s | 1.32% |
| Actix Web (native) (prepared)         | 5.93 ±0.47 †          | 1.03×    | 168.63 M lookups/s | 4.45% |
| Actix Web (SSE4.2) (prepared)         | 7.53 ±0.04            | 1.31×    | 132.80 M lookups/s | 0.64% |
| hwire + hwire_table (native) (string) | 8.26 ±0.10            | 1.44×    | 121.07 M lookups/s | 1.72% |
| hwire + hwire_table (SSE2) (string)   | 14.28 ±0.09           | 2.49×    | 70.03 M lookups/s  | 0.92% |
| hwire + hwire_table (SSE4.2) (string) | 14.64 ±0.10           | 2.55×    | 68.31 M lookups/s  | 0.96% |
| H2O (native) (prepared)               | 14.66 ±0.51           | 2.56×    | 68.21 M lookups/s  | 1.97% |
| H2O (SSE4.2) (prepared)               | 15.03 ±0.33           | 2.62×    | 66.53 M lookups/s  | 1.80% |
| hwire + hwire_table (scalar) (string) | 16.15 ±0.15           | 2.82×    | 61.92 M lookups/s  | 1.32% |
| nginx (native) (prepared)             | 16.90 ±0.35           | 2.95×    | 59.17 M lookups/s  | 1.91% |
| nginx (scalar) (prepared)             | 17.96 ±0.30           | 3.13×    | 55.68 M lookups/s  | 1.85% |
| H2O (scalar) (prepared)               | 19.43 ±0.41           | 3.39×    | 51.47 M lookups/s  | 1.95% |
| nginx (scalar) (string)               | 27.55 ±0.28           | 4.81×    | 36.30 M lookups/s  | 1.41% |
| H2O (native) (string)                 | 27.77 ±0.39           | 4.85×    | 36.01 M lookups/s  | 1.94% |
| nginx (native) (string)               | 28.77 ±0.41           | 5.02×    | 34.76 M lookups/s  | 1.55% |
| H2O (SSE4.2) (string)                 | 30.81 ±0.52           | 5.38×    | 32.46 M lookups/s  | 1.86% |
| H2O (scalar) (string)                 | 33.00 ±0.67           | 5.76×    | 30.30 M lookups/s  | 1.87% |
| Actix Web (native) (string)           | 42.34 ±0.30           | 7.39×    | 23.62 M lookups/s  | 0.98% |
| Actix Web (SSE4.2) (string)           | 45.35 ±0.33           | 7.91×    | 22.05 M lookups/s  | 1.01% |
| Actix Web (scalar) (string)           | 46.04 ±0.68           | 8.03×    | 21.72 M lookups/s  | 1.62% |

Searches Hots, Accpet, Cooxie, User-Agend, Sec-CH-UA-Platforn and Referef, in that order, repeated with equal frequency.


### First Header Lookup Cost and Break-even — Complete input

Estimate the total time to parse and store a request and perform its first header lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher parsing and storage cost.

Totals use the displayed means: `Parse + Post-process mean + Q × lookup mean`. Lookup costs are measured on a completed warm context; the first-lookup total is estimated, not timed immediately after parsing. The crossover is the first integer Q that beats the fastest Parse + Post-process + 1 lookup implementation.


### Parse + Post-process + Lookup Hit — Known Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 521.80                            | 1.00×    | 16.96 ±0.15               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 702.96                            | 1.35×    | 22.22 ±0.10               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 743.73                            | 1.43×    | 21.37 ±0.14               | No crossover             |
| H2O (native) (prepared)               | 772.70                            | 1.48×    | 4.73 ±0.08                | 22                       |
| H2O (native) (string)                 | 783.29                            | 1.50×    | 15.32 ±0.16               | 161                      |
| H2O (SSE4.2) (prepared) †             | 836.12                            | 1.60×    | 4.58 ±0.06                | 27                       |
| H2O (SSE4.2) (string) †               | 846.60                            | 1.62×    | 15.06 ±0.20               | 172                      |
| hwire + hwire_table (scalar) (string) | 927.38                            | 1.78×    | 21.71 ±0.31               | No crossover             |
| H2O (scalar) (prepared)               | 985.19                            | 1.89×    | 4.83 ±0.06                | 40                       |
| H2O (scalar) (string)                 | 995.84                            | 1.91×    | 15.48 ±0.32               | 322                      |
| nginx (native) (prepared)             | 1141.39                           | 2.19×    | 2.83 ±0.03                | 45                       |
| nginx (native) (string)               | 1154.26                           | 2.21×    | 15.70 ±0.20               | 503                      |
| nginx (scalar) (prepared)             | 1207.18                           | 2.31×    | 2.84 ±0.02                | 50                       |
| nginx (scalar) (string)               | 1219.88                           | 2.34×    | 15.54 ±0.08               | 493                      |
| Actix Web (native) (prepared)         | 1373.12                           | 2.63×    | 6.66 ±0.04                | 84                       |
| Actix Web (native) (string)           | 1387.25                           | 2.66×    | 20.79 ±0.17               | No crossover             |
| Actix Web (scalar) (prepared)         | 1468.14                           | 2.81×    | 6.84 ±0.12                | 95                       |
| Actix Web (scalar) (string)           | 1482.82                           | 2.84×    | 21.52 ±0.23               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 1500.22                           | 2.88×    | 7.88 ±0.07                | 109                      |
| Actix Web (SSE4.2) (string)           | 1514.18                           | 2.90×    | 21.84 ±0.16               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### Parse + Post-process + Lookup Hit — Unknown Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 524.57                            | 1.00×    | 19.73 ±0.13               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 706.69                            | 1.35×    | 25.95 ±0.19               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 748.23                            | 1.43×    | 25.87 ±0.13               | No crossover             |
| H2O (native) (prepared)               | 780.79                            | 1.49×    | 12.82 ±0.23               | 39                       |
| H2O (native) (string)                 | 791.80                            | 1.51×    | 23.83 ±0.32               | No crossover             |
| H2O (SSE4.2) (prepared) †             | 843.95                            | 1.61×    | 12.41 ±0.17               | 45                       |
| H2O (SSE4.2) (string) †               | 857.79                            | 1.64×    | 26.25 ±0.35               | No crossover             |
| hwire + hwire_table (scalar) (string) | 931.54                            | 1.78×    | 25.87 ±0.21               | No crossover             |
| H2O (scalar) (prepared)               | 995.38                            | 1.90×    | 15.02 ±0.20               | 101                      |
| H2O (scalar) (string)                 | 1007.38                           | 1.92×    | 27.02 ±0.25               | No crossover             |
| nginx (native) (prepared)             | 1149.52                           | 2.19×    | 10.96 ±0.32               | 73                       |
| nginx (native) (string)               | 1166.77                           | 2.22×    | 28.21 ±0.27               | No crossover             |
| nginx (scalar) (prepared)             | 1216.07                           | 2.32×    | 11.73 ±0.07               | 88                       |
| nginx (scalar) (string)               | 1232.29                           | 2.35×    | 27.95 ±0.23               | No crossover             |
| Actix Web (native) (prepared)         | 1376.78                           | 2.62×    | 10.32 ±0.24               | 92                       |
| Actix Web (native) (string)           | 1419.45                           | 2.71×    | 52.99 ±0.47               | No crossover             |
| Actix Web (scalar) (prepared)         | 1471.74                           | 2.81×    | 10.44 ±0.18               | 103                      |
| Actix Web (SSE4.2) (prepared)         | 1502.69                           | 2.86×    | 10.35 ±0.07               | 106                      |
| Actix Web (scalar) (string)           | 1514.79                           | 2.89×    | 53.49 ±1.35               | No crossover             |
| Actix Web (SSE4.2) (string)           | 1546.42                           | 2.95×    | 54.08 ±0.58               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### Parse + Post-process + Lookup Hit — Mixed Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 523.36                            | 1.00×    | 18.52 ±0.65               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 706.94                            | 1.35×    | 26.20 ±0.33               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 746.14                            | 1.43×    | 23.78 ±0.21               | No crossover             |
| H2O (native) (prepared)               | 775.64                            | 1.48×    | 7.67 ±0.12                | 25                       |
| H2O (native) (string)                 | 787.50                            | 1.50×    | 19.53 ±0.17               | No crossover             |
| H2O (SSE4.2) (prepared) †             | 839.50                            | 1.60×    | 7.96 ±0.10                | 31                       |
| H2O (SSE4.2) (string) †               | 852.01                            | 1.63×    | 20.47 ±0.27               | No crossover             |
| hwire + hwire_table (scalar) (string) | 930.93                            | 1.78×    | 25.26 ±0.18               | No crossover             |
| H2O (scalar) (prepared)               | 989.37                            | 1.89×    | 9.01 ±0.11                | 51                       |
| H2O (scalar) (string)                 | 1001.41                           | 1.91×    | 21.05 ±0.21               | No crossover             |
| nginx (native) (prepared)             | 1143.90                           | 2.19×    | 5.34 ±0.14                | 49                       |
| nginx (native) (string)               | 1160.53                           | 2.22×    | 21.97 ±0.18               | No crossover             |
| nginx (scalar) (prepared)             | 1209.95                           | 2.31×    | 5.61 ±0.10                | 55                       |
| nginx (scalar) (string)               | 1226.02                           | 2.34×    | 21.68 ±0.21               | No crossover             |
| Actix Web (native) (prepared)         | 1374.96                           | 2.63×    | 8.50 ±0.05                | 86                       |
| Actix Web (native) (string)           | 1402.93                           | 2.68×    | 36.47 ±0.33               | No crossover             |
| Actix Web (scalar) (prepared)         | 1470.29                           | 2.81×    | 8.99 ±0.18                | 101                      |
| Actix Web (scalar) (string)           | 1497.52                           | 2.86×    | 36.22 ±0.39               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 1501.83                           | 2.87×    | 9.49 ±0.05                | 110                      |
| Actix Web (SSE4.2) (string)           | 1528.41                           | 2.92×    | 36.07 ±0.25               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### Parse + Post-process + Lookup Miss (calculated)

| Implementation                        | Parse + Post-process + 1 Miss (ns) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | ---------------------------------- | -------- | -------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 513.10                             | 1.00×    | 8.26 ±0.10                 | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 695.38                             | 1.36×    | 14.64 ±0.10                | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 736.64                             | 1.44×    | 14.28 ±0.09                | No crossover             |
| H2O (native) (prepared)               | 782.63                             | 1.53×    | 14.66 ±0.51                | No crossover             |
| H2O (native) (string)                 | 795.74                             | 1.55×    | 27.77 ±0.39                | No crossover             |
| H2O (SSE4.2) (prepared) †             | 846.57                             | 1.65×    | 15.03 ±0.33                | No crossover             |
| H2O (SSE4.2) (string) †               | 862.35                             | 1.68×    | 30.81 ±0.52                | No crossover             |
| hwire + hwire_table (scalar) (string) | 921.82                             | 1.80×    | 16.15 ±0.15                | No crossover             |
| H2O (scalar) (prepared)               | 999.79                             | 1.95×    | 19.43 ±0.41                | No crossover             |
| H2O (scalar) (string)                 | 1013.36                            | 1.97×    | 33.00 ±0.67                | No crossover             |
| nginx (native) (prepared)             | 1155.46                            | 2.25×    | 16.90 ±0.35                | No crossover             |
| nginx (native) (string)               | 1167.33                            | 2.28×    | 28.77 ±0.41                | No crossover             |
| nginx (scalar) (prepared)             | 1222.30                            | 2.38×    | 17.96 ±0.30                | No crossover             |
| nginx (scalar) (string)               | 1231.89                            | 2.40×    | 27.55 ±0.28                | No crossover             |
| Actix Web (native) (prepared) †       | 1372.39                            | 2.67×    | 5.93 ±0.47                 | 370                      |
| Actix Web (native) (string)           | 1408.80                            | 2.75×    | 42.34 ±0.30                | No crossover             |
| Actix Web (scalar) (prepared)         | 1467.03                            | 2.86×    | 5.73 ±0.07                 | 379                      |
| Actix Web (SSE4.2) (prepared)         | 1499.87                            | 2.92×    | 7.53 ±0.04                 | 1353                     |
| Actix Web (scalar) (string)           | 1507.34                            | 2.94×    | 46.04 ±0.68                | No crossover             |
| Actix Web (SSE4.2) (string)           | 1537.69                            | 3.00×    | 45.35 ±0.33                | No crossover             |

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
| hwire + hwire_table (native) | 727.16 ±7.99           | 1.00×    | 1.38 M req/s | 1.54% |
| hwire + hwire_table (SSE4.2) | 916.47 ±8.57           | 1.26×    | 1.09 M req/s | 1.31% |
| hwire + hwire_table (SSE2)   | 937.23 ±8.05           | 1.29×    | 1.07 M req/s | 1.20% |
| H2O (native)                 | 1007.10 ±10.52         | 1.38×    | 0.99 M req/s | 1.46% |
| H2O (SSE4.2)                 | 1087.27 ±15.14         | 1.50×    | 0.92 M req/s | 1.95% |
| Actix Web (native)           | 1862.25 ±19.12         | 2.56×    | 0.54 M req/s | 1.44% |
| H2O (scalar)                 | 1884.13 ±46.70         | 2.59×    | 0.53 M req/s | 1.84% |
| hwire + hwire_table (scalar) | 1939.20 ±15.91         | 2.67×    | 0.52 M req/s | 1.15% |
| Actix Web (scalar)           | 2012.65 ±21.75         | 2.77×    | 0.50 M req/s | 1.51% |
| Actix Web (SSE4.2)           | 2424.36 ±33.31         | 3.33×    | 0.41 M req/s | 1.92% |
| nginx (native)               | 2738.09 ±14.33         | 3.77×    | 0.37 M req/s | 0.73% |
| nginx (scalar)               | 2740.16 ±16.02         | 3.77×    | 0.36 M req/s | 0.82% |


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
| hwire + hwire_table (native) | 1201.85 ±5.52          | 1.00×    | 1.65×             | 0.64% |
| hwire + hwire_table (SSE4.2) | 1516.77 ±12.41         | 1.26×    | 1.66×             | 1.14% |
| hwire + hwire_table (SSE2)   | 1558.06 ±8.90          | 1.30×    | 1.66×             | 0.80% |
| Actix Web (native)           | 2323.57 ±11.14         | 1.93×    | 1.25×             | 0.67% |
| H2O (native)                 | 2329.65 ±10.07         | 1.94×    | 2.31×             | 0.60% |
| H2O (SSE4.2)                 | 2420.09 ±22.18         | 2.01×    | 2.23×             | 1.28% |
| Actix Web (scalar)           | 2501.72 ±35.40         | 2.08×    | 1.24×             | 1.55% |
| nginx (native)               | 2747.83 ±22.25         | 2.29×    | 1.00×             | 1.13% |
| nginx (scalar)               | 2793.29 ±21.62         | 2.32×    | 1.02×             | 1.08% |
| hwire + hwire_table (scalar) | 3043.77 ±20.42         | 2.53×    | 1.57×             | 0.94% |
| Actix Web (SSE4.2)           | 3091.57 ±21.37         | 2.57×    | 1.28×             | 0.97% |
| H2O (scalar)                 | 3652.64 ±64.51         | 3.04×    | 1.94×             | 1.93% |


### Split at 90% — Parse + Post-process

| Implementation               | Mean ± SD (ns/request) | Relative | Vs complete input | RCIW  |
| ---------------------------- | ---------------------- | -------- | ----------------- | ----- |
| hwire + hwire_table (native) | 1324.80 ±11.41         | 1.00×    | 1.82×             | 1.20% |
| H2O (native)                 | 1650.25 ±6.88          | 1.25×    | 1.64×             | 0.58% |
| hwire + hwire_table (SSE4.2) | 1651.98 ±11.69         | 1.25×    | 1.80×             | 0.99% |
| hwire + hwire_table (SSE2)   | 1673.81 ±10.18         | 1.26×    | 1.79×             | 0.85% |
| H2O (SSE4.2)                 | 1740.06 ±13.60         | 1.31×    | 1.60×             | 1.09% |
| Actix Web (native)           | 2572.93 ±18.43         | 1.94×    | 1.38×             | 1.00% |
| nginx (native)               | 2748.90 ±17.06         | 2.07×    | 1.00×             | 0.87% |
| Actix Web (scalar)           | 2768.18 ±22.16         | 2.09×    | 1.38×             | 1.12% |
| nginx (scalar)               | 2789.85 ±19.19         | 2.11×    | 1.02×             | 0.96% |
| H2O (scalar)                 | 3423.39 ±44.85         | 2.58×    | 1.82×             | 1.83% |
| Actix Web (SSE4.2)           | 3562.11 ±26.57         | 2.69×    | 1.47×             | 1.04% |
| hwire + hwire_table (scalar) | 3609.12 ±16.79         | 2.72×    | 1.86×             | 0.65% |


### First Header Lookup Cost and Break-even — 50% split

Estimate the total time to parse and store a request and perform its first header lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher parsing and storage cost.

The parsing component uses the displayed 50% split Parse + Post-process mean: the cumulative time for both attempts, required storage reset and final conversion. Lookup costs reuse the displayed completed-context lookup means for this same input.

Totals use the displayed means: `Parse + Post-process mean + Q × lookup mean`. Lookup costs are measured on a completed warm context; the first-lookup total is estimated, not timed immediately after parsing. The crossover is the first integer Q that beats the fastest Parse + Post-process + 1 lookup implementation.


### 50% split — Parse + Post-process + Lookup Hit — Known Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1218.82                           | 1.00×    | 16.97 ±0.13               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1539.19                           | 1.26×    | 22.42 ±0.29               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1579.48                           | 1.30×    | 21.42 ±0.13               | No crossover             |
| Actix Web (native) (prepared)         | 2330.27                           | 1.91×    | 6.70 ±0.17                | 110                      |
| H2O (native) (prepared) †             | 2334.36                           | 1.92×    | 4.71 ±0.24                | 92                       |
| Actix Web (native) (string)           | 2344.46                           | 1.92×    | 20.89 ±0.17               | No crossover             |
| H2O (native) (string)                 | 2344.86                           | 1.92×    | 15.21 ±0.26               | 641                      |
| H2O (SSE4.2) (prepared)               | 2425.53                           | 1.99×    | 5.44 ±0.06                | 106                      |
| H2O (SSE4.2) (string)                 | 2436.01                           | 2.00×    | 15.92 ±0.28               | 1161                     |
| Actix Web (scalar) (prepared)         | 2508.63                           | 2.06×    | 6.91 ±0.09                | 130                      |
| Actix Web (scalar) (string)           | 2523.32                           | 2.07×    | 21.60 ±0.25               | No crossover             |
| nginx (native) (prepared)             | 2750.65                           | 2.26×    | 2.82 ±0.02                | 110                      |
| nginx (native) (string)               | 2763.47                           | 2.27×    | 15.64 ±0.15               | 1163                     |
| nginx (scalar) (prepared)             | 2796.14                           | 2.29×    | 2.85 ±0.02                | 113                      |
| nginx (scalar) (string)               | 2808.81                           | 2.30×    | 15.52 ±0.10               | 1098                     |
| hwire + hwire_table (scalar) (string) | 3065.40                           | 2.52×    | 21.63 ±0.22               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3099.38                           | 2.54×    | 7.81 ±0.04                | 207                      |
| Actix Web (SSE4.2) (string)           | 3113.25                           | 2.55×    | 21.68 ±0.14               | No crossover             |
| H2O (scalar) (prepared)               | 3657.33                           | 3.00×    | 4.69 ±0.11                | 200                      |
| H2O (scalar) (string)                 | 3668.03                           | 3.01×    | 15.39 ±0.32               | 1552                     |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 50% split — Parse + Post-process + Lookup Hit — Unknown Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1221.47                           | 1.00×    | 19.62 ±0.17               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1542.62                           | 1.26×    | 25.85 ±0.12               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1583.92                           | 1.30×    | 25.86 ±0.15               | No crossover             |
| Actix Web (native) (prepared)         | 2333.76                           | 1.91×    | 10.19 ±0.07               | 119                      |
| H2O (native) (prepared)               | 2341.39                           | 1.92×    | 11.74 ±0.06               | 144                      |
| H2O (native) (string) †               | 2356.54                           | 1.93×    | 26.89 ±3.19               | No crossover             |
| Actix Web (native) (string)           | 2378.22                           | 1.95×    | 54.65 ±0.38               | No crossover             |
| H2O (SSE4.2) (prepared)               | 2432.64                           | 1.99×    | 12.55 ±0.41               | 173                      |
| H2O (SSE4.2) (string)                 | 2446.40                           | 2.00×    | 26.31 ±0.20               | No crossover             |
| Actix Web (scalar) (prepared)         | 2512.25                           | 2.06×    | 10.53 ±0.14               | 144                      |
| Actix Web (scalar) (string)           | 2555.09                           | 2.09×    | 53.37 ±0.54               | No crossover             |
| nginx (native) (prepared)             | 2758.61                           | 2.26×    | 10.78 ±0.20               | 175                      |
| nginx (native) (string)               | 2777.29                           | 2.27×    | 29.46 ±0.27               | No crossover             |
| nginx (scalar) (prepared)             | 2805.06                           | 2.30×    | 11.77 ±0.08               | 203                      |
| nginx (scalar) (string)               | 2821.22                           | 2.31×    | 27.93 ±0.16               | No crossover             |
| hwire + hwire_table (scalar) (string) | 3069.81                           | 2.51×    | 26.04 ±0.77               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3101.87                           | 2.54×    | 10.30 ±0.07               | 203                      |
| Actix Web (SSE4.2) (string)           | 3145.10                           | 2.57×    | 53.53 ±0.25               | No crossover             |
| H2O (scalar) (prepared)               | 3666.60                           | 3.00×    | 13.96 ±0.19               | 434                      |
| H2O (scalar) (string)                 | 3680.11                           | 3.01×    | 27.47 ±0.27               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 50% split — Parse + Post-process + Lookup Hit — Mixed Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1220.04                           | 1.00×    | 18.19 ±0.11               | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1542.48                           | 1.26×    | 25.71 ±0.23               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1581.74                           | 1.30×    | 23.68 ±0.14               | No crossover             |
| Actix Web (native) (prepared)         | 2332.09                           | 1.91×    | 8.52 ±0.06                | 117                      |
| H2O (native) (prepared)               | 2337.29                           | 1.92×    | 7.64 ±0.08                | 107                      |
| H2O (native) (string)                 | 2348.90                           | 1.93×    | 19.25 ±0.19               | No crossover             |
| Actix Web (native) (string)           | 2360.72                           | 1.93×    | 37.15 ±0.51               | No crossover             |
| H2O (SSE4.2) (prepared)               | 2428.76                           | 1.99×    | 8.67 ±0.21                | 128                      |
| H2O (SSE4.2) (string) †               | 2440.71                           | 2.00×    | 20.62 ±1.08               | No crossover             |
| Actix Web (scalar) (prepared)         | 2510.44                           | 2.06×    | 8.72 ±0.17                | 138                      |
| Actix Web (scalar) (string)           | 2537.95                           | 2.08×    | 36.23 ±0.59               | No crossover             |
| nginx (native) (prepared)             | 2753.17                           | 2.26×    | 5.34 ±0.11                | 121                      |
| nginx (native) (string)               | 2770.04                           | 2.27×    | 22.21 ±0.21               | No crossover             |
| nginx (scalar) (prepared)             | 2799.02                           | 2.29×    | 5.73 ±0.09                | 128                      |
| nginx (scalar) (string)               | 2814.99                           | 2.31×    | 21.70 ±0.46               | No crossover             |
| hwire + hwire_table (scalar) (string) | 3068.99                           | 2.52×    | 25.22 ±0.35               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3101.05                           | 2.54×    | 9.48 ±0.08                | 217                      |
| Actix Web (SSE4.2) (string)           | 3127.56                           | 2.56×    | 35.99 ±0.27               | No crossover             |
| H2O (scalar) (prepared)               | 3660.69                           | 3.00×    | 8.05 ±0.10                | 242                      |
| H2O (scalar) (string)                 | 3673.09                           | 3.01×    | 20.45 ±0.19               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 50% split — Parse + Post-process + Lookup Miss (calculated)

| Implementation                        | Parse + Post-process + 1 Miss (ns) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | ---------------------------------- | -------- | -------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1210.13                            | 1.00×    | 8.28 ±0.06                 | Baseline                 |
| hwire + hwire_table (SSE4.2) (string) | 1531.40                            | 1.27×    | 14.63 ±0.14                | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1572.35                            | 1.30×    | 14.29 ±0.47                | No crossover             |
| Actix Web (native) (prepared) †       | 2329.18                            | 1.92×    | 5.61 ±0.43                 | 421                      |
| H2O (native) (prepared)               | 2344.54                            | 1.94×    | 14.89 ±0.36                | No crossover             |
| H2O (native) (string)                 | 2357.60                            | 1.95×    | 27.95 ±0.28                | No crossover             |
| Actix Web (native) (string)           | 2367.36                            | 1.96×    | 43.79 ±1.07                | No crossover             |
| H2O (SSE4.2) (prepared) †             | 2435.30                            | 2.01×    | 15.21 ±0.59                | No crossover             |
| H2O (SSE4.2) (string)                 | 2450.66                            | 2.03×    | 30.57 ±0.23                | No crossover             |
| Actix Web (scalar) (prepared)         | 2507.48                            | 2.07×    | 5.76 ±0.13                 | 516                      |
| Actix Web (scalar) (string)           | 2545.74                            | 2.10×    | 44.02 ±0.40                | No crossover             |
| nginx (native) (prepared)             | 2764.73                            | 2.28×    | 16.90 ±0.47                | No crossover             |
| nginx (native) (string)               | 2777.79                            | 2.30×    | 29.96 ±0.34                | No crossover             |
| nginx (scalar) (prepared)             | 2810.08                            | 2.32×    | 16.79 ±0.33                | No crossover             |
| nginx (scalar) (string)               | 2821.17                            | 2.33×    | 27.88 ±0.29                | No crossover             |
| hwire + hwire_table (scalar) (string) | 3060.08                            | 2.53×    | 16.31 ±0.21                | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3097.11                            | 2.56×    | 5.54 ±0.05                 | 690                      |
| Actix Web (SSE4.2) (string)           | 3137.56                            | 2.59×    | 45.99 ±0.31                | No crossover             |
| H2O (scalar) (prepared)               | 3672.74                            | 3.03×    | 20.10 ±0.51                | No crossover             |
| H2O (scalar) (string)                 | 3685.74                            | 3.05×    | 33.10 ±0.44                | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### First Header Lookup Cost and Break-even — 90% split

Estimate the total time to parse and store a request and perform its first header lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher parsing and storage cost.

The parsing component uses the displayed 90% split Parse + Post-process mean: the cumulative time for both attempts, required storage reset and final conversion. Lookup costs reuse the displayed completed-context lookup means for this same input.

Totals use the displayed means: `Parse + Post-process mean + Q × lookup mean`. Lookup costs are measured on a completed warm context; the first-lookup total is estimated, not timed immediately after parsing. The crossover is the first integer Q that beats the fastest Parse + Post-process + 1 lookup implementation.


### 90% split — Parse + Post-process + Lookup Hit — Known Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1341.77                           | 1.00×    | 16.97 ±0.13               | Baseline                 |
| H2O (native) (prepared) †             | 1654.96                           | 1.23×    | 4.71 ±0.24                | 27                       |
| H2O (native) (string)                 | 1665.46                           | 1.24×    | 15.21 ±0.26               | 185                      |
| hwire + hwire_table (SSE4.2) (string) | 1674.40                           | 1.25×    | 22.42 ±0.29               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1695.23                           | 1.26×    | 21.42 ±0.13               | No crossover             |
| H2O (SSE4.2) (prepared)               | 1745.50                           | 1.30×    | 5.44 ±0.06                | 37                       |
| H2O (SSE4.2) (string)                 | 1755.98                           | 1.31×    | 15.92 ±0.28               | 396                      |
| Actix Web (native) (prepared)         | 2579.63                           | 1.92×    | 6.70 ±0.17                | 122                      |
| Actix Web (native) (string)           | 2593.82                           | 1.93×    | 20.89 ±0.17               | No crossover             |
| nginx (native) (prepared)             | 2751.72                           | 2.05×    | 2.82 ±0.02                | 101                      |
| nginx (native) (string)               | 2764.54                           | 2.06×    | 15.64 ±0.15               | 1071                     |
| Actix Web (scalar) (prepared)         | 2775.09                           | 2.07×    | 6.91 ±0.09                | 144                      |
| Actix Web (scalar) (string)           | 2789.78                           | 2.08×    | 21.60 ±0.25               | No crossover             |
| nginx (scalar) (prepared)             | 2792.70                           | 2.08×    | 2.85 ±0.02                | 104                      |
| nginx (scalar) (string)               | 2805.37                           | 2.09×    | 15.52 ±0.10               | 1011                     |
| H2O (scalar) (prepared)               | 3428.08                           | 2.55×    | 4.69 ±0.11                | 171                      |
| H2O (scalar) (string)                 | 3438.78                           | 2.56×    | 15.39 ±0.32               | 1329                     |
| Actix Web (SSE4.2) (prepared)         | 3569.92                           | 2.66×    | 7.81 ±0.04                | 245                      |
| Actix Web (SSE4.2) (string)           | 3583.79                           | 2.67×    | 21.68 ±0.14               | No crossover             |
| hwire + hwire_table (scalar) (string) | 3630.75                           | 2.71×    | 21.63 ±0.22               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 90% split — Parse + Post-process + Lookup Hit — Unknown Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1344.42                           | 1.00×    | 19.62 ±0.17               | Baseline                 |
| H2O (native) (prepared)               | 1661.99                           | 1.24×    | 11.74 ±0.06               | 42                       |
| H2O (native) (string) †               | 1677.14                           | 1.25×    | 26.89 ±3.19               | No crossover             |
| hwire + hwire_table (SSE4.2) (string) | 1677.83                           | 1.25×    | 25.85 ±0.12               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1699.67                           | 1.26×    | 25.86 ±0.15               | No crossover             |
| H2O (SSE4.2) (prepared)               | 1752.61                           | 1.30×    | 12.55 ±0.41               | 59                       |
| H2O (SSE4.2) (string)                 | 1766.37                           | 1.31×    | 26.31 ±0.20               | No crossover             |
| Actix Web (native) (prepared)         | 2583.12                           | 1.92×    | 10.19 ±0.07               | 133                      |
| Actix Web (native) (string)           | 2627.58                           | 1.95×    | 54.65 ±0.38               | No crossover             |
| nginx (native) (prepared)             | 2759.68                           | 2.05×    | 10.78 ±0.20               | 162                      |
| nginx (native) (string)               | 2778.36                           | 2.07×    | 29.46 ±0.27               | No crossover             |
| Actix Web (scalar) (prepared)         | 2778.71                           | 2.07×    | 10.53 ±0.14               | 159                      |
| nginx (scalar) (prepared)             | 2801.62                           | 2.08×    | 11.77 ±0.08               | 187                      |
| nginx (scalar) (string)               | 2817.78                           | 2.10×    | 27.93 ±0.16               | No crossover             |
| Actix Web (scalar) (string)           | 2821.55                           | 2.10×    | 53.37 ±0.54               | No crossover             |
| H2O (scalar) (prepared)               | 3437.35                           | 2.56×    | 13.96 ±0.19               | 371                      |
| H2O (scalar) (string)                 | 3450.86                           | 2.57×    | 27.47 ±0.27               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3572.41                           | 2.66×    | 10.30 ±0.07               | 241                      |
| Actix Web (SSE4.2) (string)           | 3615.64                           | 2.69×    | 53.53 ±0.25               | No crossover             |
| hwire + hwire_table (scalar) (string) | 3635.16                           | 2.70×    | 26.04 ±0.77               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 90% split — Parse + Post-process + Lookup Hit — Mixed Headers (calculated)

| Implementation                        | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1342.99                           | 1.00×    | 18.19 ±0.11               | Baseline                 |
| H2O (native) (prepared)               | 1657.89                           | 1.23×    | 7.64 ±0.08                | 31                       |
| H2O (native) (string)                 | 1669.50                           | 1.24×    | 19.25 ±0.19               | No crossover             |
| hwire + hwire_table (SSE4.2) (string) | 1677.69                           | 1.25×    | 25.71 ±0.23               | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1697.49                           | 1.26×    | 23.68 ±0.14               | No crossover             |
| H2O (SSE4.2) (prepared)               | 1748.73                           | 1.30×    | 8.67 ±0.21                | 44                       |
| H2O (SSE4.2) (string) †               | 1760.68                           | 1.31×    | 20.62 ±1.08               | No crossover             |
| Actix Web (native) (prepared)         | 2581.45                           | 1.92×    | 8.52 ±0.06                | 130                      |
| Actix Web (native) (string)           | 2610.08                           | 1.94×    | 37.15 ±0.51               | No crossover             |
| nginx (native) (prepared)             | 2754.24                           | 2.05×    | 5.34 ±0.11                | 111                      |
| nginx (native) (string)               | 2771.11                           | 2.06×    | 22.21 ±0.21               | No crossover             |
| Actix Web (scalar) (prepared)         | 2776.90                           | 2.07×    | 8.72 ±0.17                | 153                      |
| nginx (scalar) (prepared)             | 2795.58                           | 2.08×    | 5.73 ±0.09                | 118                      |
| Actix Web (scalar) (string)           | 2804.41                           | 2.09×    | 36.23 ±0.59               | No crossover             |
| nginx (scalar) (string)               | 2811.55                           | 2.09×    | 21.70 ±0.46               | No crossover             |
| H2O (scalar) (prepared)               | 3431.44                           | 2.56×    | 8.05 ±0.10                | 207                      |
| H2O (scalar) (string)                 | 3443.84                           | 2.56×    | 20.45 ±0.19               | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3571.59                           | 2.66×    | 9.48 ±0.08                | 257                      |
| Actix Web (SSE4.2) (string)           | 3598.10                           | 2.68×    | 35.99 ±0.27               | No crossover             |
| hwire + hwire_table (scalar) (string) | 3634.34                           | 2.71×    | 25.22 ±0.35               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


### 90% split — Parse + Post-process + Lookup Miss (calculated)

| Implementation                        | Parse + Post-process + 1 Miss (ns) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ------------------------------------- | ---------------------------------- | -------- | -------------------------- | ------------------------ |
| hwire + hwire_table (native) (string) | 1333.08                            | 1.00×    | 8.28 ±0.06                 | Baseline                 |
| H2O (native) (prepared)               | 1665.14                            | 1.25×    | 14.89 ±0.36                | No crossover             |
| hwire + hwire_table (SSE4.2) (string) | 1666.61                            | 1.25×    | 14.63 ±0.14                | No crossover             |
| H2O (native) (string)                 | 1678.20                            | 1.26×    | 27.95 ±0.28                | No crossover             |
| hwire + hwire_table (SSE2) (string)   | 1688.10                            | 1.27×    | 14.29 ±0.47                | No crossover             |
| H2O (SSE4.2) (prepared) †             | 1755.27                            | 1.32×    | 15.21 ±0.59                | No crossover             |
| H2O (SSE4.2) (string)                 | 1770.63                            | 1.33×    | 30.57 ±0.23                | No crossover             |
| Actix Web (native) (prepared) †       | 2578.54                            | 1.93×    | 5.61 ±0.43                 | 468                      |
| Actix Web (native) (string)           | 2616.72                            | 1.96×    | 43.79 ±1.07                | No crossover             |
| nginx (native) (prepared)             | 2765.80                            | 2.07×    | 16.90 ±0.47                | No crossover             |
| Actix Web (scalar) (prepared)         | 2773.94                            | 2.08×    | 5.76 ±0.13                 | 573                      |
| nginx (native) (string)               | 2778.86                            | 2.08×    | 29.96 ±0.34                | No crossover             |
| nginx (scalar) (prepared)             | 2806.64                            | 2.11×    | 16.79 ±0.33                | No crossover             |
| Actix Web (scalar) (string)           | 2812.20                            | 2.11×    | 44.02 ±0.40                | No crossover             |
| nginx (scalar) (string)               | 2817.73                            | 2.11×    | 27.88 ±0.29                | No crossover             |
| H2O (scalar) (prepared)               | 3443.49                            | 2.58×    | 20.10 ±0.51                | No crossover             |
| H2O (scalar) (string)                 | 3456.49                            | 2.59×    | 33.10 ±0.44                | No crossover             |
| Actix Web (SSE4.2) (prepared)         | 3567.65                            | 2.68×    | 5.54 ±0.05                 | 817                      |
| Actix Web (SSE4.2) (string)           | 3608.10                            | 2.71×    | 45.99 ±0.31                | No crossover             |
| hwire + hwire_table (scalar) (string) | 3625.43                            | 2.72×    | 16.31 ±0.21                | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native) (string). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.
