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
| `<name>_VARIANTS` | Supported variants; defaults to the requested variants when omitted |
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
date                 : 2026-10-08T07:56:55+09:00
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
String lookup calls `hwire_table_get_ci` directly.

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


### Header Lookup

Each lookup starts from the original header-name string; native name conversion
and search are included in timing. Lookup time is the mean per search on a
completed warm context.

Known/unknown describes native header-name definitions. Unknown hit names are
present in the message but absent from those definitions; hwire_table treats
all names as strings. Key lengths differ between groups, so costs include both
name representation and key length.

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

Supported CPU builds; memory preallocated (system allocation excluded); application header limit 128; headers only.

Parse + Post-process includes initialization, HTTP parsing and native header storage, including any growth. Query decomposition, decoding and storage are excluded. Input preparation and context cleanup are outside timing.

† Target RCIW was not reached; calculated totals inherit the marker from either component.


#### Parse + Post-process

| Implementation               | Mean ± SD (ns/request) | Relative | Throughput   | RCIW  |
| ---------------------------- | ---------------------- | -------- | ------------ | ----- |
| hwire + hwire_table (native) | 500.07 ±3.42           | 1.00×    | 2.00 M req/s | 0.96% |
| hwire + hwire_table (SSE4.2) | 679.97 ±4.46           | 1.36×    | 1.47 M req/s | 0.92% |
| hwire + hwire_table (SSE2)   | 748.21 ±10.38          | 1.50×    | 1.34 M req/s | 1.94% |
| H2O (native)                 | 797.94 ±30.40 †        | 1.60×    | 1.25 M req/s | 2.16% |
| hwire + hwire_table (scalar) | 920.29 ±16.55          | 1.84×    | 1.09 M req/s | 1.97% |
| H2O (SSE4.2)                 | 935.15 ±20.03          | 1.87×    | 1.07 M req/s | 1.76% |
| H2O (scalar)                 | 1012.05 ±32.17         | 2.02×    | 0.99 M req/s | 1.90% |
| nginx (scalar)               | 1091.55 ±4.47          | 2.18×    | 0.92 M req/s | 0.57% |
| nginx (native)               | 1184.46 ±9.62          | 2.37×    | 0.84 M req/s | 1.14% |
| Actix Web (native)           | 1351.58 ±15.90         | 2.70×    | 0.74 M req/s | 1.65% |
| Actix Web (SSE4.2)           | 1467.85 ±10.84         | 2.94×    | 0.68 M req/s | 1.03% |
| Actix Web (scalar)           | 1486.81 ±12.38         | 2.97×    | 0.67 M req/s | 1.16% |


#### Lookup Hit — Known Headers

| Implementation               | Mean ± SD (ns/lookup) | Relative | Throughput        | RCIW  |
| ---------------------------- | --------------------- | -------- | ----------------- | ----- |
| nginx (native)               | 15.26 ±0.05           | 1.00×    | 65.53 M lookups/s | 0.42% |
| nginx (scalar)               | 15.46 ±0.07           | 1.01×    | 64.68 M lookups/s | 0.67% |
| H2O (native)                 | 16.00 ±0.07           | 1.05×    | 62.50 M lookups/s | 0.58% |
| H2O (SSE4.2)                 | 16.75 ±0.51           | 1.10×    | 59.70 M lookups/s | 1.95% |
| hwire + hwire_table (native) | 16.94 ±0.06           | 1.11×    | 59.03 M lookups/s | 0.47% |
| H2O (scalar)                 | 17.61 ±0.35           | 1.15×    | 56.79 M lookups/s | 1.86% |
| Actix Web (native)           | 20.68 ±0.14           | 1.36×    | 48.36 M lookups/s | 0.98% |
| hwire + hwire_table (scalar) | 21.25 ±0.08           | 1.39×    | 47.06 M lookups/s | 0.52% |
| hwire + hwire_table (SSE4.2) | 21.34 ±0.07           | 1.40×    | 46.86 M lookups/s | 0.43% |
| Actix Web (scalar)           | 21.39 ±0.12           | 1.40×    | 46.75 M lookups/s | 0.80% |
| Actix Web (SSE4.2)           | 21.86 ±0.12           | 1.43×    | 45.75 M lookups/s | 0.79% |
| hwire + hwire_table (SSE2)   | 24.34 ±0.06           | 1.60×    | 41.08 M lookups/s | 0.36% |

