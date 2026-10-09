# Benchmarking Parsers

Measure HTTP/1.1 start-line and header parsing. Native header storage and
lookup are measured separately in [Production](../production/README.md).


## Workloads

Inputs are pre-generated synthetic header blocks under
[`../data/`](../data/README.md). Bodies are excluded. Every parser receives the
same message bytes for a fixture; input preparation is outside timing.

| Plain fixture | Scenario |
| --- | --- |
| Browser GET request | Navigation with client hints, fetch metadata, Cookie and four query parameters |
| S3 API request | PutObject with authentication, checksum, encryption, tagging and metadata fields |
| Browser response | HTML response with validators, security policies and repeated Set-Cookie/Link fields |
| No Content response | Minimal 204 acknowledgement |

All supplied plain adapters stop after the start line and headers.
Plain parsing does not decode
query parameters or copy header pairs into application storage.


## Metrics

| Metric | Meaning |
| --- | --- |
| Mean ± SD | Mean time per operation ± sample standard deviation; each sample averages repeated operations |
| Relative | Mean / fastest mean for the same fixture across parser build configurations; 1.00× is the baseline and 1.20× means 20% more time |
| Throughput | Operations per second, calculated as 1 second / mean operation time |
| RCIW | Full confidence interval width / mean; describes uncertainty in the estimated mean |
| Message bytes | Input length through the header terminator, including CRLF and excluding the C string terminator |

Plain parsing measures initialization and parsing through the header terminator.
Adapters allocate no per-message context. Most use stack state; milo reuses
its native parser and event buffer, with reset included in timing. Cleanup is
outside timing.


### Sampling

Sampling starts with 20 samples and checks every 10 up to 100. Target RCIW
is 2%, using Student-t intervals with Bonferroni correction over nine stopping
points. Warmup calibrates about 1 ms of total work per sample, including timer
and cleanup overhead. Each message operation has its own timer interval;
samples average the measured operation times. Empty timer intervals are
measured and subtracted for each sample. Warmup and calibration are excluded
from sample statistics. Progress includes sample counts and achieved RCIW;
unmet targets are marked in the report.


## Adding a Benchmark Target

Add `<name>/request.c` and `<name>/response.c` under this suite. The directory
name must match `[A-Za-z][A-Za-z0-9_]*`. Directories prefixed with `_` are disabled.
The suite's registration script discovers exported functions and generates the
included registration table. C declarations of native Rust- or Zig-exported functions are also
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
  environment, together with `ZIG` for Zig adapters. Native Rust builds use locked dependencies and run offline.
- Ignore downloaded sources and native build caches. Disabled implementations
  contribute no sources, configuration, setup or native builds.

| Variable | Meaning |
| --- | --- |
| `<name>_SOURCES` | Additional native translation units; adapter `*.c` files are discovered automatically |
| `<name>_CPPFLAGS` | Include paths and preprocessor flags |
| `<name>_CFLAGS` | Compilation flags for sources under the implementation directory |
| `<name>_LDLIBS` | Link flags; `%` expands to the build variant |
| `<name>_PARSE_NAME` | Report display name; defaults to the directory name |
| `<name>_BUILD_INFO` | Native toolchain/build description recorded in the measurement metadata |
| `<name>_BUILD_DEPS` | Native source/manifests that trigger rebuilding |
| `<name>_VARIANTS` | Supported build targets, such as `nosimd sse42 native`; only those adapters are registered, compiled and measured for each target. Defaults to the suite targets when omitted |
| `<name>_ENV` | Space-separated `NAME=value` assignments (shell quoting supported) for this adapter’s setup, fetch, build and execution |

### Entry points

The caller supplies valid fixture bytes; omit argument validation and result
validation in timed adapters. Return zero on success and nonzero on failure.
The caller initializes the context pointer to NULL before invoking an entry
point and retains the input until context cleanup. Cleanup is outside timing.

