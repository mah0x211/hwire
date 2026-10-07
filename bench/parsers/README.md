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
The supplied adapters use stack state and allocate no context. Cleanup is
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
| `<name>_PARSE_NAME` | Report display name; defaults to the directory name |
| `<name>_BUILD_INFO` | Native toolchain/build description recorded in the measurement metadata |
| `<name>_BUILD_DEPS` | Native source/manifests that trigger rebuilding |

### Entry points

The caller supplies valid fixture bytes; omit argument validation and result
validation in timed adapters. Return zero on success and nonzero on failure.
The caller initializes the context pointer to NULL before invoking an entry
point and retains the input until context cleanup. Cleanup is outside timing.

```c
/**
 * Parse a request start line and headers through the header terminator.
 * Called once per timed message during warmup, calibration and sampling.
 * Use native stack state; the supplied adapters allocate no context.
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
date                 : 2026-10-07T09:59:36+09:00
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
- C11 compiler and linker: Compile the driver and C adapters.
- Cargo and rustc 1.88+: Required when Rust adapters are enabled.
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
| `make nosimd` / `make sse2` / `make sse42` / `make neon` | Measure one supported variant |
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


### Configuration

Release builds use `-O2 -DNDEBUG -std=c11`. The platform files record the
compiler, flags and measurement environment. Published results use the x86
reference system.

| Plain build | Target | Additional flags |
| --- | --- | --- |
| `nosimd` | All supported targets | Adapter-specific scalar configuration |
| `sse2` | x86-64 | Compiler's default x86-64 target |
| `sse42` | x86-64 | `-msse4.2` |
| `neon` | ARM | Compiler's default ARM target |

SIMD labels identify compiler targets; each library decides which instructions
it uses. On x86-64 the default configurations are `nosimd`, `sse2` and `sse42`;
ARM uses `nosimd` and `neon`.


## Benchmark Targets

Compiler and flag entries below record the published measurements.

These targets compare the repository parser with callback-driven, array-producing
and server-native HTTP parsing APIs. Each adapter measures start-line and header
parsing using stack state.

### hwire

A callback-driven parser that exposes request/header slices to the application.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](hwire/request.c), [response.c](hwire/response.c)
- Library: Current sources in `../../src/`
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags -->
  - `nosimd`: `-DHWIRE_NO_SIMD`; disables hwire's explicit SIMD parser paths.
  - `sse2`: Default x86-64 target; enables hwire's SSE2 parser path.
  - `sse42`: `-msse4.2`; enables the SSE4.2 parser path.
  - `neon` (ARM): Default ARM compiler target; no additional C flags.
- Build: `C`; scalar (`HWIRE_NO_SIMD`), `SSE2` and `SSE4.2` (`-msse4.2`)

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
  - `nosimd`: Default x86-64 target; uses the scalar parser path.
  - `sse2`: Default x86-64 target; no additional compiler flags. Same parser path as `nosimd`; there is no SSE2 parser path.
  - `sse42`: `-msse4.2`; enables the SSE4.2 parser path.
  - `neon` (ARM): Default ARM compiler target; no additional C flags.
- Build: `C`; scalar and `SSE4.2` (`-msse4.2`) parser paths

</details>


### llhttp (Node.js)

A callback-driven HTTP parser used by Node.js.

<details>
<summary>Adapter and build details</summary>

- Adapter: [request.c](llhttp/request.c), [response.c](llhttp/response.c)
- Library: [llhttp 9.4.3](https://github.com/nodejs/llhttp/tree/release/v9.4.3), revision `0e815792b167a9bd8ace259b95b7da953776c288`
- Compiler: <!-- compiler:compiler -->`cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`<!-- /compiler -->
- CFLAGS: <!-- flags:cflags -->`-O2 -DNDEBUG -std=c11`<!-- /flags -->
  - `nosimd`: Default x86-64 target; no additional compiler flags.
  - `sse2`: Default x86-64 target; no additional compiler flags.
  - `sse42`: `-msse4.2`; selects the compiler target, without guaranteeing a parser SIMD path.
  - `neon` (ARM): Default ARM compiler target; no additional C flags.
- Build: `C`; the suite's `nosimd`, `sse2` and `sse42` compiler configurations

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
  - `nosimd`: Default x86-64 target; no additional compiler flags.
  - `sse2`: Default x86-64 target; no additional compiler flags.
  - `sse42`: `-msse4.2`; selects the compiler target, without guaranteeing a parser SIMD path.
  - `neon` (ARM): Default ARM compiler target; no additional C flags.
- Build: `C`; `--with-compat`; the suite's `nosimd`, `sse2` and `sse42` compiler configurations

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
  - `nosimd`: Default x86-64 target; no additional compiler flags.
  - `sse2`: Default x86-64 target; no additional compiler flags.
  - `sse42`: `-msse4.2`; selects the compiler target, without guaranteeing a parser SIMD path.
  - `neon` (ARM): Default ARM compiler target; no additional C flags.
- RUSTFLAGS: `-C target-cpu=generic`
  - `nosimd`: `CARGO_CFG_HTTPARSE_DISABLE_SIMD=1`; disables httparse SIMD.
  - `sse2`: `-C target-feature=+sse2,-sse4.2,-avx2` and `CARGO_CFG_HTTPARSE_DISABLE_SIMD=1`; uses the scalar parser.
  - `sse42`: `-C target-feature=+sse4.2,-avx2`; enables SSE4.2 without AVX2.
  - `neon` (ARM): `-C target-feature=+neon`; enables the NEON parser path.
- Build: <!-- build:actix_web -->opt-level=2; panic=abort; SIMD disabled for nosimd/sse2, compile-time SSE4.2 for sse42, NEON for neon<!-- /build -->

Rust exports the registration ABI directly. Its entry-point call is timed.
The `sse42` build disables AVX2; the ARM `neon` build uses httparse's NEON path.

</details>


## Parse

Start line and headers only; each adapter uses stack state. SIMD labels
identify compiler targets, rather than guaranteeing SIMD use by every library.
Unlabelled rows use the nosimd build configuration.
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

| Parser                    | Mean ± SD (ns/message) | Relative | Throughput   | RCIW  |
| ------------------------- | ---------------------- | -------- | ------------ | ----- |
| H2O (SSE4.2)              | 189.8 ±5.3             | 1.00×    | 5.27 M msg/s | 1.91% |
| hwire (SSE4.2)            | 218.6 ±1.3             | 1.15×    | 4.57 M msg/s | 0.85% |
| hwire (SSE2)              | 250.3 ±7.0             | 1.32×    | 3.99 M msg/s | 1.90% |
| Actix Web                 | 278.2 ±6.7             | 1.47×    | 3.59 M msg/s | 1.98% |
| Actix Web (SSE2)          | 300.7 ±6.6             | 1.58×    | 3.33 M msg/s | 1.81% |
| H2O                       | 314.5 ±7.2             | 1.66×    | 3.18 M msg/s | 1.87% |
| H2O (SSE2)                | 319.5 ±4.1             | 1.68×    | 3.13 M msg/s | 1.39% |
| Actix Web (SSE4.2)        | 347.5 ±3.6             | 1.83×    | 2.88 M msg/s | 1.45% |
| hwire                     | 411.2 ±8.4             | 2.17×    | 2.43 M msg/s | 1.90% |
| llhttp (Node.js) (SSE4.2) | 588.0 ±4.9             | 3.10×    | 1.70 M msg/s | 1.17% |
| nginx (SSE2)              | 659.2 ±13.8            | 3.47×    | 1.52 M msg/s | 1.95% |
| nginx (SSE4.2)            | 689.6 ±14.6            | 3.63×    | 1.45 M msg/s | 1.97% |
| nginx                     | 708.4 ±31.9 †          | 3.73×    | 1.41 M msg/s | 2.55% |
| llhttp (Node.js) (SSE2)   | 770.6 ±6.0             | 4.06×    | 1.30 M msg/s | 1.09% |
| llhttp (Node.js)          | 856.4 ±9.9             | 4.51×    | 1.17 M msg/s | 1.62% |


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

| Parser                    | Mean ± SD (ns/message) | Relative | Throughput   | RCIW  |
| ------------------------- | ---------------------- | -------- | ------------ | ----- |
| H2O (SSE4.2)              | 199.5 ±1.0             | 1.00×    | 5.01 M msg/s | 0.70% |
| hwire (SSE4.2)            | 228.3 ±4.5             | 1.14×    | 4.38 M msg/s | 1.83% |
| hwire (SSE2)              | 252.1 ±1.5             | 1.26×    | 3.97 M msg/s | 0.86% |
| Actix Web (SSE2)          | 323.5 ±2.8             | 1.62×    | 3.09 M msg/s | 1.21% |
| Actix Web                 | 326.4 ±4.1             | 1.64×    | 3.06 M msg/s | 1.75% |
| H2O                       | 335.9 ±4.5             | 1.68×    | 2.98 M msg/s | 1.88% |
| H2O (SSE2)                | 350.0 ±2.7             | 1.75×    | 2.86 M msg/s | 1.09% |
| Actix Web (SSE4.2)        | 362.8 ±2.4             | 1.82×    | 2.76 M msg/s | 0.94% |
| hwire                     | 427.6 ±3.3             | 2.14×    | 2.34 M msg/s | 1.07% |
| llhttp (Node.js) (SSE4.2) | 571.0 ±12.5            | 2.86×    | 1.75 M msg/s | 1.80% |
| nginx (SSE2)              | 682.8 ±1.8             | 3.42×    | 1.46 M msg/s | 0.37% |
| nginx (SSE4.2)            | 708.0 ±20.0            | 3.55×    | 1.41 M msg/s | 1.94% |
| nginx                     | 715.4 ±27.5 †          | 3.59×    | 1.40 M msg/s | 2.18% |
| llhttp (Node.js) (SSE2)   | 716.6 ±21.3            | 3.59×    | 1.40 M msg/s | 1.89% |
| llhttp (Node.js)          | 822.1 ±7.2             | 4.12×    | 1.22 M msg/s | 1.22% |


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

| Parser                    | Mean ± SD (ns/message) | Relative | Throughput   | RCIW  |
| ------------------------- | ---------------------- | -------- | ------------ | ----- |
| H2O (SSE4.2)              | 209.1 ±1.4             | 1.00×    | 4.78 M msg/s | 0.97% |
| hwire (SSE4.2)            | 229.0 ±1.3             | 1.10×    | 4.37 M msg/s | 0.78% |
| hwire (SSE2)              | 247.3 ±5.5             | 1.18×    | 4.04 M msg/s | 1.66% |
| Actix Web                 | 324.7 ±3.2             | 1.55×    | 3.08 M msg/s | 1.40% |
| H2O                       | 336.1 ±2.1             | 1.61×    | 2.98 M msg/s | 0.86% |
| Actix Web (SSE2)          | 336.3 ±3.4             | 1.61×    | 2.97 M msg/s | 1.40% |
| H2O (SSE2)                | 346.2 ±4.3             | 1.66×    | 2.89 M msg/s | 1.36% |
| Actix Web (SSE4.2)        | 375.2 ±3.4             | 1.79×    | 2.67 M msg/s | 1.25% |
| hwire                     | 406.5 ±3.6             | 1.94×    | 2.46 M msg/s | 1.23% |
| llhttp (Node.js) (SSE4.2) | 529.6 ±2.7             | 2.53×    | 1.89 M msg/s | 0.72% |
| nginx (SSE2)              | 613.9 ±2.6             | 2.94×    | 1.63 M msg/s | 0.59% |
| nginx                     | 646.9 ±8.6             | 3.09×    | 1.55 M msg/s | 1.87% |
| nginx (SSE4.2)            | 661.7 ±21.8            | 3.16×    | 1.51 M msg/s | 1.97% |
| llhttp (Node.js) (SSE2)   | 749.0 ±14.9            | 3.58×    | 1.34 M msg/s | 1.85% |
| llhttp (Node.js)          | 876.6 ±7.7             | 4.19×    | 1.14 M msg/s | 1.23% |


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
| H2O (SSE4.2)              | 38.0 ±0.6              | 1.00×    | 26.28 M msg/s | 1.74% |
| H2O                       | 42.7 ±0.5              | 1.12×    | 23.43 M msg/s | 1.53% |
| H2O (SSE2)                | 43.6 ±0.9              | 1.15×    | 22.92 M msg/s | 1.98% |
| hwire (SSE2)              | 44.4 ±0.4              | 1.17×    | 22.51 M msg/s | 1.41% |
| hwire (SSE4.2)            | 48.7 ±1.0              | 1.28×    | 20.51 M msg/s | 1.74% |
| hwire                     | 58.5 ±3.0 †            | 1.54×    | 17.08 M msg/s | 2.95% |
| Actix Web                 | 60.5 ±0.7              | 1.59×    | 16.52 M msg/s | 1.55% |
| Actix Web (SSE4.2)        | 62.7 ±3.9 †            | 1.65×    | 15.95 M msg/s | 3.49% |
| Actix Web (SSE2)          | 74.5 ±0.6              | 1.96×    | 13.43 M msg/s | 1.15% |
| nginx                     | 96.4 ±0.9              | 2.53×    | 10.37 M msg/s | 1.28% |
| nginx (SSE2)              | 96.5 ±5.5 †            | 2.54×    | 10.36 M msg/s | 3.22% |
| nginx (SSE4.2)            | 100.1 ±1.3             | 2.63×    | 9.99 M msg/s  | 1.76% |
| llhttp (Node.js) (SSE4.2) | 134.7 ±2.0             | 3.54×    | 7.42 M msg/s  | 1.59% |
| llhttp (Node.js) (SSE2)   | 139.9 ±1.3             | 3.68×    | 7.15 M msg/s  | 1.34% |
| llhttp (Node.js)          | 162.1 ±1.3             | 4.26×    | 6.17 M msg/s  | 1.13% |