Searches Host, Accept, Cookie, User-Agent, Connection and Referer, in that order, repeated with equal frequency.


#### Lookup Hit — Unknown Headers

| Implementation               | Mean ± SD (ns/lookup) | Relative | Throughput        | RCIW  |
| ---------------------------- | --------------------- | -------- | ----------------- | ----- |
| hwire + hwire_table (native) | 19.77 ±0.10           | 1.00×    | 50.58 M lookups/s | 0.67% |
| H2O (SSE4.2)                 | 24.57 ±0.30           | 1.24×    | 40.70 M lookups/s | 1.71% |
| H2O (native)                 | 24.80 ±0.10           | 1.25×    | 40.32 M lookups/s | 0.59% |
| hwire + hwire_table (scalar) | 25.61 ±0.17           | 1.30×    | 39.05 M lookups/s | 0.90% |
| hwire + hwire_table (SSE4.2) | 25.66 ±0.13           | 1.30×    | 38.97 M lookups/s | 0.71% |
| H2O (scalar)                 | 27.01 ±0.27           | 1.37×    | 37.02 M lookups/s | 1.41% |
| nginx (scalar)               | 28.43 ±0.15           | 1.44×    | 35.17 M lookups/s | 0.75% |
| hwire + hwire_table (SSE2)   | 29.30 ±0.10           | 1.48×    | 34.13 M lookups/s | 0.47% |
| nginx (native)               | 31.20 ±0.18           | 1.58×    | 32.05 M lookups/s | 0.80% |
| Actix Web (native)           | 52.54 ±0.22           | 2.66×    | 19.03 M lookups/s | 0.59% |
| Actix Web (SSE4.2)           | 53.10 ±0.28           | 2.69×    | 18.83 M lookups/s | 0.73% |
| Actix Web (scalar)           | 53.40 ±0.18           | 2.70×    | 18.73 M lookups/s | 0.47% |

Searches Sec-Fetch-Site, Sec-Fetch-Mode, Sec-Fetch-User, Sec-Fetch-Dest, Sec-CH-UA and Sec-CH-UA-Platform, in that order, repeated with equal frequency.


#### Lookup Hit — Mixed Headers

| Implementation               | Mean ± SD (ns/lookup) | Relative | Throughput        | RCIW  |
| ---------------------------- | --------------------- | -------- | ----------------- | ----- |
| hwire + hwire_table (native) | 18.36 ±0.14           | 1.00×    | 54.47 M lookups/s | 1.06% |
| H2O (SSE4.2)                 | 20.03 ±0.15           | 1.09×    | 49.93 M lookups/s | 1.05% |
| H2O (native)                 | 20.49 ±0.14           | 1.12×    | 48.80 M lookups/s | 0.95% |
| nginx (scalar)               | 21.63 ±0.13           | 1.18×    | 46.23 M lookups/s | 0.82% |
| H2O (scalar)                 | 21.74 ±0.15           | 1.18×    | 46.00 M lookups/s | 0.95% |
| nginx (native)               | 22.49 ±0.18           | 1.22×    | 44.46 M lookups/s | 1.10% |
| hwire + hwire_table (SSE4.2) | 23.57 ±0.11           | 1.28×    | 42.43 M lookups/s | 0.65% |
| hwire + hwire_table (scalar) | 23.61 ±0.17           | 1.29×    | 42.35 M lookups/s | 0.98% |
| hwire + hwire_table (SSE2)   | 28.22 ±0.67           | 1.54×    | 35.44 M lookups/s | 1.95% |
| Actix Web (native)           | 36.05 ±0.27           | 1.96×    | 27.74 M lookups/s | 1.06% |
| Actix Web (SSE4.2)           | 36.23 ±0.27           | 1.97×    | 27.60 M lookups/s | 1.05% |
| Actix Web (scalar)           | 37.54 ±0.26           | 2.04×    | 26.64 M lookups/s | 0.98% |

Searches Host, Sec-Fetch-Site, Cookie, Sec-Fetch-Mode, Connection and Sec-CH-UA-Platform, in that order, repeated with equal frequency.


#### Lookup Miss