```c
/**
 * Parse a request start line and headers through the header terminator.
 * Called once per timed message during warmup, calibration and sampling.
 * Use native stack state or reusable state; allocate no per-message context.
 * @param context Optional owned output; caller initializes it to NULL.
 * @param data Valid input retained until context cleanup.
 * @param len Input byte length.
 * @return Zero on success; nonzero on parse failure.
 */
int name_request(void **context, const unsigned char *data, size_t len);

/** Parse a response status line and headers under the same timing contract. */
int name_response(void **context, const unsigned char *data, size_t len);

/**
 * Release owned output after the timer stops; accept NULL as a no-op.
 * Stack-only adapters retain no output and implement an empty cleanup function.
 */
void name_context_free(void *context);
```


# Benchmark

<!-- benchmark-environment -->
## Environment

```text
date                 : 2026-10-08T07:19:59+09:00
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
- Cargo and rustc 1.88+: Required when Rust adapters are enabled.
- Zig 0.15.1: Required for hparse; newer Zig releases are not API-compatible with the pinned source.
- `curl`, `tar` and network access: Fetch pinned dependencies during initial setup.

Dependencies are fetched before timing. Repeated setup reuses downloaded
revisions; each target owns its `fetch.sh` and `config.mk`.


## Commands

Run from `bench/parsers/`.

| Command | Action |
| --- | --- |
| `make` / `make run` | Build, measure and report plain parsing |
| `make setup` | Fetch pinned dependencies for active implementations |
| `make build` | Build the active parser variants |
| `make request` / `make response` | Measure one direction |
| `make nosimd` / `make sse2` / `make sse42` / `make neon` / `make native` | Measure one supported variant |
| `make report` | Render saved measurements |
| `make update-readme` | Publish saved measurements in this README |
| `make check` | Run registration/report and adapter checks outside timing |
| `make list` | List implementations and variants |
| `make clean` | Remove timing binaries/results; retain dependency caches |

Downloads live in each
implementation's ignored `deps/`. Raw results and metadata live in `results/`;
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

Release builds use `-O2 -DNDEBUG -std=c11`. The platform files record the
compiler, flags and measurement environment. Published results use the x86
reference system.

| Plain build | Target | Additional flags |
| --- | --- | --- |
| `nosimd` | Implementations with a scalar switch | Scalar parser; compiler loop/SLP vectorization disabled |
| `sse2` | x86-64 implementations with an SSE2 path | Compiler's default x86-64 target |
| `sse42` | x86-64 implementations with an SSE4.2 path | `-msse4.2` |
| `neon` | ARM implementations with a NEON path | Compiler's default ARM target |
| `native` | All active implementations | `-march=native` (C), `-C target-cpu=native` (Rust), `-mcpu=native` (Zig) |

Each implementation registers its supported configurations in `config.mk`.
Scalar settings disable parser SIMD and compiler loop/SLP vectorization; native
settings enable host CPU features. Separate SSE2/SSE4.2 or NEON paths are
measured where the parser supplies them. Parsers without a scalar switch are
listed only under their supported configuration. Shared system-library calls
retain their platform implementations.


## Benchmark Targets

Compiler and flag entries below record the published measurements.

These targets compare the repository parser with callback-driven, array-producing
and server-native HTTP parsing APIs. Each adapter measures start-line and header
parsing using native state.

### hwire

A callback-driven parser that exposes request/header slices to the application.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](hwire/request.c), [response.c](hwire/response.c)
- Library: Current sources in `../../src/`
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags -->
  - `nosimd`: `-DHWIRE_NO_SIMD -fno-tree-vectorize`; disables explicit parser SIMD and compiler loop/SLP vectorization.
  - `sse2`: Default x86-64 target; enables hwire's SSE2 parser path.
  - `sse42`: `-msse4.2`; enables the SSE4.2 parser path.
  - `neon` (ARM): Default ARM compiler target; enables the NEON parser path.
  - `native`: `-march=native`; enables host CPU features and the fastest available parser instruction path.
- Build: `C`; scalar, SSE2/SSE4.2 on x86-64, NEON on ARM, and native CPU tuning

The `nosimd` configuration disables explicit parser SIMD with `-DHWIRE_NO_SIMD`.

</details>


### H2O

Measures H2O's bundled picohttpparser, which writes header slices into a caller-supplied array.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](h2o/request.c), [response.c](h2o/response.c)
- Library: [picohttpparser bundled with H2O](https://github.com/h2o/h2o/tree/5da50541a4b6a038c9cea493f740747cf964f4a8/deps/picohttpparser), H2O revision `5da50541a4b6a038c9cea493f740747cf964f4a8`
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags -->
  - `nosimd`: `-fno-tree-vectorize`; uses the scalar parser path.
  - `sse42` (x86-64): `-msse4.2`; enables the SSE4.2 parser path.
  - `native`: `-march=native`; uses SSE4.2 where supported.
- Build: `C`; scalar and SSE4.2 parser paths; no SSE2 or NEON parser path

</details>


### llhttp (Node.js)

A callback-driven HTTP parser used by Node.js.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](llhttp/request.c), [response.c](llhttp/response.c)
- Library: [llhttp 9.4.3](https://github.com/nodejs/llhttp/tree/release/v9.4.3), revision `0e815792b167a9bd8ace259b95b7da953776c288`
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags -->
  - `nosimd`: `-fno-tree-vectorize`; uses the scalar parser path.
  - `sse42` (x86-64): `-msse4.2`; enables generated SSE4.2 scanning paths.
  - `native`: `-march=native`; enables host CPU features.
- Build: `C`; scalar and SSE4.2 parser paths

The adapter pauses in its headers-complete callback, before body parsing.

</details>


### nginx

The native nginx request-line, status-line and header-line parser.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](nginx/request.c), [response.c](nginx/response.c)
- Library: [nginx 1.31.6](https://github.com/nginx/nginx/tree/release-1.31.6), revision `45a318d05a0fd23f57ffe9579f7f0969c0fe402a`
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags --> `-ffunction-sections -fdata-sections`
  - `nosimd`: `-fno-tree-vectorize`; disables compiler loop/SLP vectorization.
  - `native`: `-march=native`; applies host CPU tuning.
- Build: `C`; `--with-compat`; no explicit parser SIMD implementation

The adapter calls unmodified upstream parsing functions with configure-generated
headers and types. Native header hashing and lowercase-name buffering remain
part of the parse cost; application header storage is excluded.

</details>


### Actix Web

Measures httparse, the HTTP/1 parser used by Actix Web, with header slices written into a caller-supplied array.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](actix_web/request.c), [response.c](actix_web/response.c), [parser.rs](actix_web/parser.rs)
- Library: [httparse 1.10.1](https://crates.io/crates/httparse/1.10.1), fetched by Cargo and pinned in `Cargo.lock`
- Compiler: <!-- compiler:actix_web -->`rustc 1.93.1 (01f6ddf75 2026-02-11)`<!-- /compiler -->; C ABI shim: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags --> (C ABI shim)
  - `nosimd`: `-fno-tree-vectorize` for the C declaration files.
  - `sse42` (x86-64): `-msse4.2`.
  - `neon` (ARM): Default ARM target.
  - `native`: `-march=native`.
- RUSTFLAGS:
  - `nosimd`: `-C target-cpu=generic -C no-vectorize-loops -C no-vectorize-slp`;
    `CARGO_CFG_HTTPARSE_DISABLE_SIMD=1` disables httparse SIMD.
  - `sse42` (x86-64): `-C target-cpu=generic -C target-feature=+sse4.2,-avx2`.
  - `neon` (ARM): `-C target-cpu=generic -C target-feature=+neon`.
  - `native`: `-C target-cpu=native`; enables AVX2 on the measurement CPU.
- Build: <!-- build:actix_web -->opt-level=2; panic=abort; scalar for nosimd, compile-time SSE4.2 for sse42, NEON for neon, host CPU including AVX2 for native<!-- /build -->

Rust exports the registration ABI directly. Its entry-point call is timed.
The native configuration enables httparse's AVX2 path when supported by the CPU.

</details>


### hparse

A Zig parser that uses compiler-selected SIMD and writes borrowed header slices
into a caller-supplied array.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](hparse/request.c), [response.c](hparse/response.c), [parser.zig](hparse/parser.zig)
- Library: [hparse](https://github.com/nikneym/hparse/tree/4fb3d836e804eab0469bb128f521ad00b1085731), revision `4fb3d836e804eab0469bb128f521ad00b1085731`
- Compiler: Zig 0.15.1; override its executable with `make ZIG=/path/to/zig`.
- Build: `ReleaseFast`; native C ABI exports linked into the C driver.
  - `nosimd`: `baseline-sse-sse2` on x86-64 or `baseline-neon` on ARM.
  - `sse2`: `baseline`.
  - `neon` (ARM): `baseline+neon`.
  - `native`: `native`; enables host CPU features, including AVX2 where available.

The adapter uses a 100-element stack header array for requests and responses.
Initialization and parsing are timed. Output barriers preserve the parsed values.
The upstream source is unmodified; its `std.simd.suggestVectorLength` selects
the vector path from the compiler target. Zig's ReleaseFast optimization differs
from the C adapters' `-O2` and is recorded explicitly.

</details>


### milo

A Rust HTTP/1.1 parser developed as a candidate replacement for llhttp.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](milo/request.c), [response.c](milo/response.c), [parser.rs](milo/parser.rs)
- Library: [milo 0.9.0](https://github.com/ShogunPanda/milo/tree/0cd8b8151c0a258352fe328f95cb4f5e85837223), revision `0cd8b8151c0a258352fe328f95cb4f5e85837223`; Cargo dependencies pinned in `Cargo.lock`.
- Compiler: <!-- compiler:milo -->`rustc 1.93.1 (01f6ddf75 2026-02-11)`<!-- /compiler -->
- RUSTFLAGS: `-C target-cpu=native`.
- Build: `opt-level=2; panic=abort`; native C ABI exports.

Milo uses memchr SIMD scanning, with no standard scalar switch. It is measured
only as `native`; disabling Rust target features does not disable memchr's
runtime SIMD selection.

Milo allocates a 64 KiB event buffer in its native constructor even when events
are disabled. The adapter creates one thread-local parser during warmup and
times native `reset(false)` and parsing for each message; construction and
event-buffer allocation are excluded. Thread-local access is timed.
Callbacks and events are disabled, and `suspend_after_headers` stops before
body processing.

</details>


## Parse

Start line and headers only; native initialization or reset is timed. SIMD labels
identify compiler targets, rather than guaranteeing SIMD use by every library.
Scalar rows disable explicit parser SIMD and compiler loop vectorization.

† Target RCIW was not reached. Relative compares the fastest build for each fixture.


### Browser GET Request

Synthetic desktop Chromium navigation with client hints, fetch metadata, a Cookie header and four query parameters, including a percent-encoded value.

<details>
<summary>Message (900 bytes)</summary>

```http
GET /search?q=cache%20locality&page=2&sort=recent&lang=ja HTTP/1.1
Host: app.example.com
Connection: keep-alive
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8
Accept-Encoding: gzip, deflate, br, zstd
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

