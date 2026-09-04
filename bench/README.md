# hwire Benchmark Suite

A single plain-C benchmark driver (`main.c`) measures hwire — and any
third-party HTTP parser registered under `parsers/` — with the
**identical timing loop** over shared, realistic fixtures, so the
numbers are directly comparable. `report.py` renders the comparison
(times, ratios, throughput). To compare against picohttpparser
(https://github.com/h2o/picohttpparser) or llhttp
(https://github.com/nodejs/llhttp), drop their sources and adapters
under `parsers/` (see "How a parser is registered"); the sample results
below were produced with hwire only.

- **hwire** — the library under test, always compiled from `../src`
- **picohttpparser** — minimal zero-allocation parser (h2o)
- **llhttp** — Node.js's generated state-machine parser

## Quick start

```sh
make               # build and run every SIMD variant, print the comparison
make request       # requests only; make response for responses only
make nosimd        # run a single variant (also: sse2, sse42, avx2, neon)
make report        # re-print the comparison from existing results/
make list          # show detected parsers and available variants
make clean         # remove build and result artifacts (bin/, results/)
```

The suite is built once per SIMD variant with the variant's flags applied
to **every** parser, so each run compares all parsers under the same
build configuration:

- `x86_64`: `nosimd` (`-DHWIRE_NO_SIMD`), `sse2` (default build),
  `sse42` (`-msse4.2`), `avx2` (`-mavx2`)
- elsewhere (e.g. `ARM64`): `nosimd`, `neon` (native build)

Results accumulate as `results/<parser>-<variant>.txt`; `report.py`
renders rows = parser × variant, columns = fixtures, with times
(± stddev), ratios against the native hwire build, and throughput.

Each run also records the measurement environment (OS/kernel, CPU,
cores, memory, caches, compiler, flags — `platform.sh` →
`results/platform.txt`), which the report prints first, so results stay
interpretable and traceable to the machine that produced them.

Requirements: a C99 compiler (gcc/clang) and Python 3. No other
dependencies.

## How a fixture is registered

`messages/request/*.h` and `messages/response/*.h` hold one header per
message, each defining a `static const unsigned char MSG_<NAME>[]`
array (the symbol is derived from the file name; the directory selects
the direction — report tables show a req_/rsp_ prefix). Dropping a
header in — or removing one — registers or unregisters the fixture
automatically. Names must be lowercase `[a-z][a-z0-9_]*`; others are
skipped with a warning. Name fixtures after the scenario they
represent (see `messages/README.md` for the bundled set and the
real-world evidence behind it).

## How a parser is registered

Registration discovery, name validation, and code generation live in
two small scripts called by the Makefile — `gen_parsers.py` (owns
`parsers/`) and `gen_fixtures.py` (owns `messages/`); `build_variant.sh`
owns the per-variant compile/link procedure. This keeps the Makefile
limited to dependency tracking and orchestration.

A `parsers/<name>/` directory registers a parser when it contains:

1. the parser's C sources and headers, and
2. two uniform adapters, `request.c` and `response.c`, implementing

```c
int <name>_request(const unsigned char *data, size_t len);   /* 0 = success */
int <name>_response(const unsigned char *data, size_t len);  /* 0 = success */
```

The Makefile detects those directories, generates a per-variant
`bin/<variant>/parserlist.c` (adapter declarations plus the parser-table
initializer), and `main.c` includes it. Dropping such a directory in registers the parser — the
comparison gains its column automatically; removing it unregisters it.
Record the source provenance (version/URL) in `parsers/README.md`.

hwire's own adapters live in `parsers/hwire/`; the library itself is
compiled from `../src` by a special Makefile rule, so the benchmark
always measures the current sources.

## Output

Each variant run writes `results/<parser>-<variant>.txt` with one
self-describing line per benchmark:

```
<fixture>/<bytes> <samples> <iterations> <mean-ns> <stddev-ns>
```

`report.py` includes every result file present and prints time per
message (mean ± stddev and the ratio to the native hwire build),
throughput, and a measurement note. A variant binary can also be run
directly (`./bin/<variant>/bench`) — it rewrites that variant's result
files without re-rendering the comparison.

## Sample results

`make` output on two reference hosts (collected 2026-09-04, hwire only;
add third-party parsers under parsers/ to include them in the
comparison). Numbers vary by machine — run `make` locally for your own
results.

### macOS, Apple M1 Max (arm64: nosimd / neon)

```
platform
--------
  date: 2026-09-04T13:21:13+09:00
  uname: Darwin 25.5.0 arm64
  os: macOS 26.5.2
  cpu: Apple M1 Max
  cores: 10
  memory: 32768 MiB
  cache l1i: 128 KiB
  cache l1d: 64 KiB
  cache l2: 4 MiB
  compiler: Apple clang version 17.0.0 (clang-1700.6.4.2)
  cflags: -O2 -std=c99

===== variant: nosimd (base = hwire-nosimd) ==================================

time per message, ns (lower is better)
parser        req_api_call (611B)  req_browser_logged_in (1213B)  req_browser_page_load (669B)  req_health_probe (95B)  req_mobile_app_feed (448B)  rsp_api_json (1122B)  rsp_cdn_asset (712B)  rsp_empty_204 (110B)  rsp_html_page (842B)  rsp_html_set_cookies (1115B)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-nosimd  198.7 ±1.0           355.5 ±2.1                     245.5 ±0.7                    44.7 ±0.5               176.8 ±0.5                  432.3 ±17.9           302.3 ±5.0            56.8 ±1.0             299.6 ±2.5            371.7 ±2.5                  

throughput (higher is better)
parser        req_api_call  req_browser_logged_in  req_browser_page_load  req_health_probe  req_mobile_app_feed  rsp_api_json  rsp_cdn_asset  rsp_empty_204  rsp_html_page  rsp_html_set_cookies
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-nosimd  3.08 GB/s     3.41 GB/s              2.72 GB/s              2.12 GB/s         2.53 GB/s            2.60 GB/s     2.36 GB/s      1.94 GB/s      2.81 GB/s      3.00 GB/s           

===== variant: neon (base = hwire-neon) ====================================

time per message, ns (lower is better)
parser      req_api_call (611B)  req_browser_logged_in (1213B)  req_browser_page_load (669B)  req_health_probe (95B)  req_mobile_app_feed (448B)  rsp_api_json (1122B)  rsp_cdn_asset (712B)  rsp_empty_204 (110B)  rsp_html_page (842B)  rsp_html_set_cookies (1115B)
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-neon  121.4 ±0.6           174.6 ±1.1                     171.8 ±3.4                    36.3 ±0.6               130.9 ±0.7                  287.5 ±6.6            207.0 ±0.6            38.6 ±1.1             188.4 ±3.3            215.5 ±0.9                  

throughput (higher is better)
parser      req_api_call  req_browser_logged_in  req_browser_page_load  req_health_probe  req_mobile_app_feed  rsp_api_json  rsp_cdn_asset  rsp_empty_204  rsp_html_page  rsp_html_set_cookies
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-neon  5.03 GB/s     6.95 GB/s              3.89 GB/s              2.62 GB/s         3.42 GB/s            3.90 GB/s     3.44 GB/s      2.85 GB/s      4.47 GB/s      5.17 GB/s           

measurement: 10 samples x 100000 iterations per benchmark
(xN.NN = N.NN times the base parser's time, same variant)
```

### Ubuntu 24.04, AMD Ryzen 7 PRO 4750GE, KVM guest (x86-64: nosimd / sse2 / sse42 / avx2)

```
platform
--------
  date: 2026-09-04T13:21:20+09:00
  uname: Linux 6.8.0-110-generic x86_64
  os: Ubuntu 24.04.3 LTS
  cpu: AMD Ryzen 7 PRO 4750GE with Radeon Graphics
  clock: 3.09 GHz
  cores: 1
  memory: 887 MiB
  cache l1-Data: 64K
  cache l1-Instruction: 64K
  cache l2-Unified: 512K
  cache l3-Unified: 16384K
  virtualization: kvm
  compiler: cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
  cflags: -O2 -std=c99

===== variant: nosimd (base = hwire-nosimd) ==================================

time per message, ns (lower is better)
parser        req_api_call (611B)  req_browser_logged_in (1213B)  req_browser_page_load (669B)  req_health_probe (95B)  req_mobile_app_feed (448B)  rsp_api_json (1122B)  rsp_cdn_asset (712B)  rsp_empty_204 (110B)  rsp_html_page (842B)  rsp_html_set_cookies (1115B)
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-nosimd  262.8 ±0.9           474.1 ±1.3                     316.6 ±1.2                    65.8 ±8.5               227.0 ±2.1                  510.2 ±1.7            349.2 ±0.7            60.9 ±0.6             381.0 ±13.8           471.1 ±3.9                  

throughput (higher is better)
parser        req_api_call  req_browser_logged_in  req_browser_page_load  req_health_probe  req_mobile_app_feed  rsp_api_json  rsp_cdn_asset  rsp_empty_204  rsp_html_page  rsp_html_set_cookies
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-nosimd  2.32 GB/s     2.56 GB/s              2.11 GB/s              1.44 GB/s         1.97 GB/s            2.20 GB/s     2.04 GB/s      1.81 GB/s      2.21 GB/s      2.37 GB/s           

===== variant: sse2 (base = hwire-sse2) ====================================

time per message, ns (lower is better)
parser      req_api_call (611B)  req_browser_logged_in (1213B)  req_browser_page_load (669B)  req_health_probe (95B)  req_mobile_app_feed (448B)  rsp_api_json (1122B)  rsp_cdn_asset (712B)  rsp_empty_204 (110B)  rsp_html_page (842B)  rsp_html_set_cookies (1115B)
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-sse2  152.2 ±3.1           223.1 ±3.1                     222.1 ±4.9                    48.8 ±0.9               163.5 ±7.8                  350.0 ±2.4            226.8 ±2.8            44.8 ±0.2             224.3 ±4.4            261.7 ±20.6                 

throughput (higher is better)
parser      req_api_call  req_browser_logged_in  req_browser_page_load  req_health_probe  req_mobile_app_feed  rsp_api_json  rsp_cdn_asset  rsp_empty_204  rsp_html_page  rsp_html_set_cookies
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-sse2  4.01 GB/s     5.44 GB/s              3.01 GB/s              1.95 GB/s         2.74 GB/s            3.21 GB/s     3.14 GB/s      2.45 GB/s      3.75 GB/s      4.26 GB/s           

===== variant: sse42 (base = hwire-sse42) ===================================

time per message, ns (lower is better)
parser       req_api_call (611B)  req_browser_logged_in (1213B)  req_browser_page_load (669B)  req_health_probe (95B)  req_mobile_app_feed (448B)  rsp_api_json (1122B)  rsp_cdn_asset (712B)  rsp_empty_204 (110B)  rsp_html_page (842B)  rsp_html_set_cookies (1115B)
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-sse42  131.2 ±0.4           192.6 ±0.3                     175.8 ±0.5                    46.4 ±0.2               141.4 ±2.4                  275.2 ±2.5            208.7 ±0.7            47.2 ±0.3             192.6 ±1.0            226.6 ±1.0                  

throughput (higher is better)
parser       req_api_call  req_browser_logged_in  req_browser_page_load  req_health_probe  req_mobile_app_feed  rsp_api_json  rsp_cdn_asset  rsp_empty_204  rsp_html_page  rsp_html_set_cookies
-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-sse42  4.66 GB/s     6.30 GB/s              3.80 GB/s              2.05 GB/s         3.17 GB/s            4.08 GB/s     3.41 GB/s      2.33 GB/s      4.37 GB/s      4.92 GB/s           

===== variant: avx2 (base = hwire-avx2) ====================================

time per message, ns (lower is better)
parser      req_api_call (611B)  req_browser_logged_in (1213B)  req_browser_page_load (669B)  req_health_probe (95B)  req_mobile_app_feed (448B)  rsp_api_json (1122B)  rsp_cdn_asset (712B)  rsp_empty_204 (110B)  rsp_html_page (842B)  rsp_html_set_cookies (1115B)
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-avx2  132.2 ±0.4           193.2 ±0.6                     175.9 ±0.3                    46.9 ±0.2               141.9 ±3.7                  275.4 ±9.0            205.6 ±0.5            45.9 ±0.1             188.8 ±0.5            224.2 ±0.5                  

throughput (higher is better)
parser      req_api_call  req_browser_logged_in  req_browser_page_load  req_health_probe  req_mobile_app_feed  rsp_api_json  rsp_cdn_asset  rsp_empty_204  rsp_html_page  rsp_html_set_cookies
----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
hwire-avx2  4.62 GB/s     6.28 GB/s              3.80 GB/s              2.02 GB/s         3.16 GB/s            4.07 GB/s     3.46 GB/s      2.40 GB/s      4.46 GB/s      4.97 GB/s           

measurement: 10 samples x 100000 iterations per benchmark
(xN.NN = N.NN times the base parser's time, same variant)
rm bin/sse2/parserlist.c bin/avx2/parserlist.c bin/sse42/parserlist.c bin/nosimd/parserlist.c
```