| Implementation               | Mean ± SD (ns/lookup) | Relative | Throughput         | RCIW  |
| ---------------------------- | --------------------- | -------- | ------------------ | ----- |
| hwire + hwire_table (native) | 8.21 ±0.07            | 1.00×    | 121.80 M lookups/s | 1.25% |
| hwire + hwire_table (SSE4.2) | 14.39 ±0.06           | 1.75×    | 69.49 M lookups/s  | 0.62% |
| hwire + hwire_table (scalar) | 14.43 ±0.09           | 1.76×    | 69.30 M lookups/s  | 0.91% |
| hwire + hwire_table (SSE2)   | 15.38 ±0.18           | 1.87×    | 65.02 M lookups/s  | 1.66% |
| H2O (scalar)                 | 28.26 ±0.27           | 3.44×    | 35.39 M lookups/s  | 1.33% |
| H2O (native)                 | 28.41 ±0.23           | 3.46×    | 35.20 M lookups/s  | 1.11% |
| H2O (SSE4.2)                 | 29.49 ±0.33           | 3.59×    | 33.91 M lookups/s  | 1.58% |
| nginx (native)               | 30.46 ±0.18           | 3.71×    | 32.83 M lookups/s  | 0.82% |
| nginx (scalar)               | 32.78 ±0.34           | 3.99×    | 30.51 M lookups/s  | 1.46% |
| Actix Web (native)           | 44.87 ±0.30           | 5.47×    | 22.29 M lookups/s  | 0.94% |
| Actix Web (scalar)           | 45.12 ±0.30           | 5.50×    | 22.16 M lookups/s  | 0.93% |
| Actix Web (SSE4.2)           | 45.75 ±0.31           | 5.57×    | 21.86 M lookups/s  | 0.95% |

Searches Hots, Accpet, Cooxie, User-Agend, Sec-CH-UA-Platforn and Referef, in that order, repeated with equal frequency.


### First Header Lookup Cost and Break-even

Estimate the total time to parse and store a request and perform its first header lookup. Each table shows that total, the per-lookup cost, and how many lookups are needed for faster searches to recover a higher parsing and storage cost.

Totals use the displayed means: `Parse + Post-process mean + Q × lookup mean`. Lookup costs are measured on a completed warm context; the first-lookup total is estimated, not timed immediately after parsing. The crossover is the first integer Q that beats the fastest Parse + Post-process + 1 lookup implementation.


#### Parse + Post-process + Lookup Hit — Known Headers (calculated)

| Implementation               | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ---------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) | 517.01                            | 1.00×    | 16.94 ±0.06               | Baseline                 |
| hwire + hwire_table (SSE4.2) | 701.31                            | 1.36×    | 21.34 ±0.07               | No crossover             |
| hwire + hwire_table (SSE2)   | 772.55                            | 1.49×    | 24.34 ±0.06               | No crossover             |
| H2O (native) †               | 813.94                            | 1.57×    | 16.00 ±0.07               | 317                      |
| hwire + hwire_table (scalar) | 941.54                            | 1.82×    | 21.25 ±0.08               | No crossover             |
| H2O (SSE4.2)                 | 951.90                            | 1.84×    | 16.75 ±0.51               | 2290                     |
| H2O (scalar)                 | 1029.66                           | 1.99×    | 17.61 ±0.35               | No crossover             |
| nginx (scalar)               | 1107.01                           | 2.14×    | 15.46 ±0.07               | 400                      |
| nginx (native)               | 1199.72                           | 2.32×    | 15.26 ±0.05               | 408                      |
| Actix Web (native)           | 1372.26                           | 2.65×    | 20.68 ±0.14               | No crossover             |
| Actix Web (SSE4.2)           | 1489.71                           | 2.88×    | 21.86 ±0.12               | No crossover             |
| Actix Web (scalar)           | 1508.20                           | 2.92×    | 21.39 ±0.12               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


#### Parse + Post-process + Lookup Hit — Unknown Headers (calculated)