```

</details>

| Parser                    | Mean ± SD (ns/message) | Relative | Throughput     | RCIW  |
| ------------------------- | ---------------------- | -------- | -------------- | ----- |
| H2O (SSE4.2)              | 183.3 ±2.4             | 1.00×    | 5.46 M msg/s   | 1.86% |
| H2O (native)              | 186.3 ±2.6             | 1.02×    | 5.37 M msg/s   | 1.96% |
| hwire (native)            | 207.0 ±3.8             | 1.13×    | 4.83 M msg/s   | 2.00% |
| hwire (SSE4.2)            | 221.8 ±3.8             | 1.21×    | 4.51 M msg/s   | 1.59% |
| hparse (native)           | 225.4 ±1.4             | 1.23×    | 4.44 M msg/s   | 0.87% |
| hwire (SSE2)              | 240.1 ±5.6             | 1.31×    | 4.16 M msg/s   | 1.91% |
| Actix Web (native)        | 272.6 ±3.8             | 1.49×    | 3.67 M msg/s   | 1.92% |
| Actix Web (scalar)        | 284.2 ±2.6             | 1.55×    | 3.52 M msg/s   | 1.27% |
| hparse (SSE2)             | 295.4 ±1.9             | 1.61×    | 3.38 M msg/s   | 0.92% |
| Actix Web (SSE4.2)        | 345.0 ±5.2             | 1.88×    | 2.90 M msg/s   | 1.64% |
| H2O (scalar)              | 347.8 ±4.5             | 1.90×    | 2.87 M msg/s   | 1.42% |
| milo (native)             | 361.0 ±46.6 †          | 1.97×    | 2.77 M msg/s   | 7.32% |
| hwire (scalar)            | 460.0 ±2.2             | 2.51×    | 2.17 M msg/s   | 0.68% |
| llhttp (Node.js) (native) | 558.4 ±5.1             | 3.05×    | 1.79 M msg/s   | 1.28% |
| llhttp (Node.js) (SSE4.2) | 602.4 ±6.9             | 3.29×    | 1.66 M msg/s   | 1.60% |
| nginx (scalar)            | 644.7 ±12.1            | 3.52×    | 1.55 M msg/s   | 1.74% |
| nginx (native)            | 682.0 ±21.0            | 3.72×    | 1.47 M msg/s   | 1.97% |
| llhttp (Node.js) (scalar) | 860.4 ±5.9             | 4.70×    | 1.16 M msg/s   | 0.95% |
| hparse (scalar)           | 1034.4 ±17.0           | 5.64×    | 966.74 k msg/s | 1.80% |


### S3 API Request

Synthetic S3 PutObject header block with AWS Signature Version 4, checksum, encryption, tagging and user metadata fields. The upload body is excluded; credentials and signature are examples.

<details>
<summary>Message (941 bytes)</summary>

```http
PUT /reports/quarter%20one.bin HTTP/1.1
Host: benchmark-bucket.s3.us-east-1.amazonaws.com
Content-Length: 1024
Content-Type: application/octet-stream
Cache-Control: max-age=3600
Content-Disposition: attachment; filename="quarter one.bin"
If-None-Match: *
x-amz-date: 20261006T000000Z
x-amz-content-sha256: 2edc986847e209b4016e141a6dc8716d3207350f416969382d431539bf292e4a
x-amz-checksum-sha256: LtyYaEfiCbQBbhQabchxbTIHNQ9BaWk4LUMVOb8pLko=
x-amz-storage-class: STANDARD
x-amz-server-side-encryption: AES256
x-amz-meta-project: hwire-benchmark
x-amz-meta-owner: benchmark
x-amz-tagging: project=hwire&environment=benchmark
Authorization: AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20261006/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-content-sha256;x-amz-date, Signature=0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
User-Agent: aws-sdk-example/1.0
Accept-Encoding: identity
Connection: keep-alive

```

</details>

| Parser                    | Mean ± SD (ns/message) | Relative | Throughput     | RCIW  |
| ------------------------- | ---------------------- | -------- | -------------- | ----- |
| H2O (SSE4.2)              | 195.0 ±1.5             | 1.00×    | 5.13 M msg/s   | 1.09% |
| H2O (native)              | 197.0 ±1.8             | 1.01×    | 5.08 M msg/s   | 1.25% |
| hwire (native)            | 216.7 ±0.9             | 1.11×    | 4.62 M msg/s   | 0.56% |
| hwire (SSE4.2)            | 228.4 ±1.6             | 1.17×    | 4.38 M msg/s   | 1.00% |
| hparse (native)           | 250.9 ±4.9             | 1.29×    | 3.99 M msg/s   | 1.81% |
| hwire (SSE2)              | 257.5 ±2.3             | 1.32×    | 3.88 M msg/s   | 1.27% |
| Actix Web (native)        | 294.7 ±3.3             | 1.51×    | 3.39 M msg/s   | 1.59% |
| hparse (SSE2)             | 320.5 ±2.8             | 1.64×    | 3.12 M msg/s   | 1.23% |
| Actix Web (scalar)        | 331.5 ±2.9             | 1.70×    | 3.02 M msg/s   | 1.21% |
| milo (native)             | 360.8 ±2.2             | 1.85×    | 2.77 M msg/s   | 0.84% |
| Actix Web (SSE4.2)        | 361.7 ±2.9             | 1.85×    | 2.77 M msg/s   | 1.12% |
| H2O (scalar)              | 389.6 ±5.0             | 2.00×    | 2.57 M msg/s   | 1.79% |
| hwire (scalar)            | 487.0 ±4.1             | 2.50×    | 2.05 M msg/s   | 1.19% |
| llhttp (Node.js) (native) | 538.0 ±3.0             | 2.76×    | 1.86 M msg/s   | 0.79% |
| llhttp (Node.js) (SSE4.2) | 568.7 ±2.1             | 2.92×    | 1.76 M msg/s   | 0.52% |
| nginx (scalar)            | 682.1 ±74.7 †          | 3.50×    | 1.47 M msg/s   | 6.21% |
| nginx (native)            | 703.5 ±9.2             | 3.61×    | 1.42 M msg/s   | 1.83% |
| llhttp (Node.js) (scalar) | 833.9 ±11.9            | 4.28×    | 1.20 M msg/s   | 1.99% |
| hparse (scalar)           | 1122.8 ±3.9            | 5.76×    | 890.65 k msg/s | 0.49% |


### Browser Response

Synthetic HTML response with cache validators, security policies and repeated Set-Cookie and Link fields. The body is excluded.

<details>
<summary>Message (950 bytes)</summary>

```http
HTTP/1.1 200 OK
Date: Tue, 06 Oct 2026 00:00:00 GMT
Content-Type: text/html; charset=utf-8
Content-Length: 8192
Content-Encoding: br
Connection: keep-alive
Cache-Control: private, max-age=0, must-revalidate
ETag: W/"page-20261006"
Last-Modified: Mon, 05 Oct 2026 12:00:00 GMT
Vary: Accept-Encoding, Cookie
Strict-Transport-Security: max-age=31536000; includeSubDomains
Content-Security-Policy: default-src 'self'; script-src 'self' 'nonce-benchmark123'; object-src 'none'; frame-ancestors 'none'
X-Content-Type-Options: nosniff
Referrer-Policy: strict-origin-when-cross-origin
Permissions-Policy: camera=(), microphone=(), geolocation=()
Set-Cookie: session=0123456789abcdef; Path=/; Secure; HttpOnly; SameSite=Lax
Set-Cookie: csrf=abcdef0123456789; Path=/; Secure; SameSite=Lax
Set-Cookie: locale=ja_JP; Path=/; Secure; SameSite=Lax
Link: </assets/app.css>; rel=preload; as=style
Link: </assets/app.js>; rel=preload; as=script