| Implementation               | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ---------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) | 519.84                            | 1.00×    | 19.77 ±0.10               | Baseline                 |
| hwire + hwire_table (SSE4.2) | 705.63                            | 1.36×    | 25.66 ±0.13               | No crossover             |
| hwire + hwire_table (SSE2)   | 777.51                            | 1.50×    | 29.30 ±0.10               | No crossover             |
| H2O (native) †               | 822.74                            | 1.58×    | 24.80 ±0.10               | No crossover             |
| hwire + hwire_table (scalar) | 945.90                            | 1.82×    | 25.61 ±0.17               | No crossover             |
| H2O (SSE4.2)                 | 959.72                            | 1.85×    | 24.57 ±0.30               | No crossover             |
| H2O (scalar)                 | 1039.06                           | 2.00×    | 27.01 ±0.27               | No crossover             |
| nginx (scalar)               | 1119.98                           | 2.15×    | 28.43 ±0.15               | No crossover             |
| nginx (native)               | 1215.66                           | 2.34×    | 31.20 ±0.18               | No crossover             |
| Actix Web (native)           | 1404.12                           | 2.70×    | 52.54 ±0.22               | No crossover             |
| Actix Web (SSE4.2)           | 1520.95                           | 2.93×    | 53.10 ±0.28               | No crossover             |
| Actix Web (scalar)           | 1540.21                           | 2.96×    | 53.40 ±0.18               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


#### Parse + Post-process + Lookup Hit — Mixed Headers (calculated)

| Implementation               | Parse + Post-process + 1 Hit (ns) | Relative | Hit Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ---------------------------- | --------------------------------- | -------- | ------------------------- | ------------------------ |
| hwire + hwire_table (native) | 518.43                            | 1.00×    | 18.36 ±0.14               | Baseline                 |
| hwire + hwire_table (SSE4.2) | 703.54                            | 1.36×    | 23.57 ±0.11               | No crossover             |
| hwire + hwire_table (SSE2)   | 776.43                            | 1.50×    | 28.22 ±0.67               | No crossover             |
| H2O (native) †               | 818.43                            | 1.58×    | 20.49 ±0.14               | No crossover             |
| hwire + hwire_table (scalar) | 943.90                            | 1.82×    | 23.61 ±0.17               | No crossover             |
| H2O (SSE4.2)                 | 955.18                            | 1.84×    | 20.03 ±0.15               | No crossover             |
| H2O (scalar)                 | 1033.79                           | 1.99×    | 21.74 ±0.15               | No crossover             |
| nginx (scalar)               | 1113.18                           | 2.15×    | 21.63 ±0.13               | No crossover             |
| nginx (native)               | 1206.95                           | 2.33×    | 22.49 ±0.18               | No crossover             |
| Actix Web (native)           | 1387.63                           | 2.68×    | 36.05 ±0.27               | No crossover             |
| Actix Web (SSE4.2)           | 1504.08                           | 2.90×    | 36.23 ±0.27               | No crossover             |
| Actix Web (scalar)           | 1524.35                           | 2.94×    | 37.54 ±0.26               | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.


#### Parse + Post-process + Lookup Miss (calculated)

| Implementation               | Parse + Post-process + 1 Miss (ns) | Relative | Miss Mean ± SD (ns/lookup) | Lookups to beat baseline |
| ---------------------------- | ---------------------------------- | -------- | -------------------------- | ------------------------ |
| hwire + hwire_table (native) | 508.28                             | 1.00×    | 8.21 ±0.07                 | Baseline                 |
| hwire + hwire_table (SSE4.2) | 694.36                             | 1.37×    | 14.39 ±0.06                | No crossover             |
| hwire + hwire_table (SSE2)   | 763.59                             | 1.50×    | 15.38 ±0.18                | No crossover             |
| H2O (native) †               | 826.35                             | 1.63×    | 28.41 ±0.23                | No crossover             |
| hwire + hwire_table (scalar) | 934.72                             | 1.84×    | 14.43 ±0.09                | No crossover             |
| H2O (SSE4.2)                 | 964.64                             | 1.90×    | 29.49 ±0.33                | No crossover             |
| H2O (scalar)                 | 1040.31                            | 2.05×    | 28.26 ±0.27                | No crossover             |
| nginx (scalar)               | 1124.33                            | 2.21×    | 32.78 ±0.34                | No crossover             |
| nginx (native)               | 1214.92                            | 2.39×    | 30.46 ±0.18                | No crossover             |
| Actix Web (native)           | 1396.45                            | 2.75×    | 44.87 ±0.30                | No crossover             |
| Actix Web (SSE4.2)           | 1513.60                            | 2.98×    | 45.75 ±0.31                | No crossover             |
| Actix Web (scalar)           | 1531.93                            | 3.01×    | 45.12 ±0.30                | No crossover             |

Parse + Post-process + 1 lookup baseline: hwire + hwire_table (native). Each Relative uses the fastest total in its column.
No crossover: it cannot overtake under this model. The crossover is the first integer Q giving a strictly lower total.