```

</details>

| Parser                    | Mean ± SD (ns/message) | Relative | Throughput     | RCIW  |
| ------------------------- | ---------------------- | -------- | -------------- | ----- |
| H2O (native)              | 206.0 ±2.0             | 1.00×    | 4.85 M msg/s   | 1.33% |
| H2O (SSE4.2)              | 206.2 ±1.9             | 1.00×    | 4.85 M msg/s   | 1.27% |
| hwire (native)            | 215.6 ±0.9             | 1.05×    | 4.64 M msg/s   | 0.56% |
| hwire (SSE2)              | 233.5 ±0.9             | 1.13×    | 4.28 M msg/s   | 0.56% |
| hwire (SSE4.2)            | 237.4 ±1.6             | 1.15×    | 4.21 M msg/s   | 0.92% |
| hparse (native)           | 252.0 ±1.7             | 1.22×    | 3.97 M msg/s   | 0.95% |
| Actix Web (native)        | 288.8 ±2.7             | 1.40×    | 3.46 M msg/s   | 1.31% |
| milo (native)             | 298.7 ±2.3             | 1.45×    | 3.35 M msg/s   | 1.09% |
| Actix Web (scalar)        | 322.9 ±2.0             | 1.57×    | 3.10 M msg/s   | 0.87% |
| hparse (SSE2)             | 330.3 ±1.8             | 1.60×    | 3.03 M msg/s   | 0.75% |
| H2O (scalar)              | 374.9 ±6.1             | 1.82×    | 2.67 M msg/s   | 1.78% |
| Actix Web (SSE4.2)        | 382.5 ±2.3             | 1.86×    | 2.61 M msg/s   | 0.85% |
| hwire (scalar)            | 470.4 ±4.9             | 2.28×    | 2.13 M msg/s   | 1.44% |
| llhttp (Node.js) (native) | 523.0 ±2.9             | 2.54×    | 1.91 M msg/s   | 0.76% |
| llhttp (Node.js) (SSE4.2) | 554.7 ±2.9             | 2.69×    | 1.80 M msg/s   | 0.73% |
| nginx (scalar)            | 592.7 ±4.5             | 2.88×    | 1.69 M msg/s   | 1.07% |
| nginx (native)            | 641.3 ±34.8 †          | 3.11×    | 1.56 M msg/s   | 3.07% |
| llhttp (Node.js) (scalar) | 871.4 ±7.7             | 4.23×    | 1.15 M msg/s   | 1.23% |
| hparse (scalar)           | 1072.5 ±3.6            | 5.21×    | 932.43 k msg/s | 0.47% |


### No Content Response

Minimal 204 response representing a beacon or acknowledgement, with no body or Content-Length field.

<details>
<summary>Message (105 bytes)</summary>

```http
HTTP/1.1 204 No Content
Date: Tue, 06 Oct 2026 00:00:00 GMT
Server: example
Connection: keep-alive

```

</details>

| Parser                    | Mean ± SD (ns/message) | Relative | Throughput    | RCIW  |
| ------------------------- | ---------------------- | -------- | ------------- | ----- |
| hparse (native)           | 32.4 ±0.6              | 1.00×    | 30.85 M msg/s | 1.83% |
| hparse (SSE2)             | 36.2 ±0.8              | 1.12×    | 27.59 M msg/s | 1.98% |
| H2O (SSE4.2)              | 38.3 ±0.6              | 1.18×    | 26.12 M msg/s | 1.82% |
| hwire (native)            | 41.0 ±0.4              | 1.26×    | 24.41 M msg/s | 1.51% |
| H2O (native)              | 41.3 ±0.7              | 1.28×    | 24.19 M msg/s | 1.82% |
| hwire (SSE2)              | 44.5 ±0.7              | 1.37×    | 22.45 M msg/s | 1.70% |
| H2O (scalar)              | 45.0 ±0.8              | 1.39×    | 22.24 M msg/s | 1.83% |
| Actix Web (native)        | 55.4 ±5.1 †            | 1.71×    | 18.04 M msg/s | 5.17% |
| hwire (SSE4.2)            | 59.5 ±0.5              | 1.84×    | 16.81 M msg/s | 1.20% |
| Actix Web (scalar)        | 60.7 ±0.7              | 1.87×    | 16.47 M msg/s | 1.65% |
| Actix Web (SSE4.2)        | 65.5 ±1.9              | 2.02×    | 15.27 M msg/s | 1.98% |
| milo (native)             | 69.6 ±1.4              | 2.15×    | 14.36 M msg/s | 1.88% |
| hwire (scalar)            | 73.2 ±1.2              | 2.26×    | 13.66 M msg/s | 1.85% |
| nginx (native)            | 86.7 ±2.0              | 2.67×    | 11.54 M msg/s | 1.92% |
| nginx (scalar)            | 88.8 ±1.5              | 2.74×    | 11.26 M msg/s | 1.84% |
| hparse (scalar)           | 103.6 ±0.8             | 3.20×    | 9.65 M msg/s  | 1.13% |
| llhttp (Node.js) (native) | 122.2 ±1.2             | 3.77×    | 8.19 M msg/s  | 1.36% |
| llhttp (Node.js) (SSE4.2) | 142.9 ±1.8             | 4.41×    | 7.00 M msg/s  | 1.71% |
| llhttp (Node.js) (scalar) | 167.3 ±2.1             | 5.16×    | 5.98 M msg/s  | 1.77% |
