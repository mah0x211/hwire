# hwire

[![release](https://img.shields.io/github/v/release/mah0x211/hwire)](https://github.com/mah0x211/hwire/releases/latest)
[![test](https://github.com/mah0x211/hwire/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/hwire/actions/workflows/test.yml)
[![fuzz](https://github.com/mah0x211/hwire/actions/workflows/fuzz.yml/badge.svg?branch=master)](https://github.com/mah0x211/hwire/actions/workflows/fuzz.yml)
[![codecov](https://codecov.io/gh/mah0x211/hwire/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/hwire)

Zero-allocation `HTTP/1.1` parser written in `C99` or later.


---

## Features

- **Zero allocation** — no internal heap allocation; the caller owns all buffers.
- **Non-destructive** — the input buffer is never modified; `hwire_str_t` fields reference it directly.
- **EAGAIN-based streaming** — returns `HWIRE_EAGAIN` when the buffer is
  incomplete; append more data and call again with the same message-start
  offset. Stateless re-scan: no per-call resume state is maintained.
- **SIMD acceleration** — auto-selects `SSE4.2` / `SSSE3` / `SSE2` on `x86-64`, `NEON` on `ARM64`; falls back to scalar code when none of those instruction sets are available at compile time.
- **HTTP/1.x grammar** — validates method tokens, URIs, header field names, header field values (including `obs-text`), `chunk-size`, and quoted-strings per **RFC 9110**, **RFC 9112**, **RFC 7230**, and **RFC 3986**.
- **`C99` or later / `C++` compatible** — single header + single source file; `extern "C"` guard included.


## RFC Compliance

This library parses `HTTP/1.x` **request/response lines, header fields, and chunk-size lines with extensions**. Application-level HTTP semantics (content negotiation, conditional requests, authentication, caching) are outside its scope.

### Compliant

The following rules from [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110) and [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112) are fully implemented:

- **Token characters** (**RFC 9110** §5.6.2): all 77 `tchar` values recognized.
- **Quoted-string** (**RFC 9110** §5.6.4): including quoted-pair (`\` escapes).
- **Parameters** (**RFC 9110** §5.6.6): semicolon-separated parameter parsing.
- **Header field values** (**RFC 9110** §5.5): `obs-text` bytes (0x80–0xFF) permitted.
- **Methods** (**RFC 9110** §9): any `1*tchar` token accepted; `HWIRE_EMETHOD` for non-`tchar` input.
- **HTTP versions** (**RFC 9112** §2.3): `HTTP/1.0` and `HTTP/1.1` recognized; others rejected.
- **Request-line** (**RFC 9112** §3): `method SP request-target SP HTTP-version CRLF`.
- **Status-line** (**RFC 9112** §4): `HTTP-version SP status-code SP reason-phrase CRLF`.
- **Header field syntax** (**RFC 9112** §5.1): `field-name ":" OWS field-value OWS CRLF`.
- **`obs-fold`** (**RFC 9112** §5.2): deprecated line folding correctly rejected with `HWIRE_EHDRNAME`.
- **Chunk-size line** (**RFC 9112** §7.1.1): hex digits + optional extensions + `CRLF`.

### Lenient

The following behaviors deviate from strict RFC requirements for robustness and backward compatibility ([RFC 9112 §2.2](https://www.rfc-editor.org/rfc/rfc9112#section-2.2)):

- **`CR?LF` line terminators**: although senders MUST use `CRLF`, recipients accept either `CRLF` or a bare `LF` at every parsed line boundary — request-line, status-line, header field line, end-of-headers, and chunk-size line. A bare `CR` is rejected.
- **Leading empty lines**: empty `CRLF` or `LF` lines before a request-line or status-line are ignored. A bare `CR` is not an empty line and is rejected.

### Partial

- **request-target** ([RFC 9112](https://www.rfc-editor.org/rfc/rfc9112) §3.2 / [RFC 3986](https://www.rfc-editor.org/rfc/rfc3986) §2–3): origin-form, absolute-form, authority-form, and asterisk-form are structurally validated. URI components are returned as zero-copy slices; scheme-specific semantics, decoding, and normalization remain outside the parser.
- **Chunked transfer encoding** ([RFC 9112](https://www.rfc-editor.org/rfc/rfc9112) §7): the chunk-size line (§7.1.1) is parsed; chunk body data and trailer fields are not handled.

### Out of scope

Message body parsing, transfer-coding, and connection management (**RFC 9112** §6–9), and all application-level HTTP semantics (**RFC 9110** §6–12) are not implemented.

---

## Benchmark

Three independent benchmark suites live in [bench/](bench/):
HTTP parsing, hashmap operations, and production request/header processing.
Each implementation owns its dependency-fetch and build configuration.

```sh
cd bench
make             # all three suites
make parsers     # parser only
make hashmaps    # map operations and memory
make production  # native request/header storage and lookup
```

Detailed configurations and results are in [Parsers](bench/parsers/README.md),
[Hashmaps](bench/hashmaps/README.md) and [Production](bench/production/README.md).
Manual [benchmark Actions](https://github.com/mah0x211/hwire/actions/workflows/benchmark.yml)
publish reports in the run Summary and downloadable artifacts. See
[manual execution](bench/README.md#manual-github-actions); published results below
are updated at selected milestones.

### Parser-only benchmark

The hwire results below measure start-line and header parsing with stack state.
Sampling targets a 2% relative confidence interval width, with 20–100 samples
and approximately 1 ms per sample. Cells show **ns/message (M messages/s)**;
† marks an unmet RCIW target.

| Fixture             | Bytes | Scalar       | SSE2         | SSE4.2       | Native       |
| ------------------- | ----- | ------------ | ------------ | ------------ | ------------ |
| Browser GET request | 900   | 460.0 (2.17) | 240.1 (4.16) | 221.8 (4.51) | 207.0 (4.83) |
| S3 API request      | 941   | 487.0 (2.05) | 257.5 (3.88) | 228.4 (4.38) | 216.7 (4.62) |
| Browser response    | 950   | 470.4 (2.13) | 233.5 (4.28) | 237.4 (4.21) | 215.6 (4.64) |
| No Content response | 105   | 73.2 (13.66) | 44.5 (22.45) | 59.5 (16.81) | 41.0 (24.41) |


### Production request and header storage

These hwire + hwire_table results measure request initialization, HTTP parsing
and header storage using preallocated memory on an AMD Ryzen 7 PRO 4750GE
(KVM, Linux x86-64). The native CPU build uses AES hashing, a case-insensitive
table and 8N slots. Socket I/O, arrival delays, input copying,
system allocation and cleanup are excluded. Cells show **mean ± SD (ns/request)**.
All displayed results met the 2% RCIW target.

**Complete Browser GET via CDN (998 bytes, 21 headers)**

| Implementation      | Parse + Post-process (ns/request) |
| ------------------- | --------------------------------- |
| hwire + hwire_table | 504.84 ±6.81                      |

**Authorization and session Cookie request (4,201 bytes, 22 headers)**

The same request is measured complete and in two calls: first 2,100 bytes (50%)
or 3,780 bytes (90%), then the rest. Split results include both attempts and
required partial-storage reset. These supplemental cases measure retry overhead;
they do not assume a fragmentation frequency or rank overall server throughput.

| Implementation      | Complete input | Split at 50%  | Split at 90%   |
| ------------------- | -------------- | ------------- | -------------- |
| hwire + hwire_table | 727.16 ±7.99   | 1201.85 ±5.52 | 1324.80 ±11.41 |

See [Production](bench/production/README.md) for CPU settings, retry behavior,
confidence intervals, header lookup and calculated total costs.


## Requirements

- Any `C99` or later compiler; `stddef.h` and `stdint.h` are required
- For `SIMD` code paths: `GCC` ≥ 4.9, `clang` ≥ 3.5, or `MSVC` (with `<intrin.h>`
  for `_BitScanForward`/`_BitScanForward64`); `SIMD` is detected automatically via
  predefined macros (`__SSE4_2__`, `__SSSE3__`, `__aarch64__`, etc.) and silently
  disabled on unsupported compilers, falling back to the scalar implementation

---

## Releases and Versioning

Releases use CalVer `YYYY.MM.SEQUENCE`, starting at sequence `0` each month
(for example, `2026.10.0`, then `2026.10.1`). CalVer identifies the release
date; API compatibility and migration notes are documented in each release.

Download `hwire-<version>.tar.gz` from [Releases](https://github.com/mah0x211/hwire/releases)
for versioned sources. The archive contains `src/`, `LICENSE`, and `README.md`
for embedding the library in an application. Both hwire and hwire_table expose
these macros through `hwire.h`:

```c
#define HWIRE_VERSION "2026.10.0"
#define HWIRE_VERSION_IS_DEVELOPMENT 0
```

Repository sources, tag checkouts, and GitHub's automatic **Source code**
archives instead define `HWIRE_VERSION` as `"development"` and
`HWIRE_VERSION_IS_DEVELOPMENT` as `1`. No compiler warning is emitted.

Maintainers create draft releases through the manual workflow described in
[CI.md](https://github.com/mah0x211/hwire/blob/master/CI.md#releases).


## Building

Copy `src/hwire.h` and `src/hwire.c` into your project and compile `hwire.c` together with your sources:

```sh
cc -std=c99 -Isrc -o myapp myapp.c src/hwire.c  # C11 or later also works
```

`SIMD` code paths are selected automatically at compile time based on the target architecture. To force a specific instruction set, pass the appropriate compiler flag (e.g., `-msse4.2` for `SSE4.2` on `x86-64`); scalar fallback is used when no supported `SIMD` macro is defined. Use `-march=native` to enable the build machine's supported instruction sets.

Define `HWIRE_NO_SIMD` (e.g. `-DHWIRE_NO_SIMD`) to force the portable scalar implementation on any target, regardless of the detected architecture. `make test-nosimd` builds and runs the test suite in this configuration.

### Building with hwire_table

To retain parsed key/value pairs, also compile `src/hwire_table.c` and copy
`src/hwire_table.h` and `src/hwire_table_aes.h` into your project:

```sh
cc -std=c99 -Isrc -o myapp myapp.c src/hwire.c src/hwire_table.c
```

The table hash is selected at compile time from compiler target features.
For GCC/Clang builds targeting a CPU with AES instructions, apply these flags
to the application build, including `hwire_table.c`:

| Target | Native build | Explicit features |
|---|---|---|
| ARM64 | `-mcpu=native` | `-march=armv8-a+crypto` |
| x86-64 | `-march=native` | `-maes -mssse3` |

`native` selects the build machine's CPU. ARM requires NEON and AES/crypto
features; x86 requires AES, SSE2, and SSSE3. For example, on Apple Silicon:

```sh
cc -std=c99 -O2 -mcpu=native -Isrc -o myapp myapp.c src/hwire.c src/hwire_table.c
```

On x86-64, use `-march=native` or `-maes -mssse3` instead. If the required
features are not enabled, the table uses SipHash-1-3. Define `HWIRE_NO_AES`
(e.g. `-DHWIRE_NO_AES`) to force SipHash-1-3, or `-DHWIRE_NO_SIMD` to disable
both parser SIMD and table AES. No runtime CPU detection is performed.

### Tests

To run the test suite:

```sh
make test
```

---

## Quick Start

The following example parses a complete `HTTP/1.1` request from a fixed buffer.

```c
#include <stdio.h>
#include <string.h>
#include "hwire.h"

static int on_request(hwire_ctx_t *ctx, hwire_request_t *req)
{
    const char *ver = (req->version == HWIRE_HTTP_V11) ? "HTTP/1.1" : "HTTP/1.0";
    printf("%.*s %.*s %s\n",
           (int)req->method.len, req->method.ptr,
           (int)req->uri.len, req->uri.ptr,
           ver);
    return 0;
}

static int on_header(hwire_ctx_t *ctx, hwire_header_t *hdr)
{
    (void)ctx;
    printf("  %.*s: %.*s\n",
           (int)hdr->key.len, hdr->key.ptr,
           (int)hdr->value.len, hdr->value.ptr);
    return 0;
}

int main(void)
{
    const char *data =
        "GET /index.html HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: close\r\n"
        "\r\n";

    hwire_ctx_t ctx = {0};
    ctx.request_cb  = on_request;
    ctx.header_cb   = on_header;

    size_t pos = 0;
    int rc = hwire_parse_request(&ctx, data, strlen(data), &pos,
                                 UINT16_MAX);
    if (rc == HWIRE_OK)
        printf("consumed %zu bytes\n", pos);
    else
        printf("parse error: %d\n", rc);

    return 0;
}
```

Expected output:

```
GET /index.html HTTP/1.1
  Host: example.com
  Connection: close
consumed 66 bytes
```

---

## API Reference

### Error Codes

All parse functions return `hwire_code_t`. Negative values are errors.

| Code | Value | Meaning |
|------|------:|---------|
| `HWIRE_OK` | 0 | Success |
| `HWIRE_EAGAIN` | −1 | Incomplete data — supply more and retry |
| `HWIRE_ELEN` | −2 | Length exceeded |
| `HWIRE_EMETHOD` | −3 | Unknown / unimplemented HTTP method |
| `HWIRE_EVERSION` | −4 | Unsupported HTTP version |
| `HWIRE_EEOL` | −5 | Invalid end-of-line (expected `CR?LF`) |
| `HWIRE_EHDRNAME` | −6 | Invalid header field name |
| `HWIRE_EHDRVALUE` | −7 | Invalid header field value |
| `HWIRE_EHDRLEN` | −8 | Header length exceeded `maxlen` |
| `HWIRE_ESTATUS` | −9 | Invalid HTTP status code |
| `HWIRE_EILSEQ` | −10 | Invalid byte sequence |
| `HWIRE_ERANGE` | −11 | Value out of range (e.g., chunk size) |
| `HWIRE_EEXTNAME` | −12 | Invalid chunk extension name |
| `HWIRE_EEXTVAL` | −13 | Invalid chunk extension value or missing EOL |
| `HWIRE_ENOBUFS` | −14 | Insufficient output buffer space |
| `HWIRE_ECALLBACK` | −15 | A callback returned non-zero |
| `HWIRE_EURI` | −16 | Invalid request-target form, component, or character |

---

### Maximum Values

Compile-time constants that can be used as default limit arguments:

| Constant | Value | Description |
|----------|------:|-------------|
| `HWIRE_MAX_CHUNKSIZE` | 4294967295 | Maximum chunk size (`UINT32_MAX`) |

---

### Data Structures

#### `hwire_str_t` — String slice

```c
typedef struct {
    size_t      len; /* String length */
    const char *ptr; /* Pointer into the input buffer (not NUL-terminated) */
} hwire_str_t;
```

References the caller's input buffer directly. Valid only as long as the input buffer is live. **Not NUL-terminated** — always use `.len` when printing or copying.

#### `hwire_buf_t` — Caller-allocated buffer

```c
typedef struct {
    size_t size; /* Buffer capacity (set by caller) */
    size_t len;  /* Bytes used (set by the library) */
    char  *buf;  /* Buffer pointer (allocated by caller) */
} hwire_buf_t;
```

Used for `ctx.qrybuf` to receive decoded query keys and values. Set `buf` to a caller-allocated buffer and `size` to its capacity before calling `hwire_parse_query`.

#### `hwire_kv_pair_t` / aliases

```c
typedef struct {
    hwire_str_t key;
    hwire_str_t value;
} hwire_kv_pair_t;

typedef hwire_kv_pair_t hwire_param_t;         /* parameter */
typedef hwire_kv_pair_t hwire_header_t;        /* header field */
typedef hwire_kv_pair_t hwire_chunksize_ext_t; /* chunk extension */
```

#### `hwire_request_t` — Parsed request line

```c
typedef struct {
    hwire_str_t          method;
    hwire_str_t          uri; /* complete request-target */
    hwire_http_version_t version;
    hwire_uri_type_t     uri_type;
    hwire_str_t          scheme;
    hwire_str_t          userinfo;
    hwire_str_t          host;
    hwire_str_t          port;
    hwire_str_t          path;
    hwire_str_t          query;
} hwire_request_t;
```

`uri` always references the complete request-target. Component slices exclude
their delimiters, except that an IP-literal `host` retains its square brackets.
An absent component has `ptr == NULL` and `len == 0`; a syntactically present
but empty component has a non-NULL pointer and `len == 0`. No component is
decoded, normalized, or NUL-terminated.

`uri_type` classifies the request-target syntax occupying the request line's
URI field. It is one of:

| Constant | Request-target syntax |
|----------|-----------------------|
| `HWIRE_ORIGIN_URI` | `absolute-path [ "?" query ]` |
| `HWIRE_ABSOLUTE_URI` | `absolute-URI` |
| `HWIRE_AUTHORITY_URI` | `uri-host ":" port` for CONNECT |
| `HWIRE_ASTERISK_URI` | exact `"*"` for OPTIONS |

RFC 3986 defines `userinfo` as one component. hwire does not reinterpret it as
a username/password pair; that historical form is deprecated. Applications
can reject a non-NULL `userinfo` according to their scheme and security policy.

#### `hwire_response_t` — Parsed status line

```c
typedef struct {
    hwire_http_version_t version;
    uint16_t             status; /* 100–599 */
    hwire_str_t          reason;
} hwire_response_t;
```

#### `hwire_http_version_t`

| Constant | Value | Meaning |
|----------|------:|---------|
| `HWIRE_HTTP_V10` | 0x0100 | HTTP/1.0 |
| `HWIRE_HTTP_V11` | 0x0101 | HTTP/1.1 |

Compare with `HWIRE_HTTP_V11` or `HWIRE_HTTP_V10` directly (e.g., `req->version == HWIRE_HTTP_V11`).

---

### Context Setup

`hwire_ctx_t` is the central configuration object passed to every parse function. Initialize it on the stack, zero it, then set the fields you need.

```c
typedef struct hwire_ctx_st {
    void        *uctx;   /* Opaque user pointer; not used by the library */
    hwire_buf_t  qrybuf; /* Decoded query buffer; set buf and size before hwire_parse_query */

    int (*query_cb      )(struct hwire_ctx_st *ctx, hwire_query_param_t    *param);
    int (*param_cb      )(struct hwire_ctx_st *ctx, hwire_param_t         *param);
    int (*chunksize_cb  )(struct hwire_ctx_st *ctx, uint32_t               size);
    int (*chunksize_ext_cb)(struct hwire_ctx_st *ctx, hwire_chunksize_ext_t *ext);
    int (*header_cb     )(struct hwire_ctx_st *ctx, hwire_header_t        *header);
    int (*request_cb    )(struct hwire_ctx_st *ctx, hwire_request_t       *req);
    int (*response_cb   )(struct hwire_ctx_st *ctx, hwire_response_t      *rsp);
} hwire_ctx_t;
```

**Required fields** per function:

| Parse function | Required callbacks | Buffer |
|---|---|---|
| `hwire_parse_parameters` | `param_cb` | — |
| `hwire_parse_chunksize` | `chunksize_cb` | — |
| `hwire_parse_headers` | `header_cb` | — |
| `hwire_parse_request` | `request_cb`, `header_cb` | — |
| `hwire_parse_response` | `response_cb`, `header_cb` | — |
| `hwire_parse_query` | `query_cb` | `qrybuf` required |

> **`qrybuf`**: before calling `hwire_parse_query`, set `qrybuf.buf` to non-NULL caller-owned storage and `qrybuf.size` to its capacity. Decoded callback slices remain valid until that storage is reused or released.

**Callbacks** must return `0` to continue parsing. Any non-zero return causes the parse function to stop immediately and return `HWIRE_ECALLBACK`.

**`uctx`** lets you attach application state so callbacks can access it without global variables:

```c
typedef struct { int header_count; } my_state_t;

static int on_header(hwire_ctx_t *ctx, hwire_header_t *hdr)
{
    my_state_t *s = ctx->uctx;
    s->header_count++;
    return 0;
}

my_state_t state = {0};
hwire_ctx_t ctx  = {0};
ctx.uctx         = &state;
ctx.header_cb    = on_header;
```

---

### Character Validation

These functions are building blocks used internally and exposed for custom parsing.

#### `hwire_is_tchar`

```c
int hwire_is_tchar(unsigned char c);
```

Returns `1` if `c` is a [tchar](https://www.rfc-editor.org/rfc/rfc9110#section-5.6.2) (HTTP token character: `!`, `#`, `$`, `%`, `&`, `'`, `*`, `+`, `-`, `.`, `^`, `_`, `` ` ``, `|`, `~`, `0-9`, `a-z`, `A-Z`), `0` otherwise.

**Parameters**

- `c` — character to test.


#### `hwire_is_vchar`

```c
int hwire_is_vchar(unsigned char c);
```

Returns `1` if `c` is a visible ASCII character (`0x21–0x7E`) or an `obs-text` byte (`0x80–0xFF`), `0` otherwise.

**Parameters**

- `c` — character to test.


#### `hwire_is_fcchar`

```c
int hwire_is_fcchar(unsigned char c);
```

Returns `1` if `c` is a field-content character: VCHAR (`0x21–0x7E`), obs-text (`0x80–0xFF`), SP (`0x20`), or HTAB (`0x09`). Returns `0` otherwise.

**Parameters**

- `c` — character to test.

#### `hwire_parse_tchar`

```c
size_t hwire_parse_tchar(const char *str, size_t len, size_t *pos);
```

Advances `*pos` past consecutive tchar characters starting at `str[*pos]`. Returns the number of characters consumed (`0` if `str[*pos]` is not tchar).

**Parameters**

- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: start offset on entry, first non-tchar offset on return (must not be NULL). If the initial offset is greater than or equal to `len`, the function returns `0` and leaves `*pos` unchanged.

#### `hwire_parse_vchar`

```c
size_t hwire_parse_vchar(const char *str, size_t len, size_t *pos);
```

Same as `hwire_parse_tchar` but for vchar (visible ASCII + obs-text).

**Parameters**

- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: start offset on entry, first non-vchar offset on return (must not be NULL). If the initial offset is greater than or equal to `len`, the function returns `0` and leaves `*pos` unchanged.


#### `hwire_parse_fcchar`

```c
size_t hwire_parse_fcchar(const char *str, size_t len, size_t *pos);
```

Advances `*pos` past consecutive field-content characters (VCHAR, obs-text, SP, HTAB) starting at `str[*pos]`. Returns the number of characters consumed (`0` if `str[*pos]` is not fcchar). Stops at CR, LF, NUL, DEL, or any other CTL.

This is the superset of `hwire_parse_vchar`: it additionally accepts SP and HTAB, which are valid within an HTTP field-value per **RFC 9110 §5.5**.

**Parameters**

- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: start offset on entry, first non-fcchar offset on return (must not be NULL). If the initial offset is greater than or equal to `len`, the function returns `0` and leaves `*pos` unchanged.

---

### String Parsing

#### `hwire_parse_quoted_string`

```c
int hwire_parse_quoted_string(const char *str, size_t len, size_t *pos,
                              size_t maxlen);
```

Parses a quoted-string per [RFC 9110 §5.6.4](https://www.rfc-editor.org/rfc/rfc9110#section-5.6.4). `str[*pos]` must be `"`. On success, `*pos` is advanced past the closing `"`.

**Parameters**

- `str` — input string (must not be NULL; `str[*pos]` must be `"`).
- `len` — total bytes in `str`.
- `pos` — in/out: start offset on entry, end offset on return (must not be NULL).
- `maxlen` — maximum number of bytes examined from the initial `*pos`. Both
  `"` delimiters count toward the budget.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | Valid quoted-string consumed |
| `HWIRE_EAGAIN` | Input ends before the `maxlen` budget is exhausted |
| `HWIRE_EILSEQ` | Invalid character inside the string |
| `HWIRE_ELEN` | The quoted-string is incomplete when the `maxlen` budget is exhausted, including empty input with `maxlen == 0` |

#### `hwire_parse_parameters`

```c
int hwire_parse_parameters(hwire_ctx_t *ctx, const char *str, size_t len,
                           size_t *pos, size_t maxlen,
                           int skip_leading_semicolon);
```

Parses a semicolon-separated parameter list per **RFC 9110 §5.6.6**:

```
parameters = *( OWS ";" OWS [ parameter ] )
parameter  = parameter-name "=" parameter-value
```

`ctx->param_cb` is called for each non-empty parameter. Parameter names reference the original input bytes; case-insensitive comparison is handled by the application or its container. Empty parameters are skipped, and a trailing semicolon is complete at input end. CR or LF after an empty parameter is left unconsumed. On `HWIRE_OK`, check `*pos` against `len` and, if input remains, validate the terminator at `str[*pos]`.

**Parameters**

- `ctx` — parser context (`param_cb` must not be NULL).
- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: start offset on entry, end offset on return (must not be NULL). An initial offset greater than `len` returns `HWIRE_EILSEQ` unchanged.
- `maxlen` — maximum number of bytes examined from the initial `*pos`. If a
  required parameter component is incomplete when this budget is exhausted,
  the function returns `HWIRE_ELEN` without examining later bytes.
- `skip_leading_semicolon` — non-zero to accept the first parameter without a leading `;`.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | All parameters consumed |
| `HWIRE_EAGAIN` | A required parameter component needs more input before `maxlen` is exhausted |
| `HWIRE_EILSEQ` | Invalid byte sequence |
| `HWIRE_ELEN` | A required parameter component is incomplete upon exhausting `maxlen` |
| `HWIRE_ECALLBACK` | Callback returned non-zero |

---

### HTTP Parsing

#### `hwire_parse_chunksize`

```c
int hwire_parse_chunksize(hwire_ctx_t *ctx, const char *str, size_t len,
                          size_t *pos, size_t maxlen);
```

Parses a chunked-encoding size line per [RFC 9112 §7.1](https://www.rfc-editor.org/rfc/rfc9112#section-7.1):

```
chunk-size = 1*HEXDIG
chunk-ext  = *( BWS ";" BWS chunk-ext-name [ BWS "=" BWS chunk-ext-val ] )
```

`ctx->chunksize_cb` is called once with the parsed size; `ctx->chunksize_ext_cb` is called for each extension when set (optional). When it is `NULL`, extensions are still syntax-checked but are not delivered. Set a callback when an application extension limit is needed. On success, `*pos` is advanced past the trailing `CRLF` or `LF`.

When `=` is present, it must be followed by a non-empty token or a
quoted-string. An empty quoted-string (`foo=""`) is valid, while an empty token
value (`foo=`) is rejected with `HWIRE_EEXTVAL`.

**Parameters**

- `ctx` — parser context (`chunksize_cb` must not be NULL).
- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: start offset on entry, position after the trailing `CRLF`
  or `LF` on success (must not be NULL). It is unchanged on failure.
- `maxlen` — maximum line length and input scanning budget in bytes from the
  initial `*pos`. The complete line, including its trailing `CRLF` or `LF`,
  must fit within this budget. If available input ends before the budget is
  exhausted, the parser returns `HWIRE_EAGAIN`. If the line is incomplete
  upon exhausting `maxlen`, it returns `HWIRE_ELEN` without examining later
  bytes.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | Chunk-size line consumed including `CRLF` or `LF` |
| `HWIRE_EAGAIN` | More data needed before `maxlen` is reached |
| `HWIRE_ELEN` | Line incomplete upon reaching `maxlen`, including empty input with `maxlen == 0` |
| `HWIRE_ERANGE` | Chunk size exceeds `HWIRE_MAX_CHUNKSIZE` |
| `HWIRE_EILSEQ` | Invalid byte sequence |
| `HWIRE_EEOL` | Invalid end-of-line terminator |
| `HWIRE_EEXTNAME` | Invalid extension name |
| `HWIRE_EEXTVAL` | Invalid extension value or missing line terminator |
| `HWIRE_ECALLBACK` | Callback returned non-zero |

#### `hwire_parse_headers`

```c
int hwire_parse_headers(hwire_ctx_t *ctx, const char *str, size_t len,
                        size_t *pos, size_t maxlen);
```

Parses HTTP header fields until an empty `CRLF` or `LF` line. `ctx->header_cb` is called for each field. Field names reference the original input bytes; case-insensitive comparison is handled by the application or its container, such as `hwire_table`.

**Parameters**

- `ctx` — parser context (`header_cb` must not be NULL).
- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: start offset on entry, position after the empty `CRLF` or
  `LF` line on success (must not be NULL). It is unchanged on failure.
- `maxlen` — maximum total length of the header block from the initial
  `*pos`, including field delimiters, line endings, and the terminating empty
  line. An incomplete block at the budget boundary returns `HWIRE_EHDRLEN`
  without examining later bytes.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | All headers consumed including the empty line |
| `HWIRE_EAGAIN` | Input is absent at the start or ends before the header block budget |
| `HWIRE_EHDRNAME` | Invalid header field name |
| `HWIRE_EHDRVALUE` | Invalid header field value |
| `HWIRE_EHDRLEN` | Header block is incomplete upon exhausting `maxlen` |
| `HWIRE_EEOL` | Invalid end-of-line in header value (CR without LF) |
| `HWIRE_ECALLBACK` | Callback returned non-zero |

#### `hwire_parse_request`

```c
int hwire_parse_request(hwire_ctx_t *ctx, const char *str, size_t len,
                        size_t *pos, size_t maxlen);
```

Parses a full `HTTP/1.x` request (request-line + headers). `ctx->request_cb` is called once for the request line, then `ctx->header_cb` for each header field. The request-target is structurally parsed into its RFC 9112 form and RFC 3986 components while `req->uri` retains the complete wire value. Returns `HWIRE_OK` when the empty line terminating the headers has been consumed.

```
GET /index.html HTTP/1.1\r\n
Host: example.com\r\n
\r\n
```

**Parameters**

- `ctx` — parser context (`request_cb` and `header_cb` must not be NULL).
- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: request start offset on entry and end offset on success
  (must not be NULL). It is unchanged on failure, including `HWIRE_EAGAIN`.
- `maxlen` — maximum total byte length from the initial `*pos` (leading
  empty lines + request-line + header fields and the terminating empty line,
  all delimiters included). An incomplete component at
  the budget boundary returns its length error without examining later bytes.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | Full request consumed including the empty line |
| `HWIRE_EAGAIN` | Input ends before the applicable budget |
| `HWIRE_EMETHOD` | Invalid method (not tchar or missing SP) |
| `HWIRE_EVERSION` | Unsupported HTTP version |
| `HWIRE_EEOL` | Invalid end-of-line |
| `HWIRE_ELEN` | Leading empty lines or the request-line are incomplete upon exhausting `maxlen` |
| `HWIRE_EURI` | Invalid request-target form, component, or character |
| `HWIRE_EHDRNAME` | Invalid header field name |
| `HWIRE_EHDRVALUE` | Invalid header field value |
| `HWIRE_EHDRLEN` | The header section is incomplete upon exhausting the remaining `maxlen` budget |
| `HWIRE_ECALLBACK` | Callback returned non-zero |

#### `hwire_parse_query`

```c
int hwire_parse_query(hwire_ctx_t *ctx, const char *str, size_t len,
                      size_t *pos, size_t maxlen);
```

This opt-in API splits query input (without `?`) on literal `&` and the
first literal `=` per nonempty segment, delivering pairs to `ctx->query_cb` in
order. `;` is ordinary data. Configure `ctx->qrybuf` with a separate,
non-overlapping caller-owned buffer; `%HH` and `+` are decoded into it (`+`
becomes a space), while `%26` and `%3D` never act as separators. Keep the
decode buffer alive while retaining callback slices. `maxlen` is a byte budget
from the initial `*pos`; success sets `*pos == len`. An incomplete `%HH` at the
available input end returns `HWIRE_EAGAIN` while budget remains, or
`HWIRE_ELEN` when it is exhausted. Invalid characters or hex digits return
`HWIRE_EURI`; decode-buffer exhaustion returns `HWIRE_ENOBUFS`, and a
stopped callback returns `HWIRE_ECALLBACK`. As with parameter parsing, a pair
ending at the byte budget may be delivered before the outer parser returns
`HWIRE_ELEN`; in that case `*pos` advances to the budget boundary.

For example, a request callback can parse `req->query` without changing the
request-target parser. The decoded storage and saved slices must last until
the application finishes handling the request:

```c
typedef struct {
    hwire_buf_t query_storage; /* caller-allocated request-lifetime buffer */
    hwire_query_param_t params[32];
    size_t count;
    int query_error;
    int storage_error;
} app_request_t;

static int on_query(hwire_ctx_t *ctx, hwire_query_param_t *param)
{
    app_request_t *app = ctx->uctx;
    if (app->count >= sizeof(app->params) / sizeof(app->params[0])) {
        app->storage_error = HWIRE_ENOBUFS;
        return -1;
    }
    app->params[app->count++] = *param;
    return 0;
}

static int on_request(hwire_ctx_t *ctx, hwire_request_t *req)
{
    app_request_t *app = ctx->uctx;
    if (req->query.ptr == NULL) {
        return 0;
    }
    size_t pos = 0;
    ctx->qrybuf = app->query_storage;
    app->query_error = hwire_parse_query(ctx, req->query.ptr, req->query.len,
                                         &pos, req->query.len);
    return app->query_error == HWIRE_OK ? 0 : -1;
}
/* Before hwire_parse_request, set app.query_storage.buf/size to separate
request-lifetime storage, reset app.count to zero and both errors to
   HWIRE_OK, and set ctx.query_cb
   = on_query and ctx.request_cb = on_request. If the outer parser returns
   HWIRE_ECALLBACK, inspect app.query_error and app.storage_error. */
```

#### `hwire_parse_response`

```c
int hwire_parse_response(hwire_ctx_t *ctx, const char *str, size_t len,
                         size_t *pos, size_t maxlen);
```

Parses a full `HTTP/1.x` response (status-line + headers). `ctx->response_cb` is called once for the status line, then `ctx->header_cb` for each header field. Returns `HWIRE_OK` when the empty line has been consumed.

```
HTTP/1.1 200 OK\r\n
Content-Length: 0\r\n
\r\n
```

**Parameters**

- `ctx` — parser context (`response_cb` and `header_cb` must not be NULL).
- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: response start offset on entry and end offset on success
  (must not be NULL). It is unchanged on failure, including `HWIRE_EAGAIN`.
- `maxlen` — maximum total byte length from the initial `*pos` (leading
  empty lines + status-line + header fields and the terminating empty line,
  all delimiters included). An incomplete component at
  the budget boundary returns its length error without examining later bytes.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | Full response consumed including the empty line |
| `HWIRE_EAGAIN` | Input ends before the applicable budget |
| `HWIRE_ESTATUS` | Invalid HTTP status code |
| `HWIRE_EVERSION` | Unsupported HTTP version |
| `HWIRE_EEOL` | Invalid end-of-line |
| `HWIRE_EILSEQ` | Invalid character in reason phrase |
| `HWIRE_ELEN` | Leading empty lines or the status-line are incomplete upon exhausting `maxlen` |
| `HWIRE_EHDRNAME` | Invalid header field name |
| `HWIRE_EHDRVALUE` | Invalid header field value |
| `HWIRE_EHDRLEN` | The header section is incomplete upon exhausting the remaining `maxlen` budget |
| `HWIRE_ECALLBACK` | Callback returned non-zero |

---

## Application limits

Callbacks enforce header, query-parameter, parameter and chunk-extension limits
using caller-owned state reached through `ctx->uctx`. Reject an item before
writing beyond the destination capacity, save the application error, and return
nonzero. The parser stops immediately with `HWIRE_ECALLBACK`; inspect the saved
error to distinguish full storage from another callback failure. No item-count
limit is imposed by the parser. `maxlen` still bounds examined input bytes, and
parser output-buffer bounds remain enforced.

If storage overwrites duplicate keys or discards items, its size may differ from
the received item count. Keep a separate callback counter when the application
needs to limit received items. Earlier callback effects are not rolled back on
failure. Reset per-attempt storage, counters and application errors before
retrying a message from its start after `HWIRE_EAGAIN`.

## Streaming and EAGAIN

`hwire` is designed for incremental I/O. When the input buffer does not yet contain a complete message, the parse function returns `HWIRE_EAGAIN`. The caller must then:

1. Keep the data received so far.
2. Read more bytes from the network and append them to the buffer.
3. Call the parse function again with the same `str`, updated `len`, and
   unchanged message-start `pos`.

On `HWIRE_EAGAIN`, request, response, header, and chunk-size parsers leave
`pos` at the current construct's start and re-scan from there on retry. After
`HWIRE_OK`, `pos` points to the next byte, so a concatenated message or line
can be parsed by calling the same function again without slicing `str`.

```c
char buf[4096];
size_t filled = 0;
size_t pos = 0;

for (;;) {
    ssize_t n = recv(fd, buf + filled, sizeof(buf) - filled, 0);
    if (n <= 0) break; /* connection closed or error */
    filled += (size_t)n;

    int rc = hwire_parse_response(&ctx, buf, filled, &pos,
                                  UINT16_MAX);
    if (rc == HWIRE_OK) {
        /* buf[0..pos-1] is the header section; body starts at buf[pos] */
        break;
    }
    if (rc == HWIRE_EAGAIN) {
        /* need more data; continue reading */
        if (filled == sizeof(buf)) {
            /* buffer full, message too large */
            break;
        }
        continue;
    }
    /* parse error */
    break;
}
```

> **Note:** `hwire_parse_parameters` does not consume the byte following the
> parameter list. It can return `HWIRE_EAGAIN` after a prefix such as `;name=` or
> `name=` when more input can still fit within `maxlen`.

---

# hwire_table

`hwire_table` is hwire's built-in data structure library for retaining
key/value pairs produced by header, query, and parameter parsing callbacks.
Build the table alongside the parser as described in
[Building](#building-with-hwire_table).

## Features

- **Zero allocation** — no internal heap allocation; the caller supplies the
  pair and index arrays and key/value storage.
- **Fixed capacity, fully usable** — every entry up to the configured capacity
  can store a pair, including distinct keys. No space needs to be reserved for
  a load-factor threshold, and no rehashing or resizing occurs. Key and value
  lengths may vary.
- **Caller-controlled growth** — link another segment when full; existing
  pairs and indexes remain in place.
- **Stable entries** — inserting pairs never moves existing entries or
  invalidates their addresses.
- **Borrowed key/value storage** — copies pair descriptors and references the
  supplied bytes without copying or modifying them.
- **Selectable indexes** — enable exact lookup, ASCII case-insensitive lookup,
  or both.
- **Insertion order** — duplicate traversal and full-table iteration preserve
  the order in which pairs were pushed.
- **Accelerated hashing** — selects AES instructions on supported ARM and x86
  build targets, with SipHash-1-3 as the fallback.

## Storage and keys

- `hwire_table_t` is the table descriptor.
- `hwire_kv_pair_t` is the stored pair descriptor. Supply a mutable pair array
  on the stack, in static storage, or in caller-allocated memory.
- `hwire_table_index_t` is the element type for the separate mutable index
  array. Single-index and dual-index tables require different array lengths.
- `hwire_table_key_t` holds two 64-bit hash-key words, copied by initialization.
- `hwire_table_iter_t` holds a current pair position;
  initialize it with `{0}` for iteration.

`hwire_table_push` copies an `hwire_kv_pair_t`, whose `key` and `value` are
`hwire_str_t` slices (`len`, `ptr`). It borrows the referenced bytes. Keep the
pair and index arrays and referenced storage alive while using the table.
Stored keys must remain unchanged. Pairs remain at stable addresses after
subsequent pushes; reinitialization invalidates all returned pairs and iterator
positions. Do not modify the index or descriptor fields directly.

Keys are length-delimited byte sequences: empty keys, embedded NUL, and UTF-8
bytes are supported. Exact lookup compares every byte. Case-insensitive lookup
folds only ASCII `A`–`Z` to `a`–`z`; it does not perform Unicode case folding
or normalization. Values are stored as supplied.

Capacity counts all pairs, including duplicates. Pass a power of two from
1 through 32768 to `hwire_table_init`. Use the capacity macros documented
under API below to size the pair and index arrays.

| Mode | Indexed comparison | Index capacity macro |
|---|---|---|
| `HWIRE_TABLE_CASE_SENSITIVE` | Exact | `HWIRE_TABLE_INDEX_CAPACITY` |
| `HWIRE_TABLE_CASE_INSENSITIVE` | ASCII case-insensitive | `HWIRE_TABLE_INDEX_CAPACITY` |
| Both flags | Exact and ASCII case-insensitive | `HWIRE_TABLE_INDEX_BOTH_CAPACITY` |

Use `HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE` to enable both
indexes. The mode type is an integer bit set, so the OR expression also works
in C++ without a cast.

CI-only mode avoids constructing an exact index. It is suitable for HTTP
headers; exact-only mode is suitable for case-sensitive query parameters.

All `capacity` entries can be filled, including with distinct keys.
No entries need to be reserved to keep the hash index
below a load-factor threshold. The table never rehashes, resizes, or allocates
additional storage. After `capacity` pairs have been stored, another push returns
`HWIRE_TABLE_EFULL`. Link another caller-owned segment to extend the chain.
Each segment retains its own capacity and index. Applications control allocation,
pooling and the total pair limit. Deletion of individual pairs is not supported.
Operate on the chain root for push, link and unlink; do not reinitialize linked
segments. Link and unlink invalidate existing iterators.

## API

```c
HWIRE_TABLE_CAPACITY(n)
```

Round the requested pair count up to a supported power-of-two capacity.
For example, `hwire_kv_pair_t entries[HWIRE_TABLE_CAPACITY(100)]` provides
128 pair slots. Zero, negative, or greater-than-32768 requests produce zero.
The argument may be evaluated multiple times; do not pass an expression with
side effects.

```c
typedef enum {
    HWIRE_TABLE_SLOTS_CAP_2N  = 2,
    HWIRE_TABLE_SLOTS_CAP_4N  = 4,
    HWIRE_TABLE_SLOTS_CAP_8N  = 8
} hwire_table_slots_capacity_t;
```

Select the number of hash slots relative to pair capacity for each enabled
index. More slots lower occupancy and can shorten the probe sequences used
by insertion and key lookup. With distinct keys filling the pair array,
2N, 4N, and 8N have slot occupancies of 50%, 25%, and 12.5%, respectively.

The tradeoff is a larger index array and more work to clear it during
initialization and linking. A larger slot capacity does not guarantee better
performance: the benefit depends on key count, occupancy, and cache behavior.
Choose the slot capacity using measurements of your workload, including table
construction and the expected number of key lookups.

```c
HWIRE_TABLE_INDEX_CAPACITY(capacity, slots_capacity)
HWIRE_TABLE_INDEX_BOTH_CAPACITY(capacity, slots_capacity)
```

Return the number of `hwire_table_index_t` elements required, not the number
of bytes. Both arguments must be supported values; the macros do not round or
validate them. Use the single-index macro for exact-only or CI-only lookup,
and the both-index macro when both flags are enabled. Use the same slot
capacity when allocating the index array and initializing the table.

Let `N` be the pair capacity and `S` the selected slot multiplier.

| Macro | Elements | Layout |
|---|---:|---|
| `HWIRE_TABLE_INDEX_CAPACITY(N, S)` | `(S + 2) * N` | `S * N` hash slots, `N` duplicate-next references, `N` duplicate-tail references |
| `HWIRE_TABLE_INDEX_BOTH_CAPACITY(N, S)` | `2 * (S + 2) * N` | One layout for exact lookup and one for CI lookup |

`HWIRE_TABLE_INDEX_FACTOR(slots_capacity)` is the slot multiplier plus `2u`,
for the duplicate-next and duplicate-tail regions.
`HWIRE_TABLE_INDEX_BOTH_FACTOR(slots_capacity)` is twice that factor.

```c
enum { CAPACITY = HWIRE_TABLE_CAPACITY(100) };
hwire_kv_pair_t entries[CAPACITY];
hwire_table_index_t index[
    HWIRE_TABLE_INDEX_CAPACITY(CAPACITY, HWIRE_TABLE_SLOTS_CAP_4N)
];
```

```c
void hwire_table_key_init(hwire_table_key_t *key, uint64_t seed);
```

Expand a caller-selected 64-bit seed into a deterministic 128-bit key.
`key` must be non-NULL. This helper adds no entropy. Applications requiring
secret key material can populate the two `key.words` elements from their
random source.

```c
hwire_table_code_t hwire_table_init(
    hwire_table_t *table,
    const hwire_table_key_t *key,
    hwire_table_mode_t mode,
    hwire_kv_pair_t *entries,
    size_t capacity,
    hwire_table_index_t *index,
    hwire_table_slots_capacity_t slots_capacity);
```

Initialize or reset a table and copy the key. All pointers must be non-NULL.
The pair array must hold at least `capacity` elements. The index array must use
the single-index capacity macro for either single mode, and the both-index
capacity macro when both flags are set. Zero or unknown mode bits and
unsupported slot capacities return
`HWIRE_TABLE_EINVAL`. The pair capacity must be a power of two in [1, 32768];
unsupported pair capacities return `HWIRE_TABLE_ECAPACITY`. Initialization
clears the complete index array for the
selected mode and slot capacity and leaves the pair array unchanged. Return
`HWIRE_TABLE_OK`, `HWIRE_TABLE_EINVAL`, or `HWIRE_TABLE_ECAPACITY`. Invalid
arguments leave the table and both arrays unchanged. Unlink all following
segments before resetting a chain root; do not reset linked segments.

```c
hwire_table_code_t hwire_table_link(hwire_table_t *table,
                                    hwire_table_t *next_table);
hwire_table_t *hwire_table_unlink(hwire_table_t *table);
```

Link an initialized, exclusively owned standalone segment to a chain whose
final segment is full. Single-index segments are compatible with either
single comparison mode; dual-index segments require dual-index storage.
Link copies the root key and mode, clears the appended index and resets its
length without modifying pair bytes. Each segment keeps its selected slot count;
segments may use different slot capacities. Return `HWIRE_TABLE_OK` or
`HWIRE_TABLE_EINVAL`; rejection leaves both tables unchanged. All storage
remains caller-owned, and no allocation or rehashing occurs.

Unlink removes the segment immediately after the root, reconnects the suffix
and returns the detached, readable standalone segment. Return NULL when there
is no following segment or the root is NULL. Release or return detached storage
to its pool after unlinking. Both operations invalidate existing iterators.

```c
/* extra is initialized with its own pair and index arrays. */
if (hwire_table_push(&table, &pair) == HWIRE_TABLE_EFULL) {
    if (hwire_table_link(&table, &extra) != HWIRE_TABLE_OK ||
        hwire_table_push(&table, &pair) != HWIRE_TABLE_OK) {
        /* handle the application pair limit or storage failure */
    }
}
hwire_table_t *detached;
while ((detached = hwire_table_unlink(&table)) != NULL) {
    /* return detached and its arrays to the application pool */
}
```

```c
hwire_table_code_t hwire_table_push(hwire_table_t *table,
                                  const hwire_kv_pair_t *kv);
```

Append one pair to the final segment of an initialized chain. Nonempty slices
require non-NULL pointers. Return `HWIRE_TABLE_OK`, `HWIRE_TABLE_EINVAL`, or
`HWIRE_TABLE_EFULL`. A failed push leaves the table unchanged.

```c
const hwire_kv_pair_t *hwire_table_get(const hwire_table_t *table,
                                       const char *key, size_t keylen,
                                       hwire_table_iter_t *iter);
const hwire_kv_pair_t *hwire_table_get_ci(const hwire_table_t *table,
                                          const char *key, size_t keylen,
                                          hwire_table_iter_t *iter);
```

Find the first matching pair in insertion order using exact or ASCII
case-insensitive comparison. Return NULL when no key matches or the requested
index was not enabled. `table` must be initialized; `table` and `key` must
be non-NULL. Use an empty string for a zero-length query. Pass NULL for `iter` when only the
first pair is needed. Otherwise lookup records the owning table and
current entry index; failed lookup leaves the cursor unchanged. A missing key is an
ordinary lookup result.

```c
const hwire_kv_pair_t *hwire_table_next(hwire_table_iter_t *iter);
const hwire_kv_pair_t *hwire_table_next_ci(hwire_table_iter_t *iter);
```

Follow exact or ASCII case-insensitive duplicates after the current cursor
position in insertion order. Use a cursor from get, get_ci, iterate or next.
The cursor pointer must be non-NULL. Return NULL at the end, for a cursor
whose table is NULL, or when the requested index is disabled. On success, update the position; otherwise leave it unchanged.
Within a segment these functions follow duplicate indexes without hashing.
Across segments they reuse separate exact and CI hashes. A zero cache value
triggers recomputation; a hash whose actual value is zero remains valid for
lookup but is recalculated when next needed. A CI step can change the exact key casing,
so the APIs invalidate an exact hash cached for the preceding pair.

Comparison is selected by the called function: get followed by next_ci is
supported, and only later CI matches are visited. Cursor fields are maintained
by the APIs.

```c
const hwire_kv_pair_t *hwire_table_iterate(const hwire_table_t *table,
                                           hwire_table_iter_t *iter);
```

Visit every pair in an initialized table in insertion order. Initialize the
non-NULL cursor with `{0}` before the first call. Each result records the
current pair. Return NULL at the end without changing the cursor. The same cursor may be used for next/next_ci; iteration
then continues after its current pair. Pairs appended during iteration become
visible on later calls. Link, unlink and reset invalidate prior cursor positions.

| Result | Meaning |
|---|---|
| `HWIRE_TABLE_OK` (0) | Operation succeeded |
| `HWIRE_TABLE_EINVAL` (-1) | Invalid argument, pair slice or link |
| `HWIRE_TABLE_ECAPACITY` (-2) | Initialization capacity is unsupported |
| `HWIRE_TABLE_EFULL` (-3) | Pair capacity is exhausted |

## Example

Parse a complete request and retain headers and decoded query parameters in
separate tables. `request_cb` calls `hwire_parse_query`; `query_cb` and
`header_cb` append the pairs through the application state in `ctx.uctx`.
Header names use ASCII case-insensitive lookup, while query keys use exact
lookup. Both duplicate chains retain insertion order.

`app_request_t` contains the parser context, both tables, their pair and index
arrays, and the query decode buffer. The header table enables CI indexing; the
query table uses the smaller exact-only index. Set `app.ctx.uctx` to `&app` so
callbacks can use this request state. Header slices reference `input`; decoded
query slices reference `app.query_storage`. Keep the request state and input
alive while using the tables. The parser preserves field names, and the header
table supplies case-insensitive lookup.
For a header block, use the same header callback with `hwire_parse_headers`.

An insertion failure stops the callback with `HWIRE_ECALLBACK`. The example
keeps query and table results separately to identify the underlying failure.
When retrying after `HWIRE_EAGAIN`, reinitialize both tables before parsing
again, since the parser replays callbacks from the message start.

```c
#include "hwire_table.h"
#include <stdio.h>

enum { HEADER_CAPACITY = HWIRE_TABLE_CAPACITY(100), QUERY_CAPACITY = 16 };

typedef struct {
    hwire_ctx_t ctx;
    hwire_request_t request;
    hwire_table_t headers;
    hwire_table_t query_params;
    hwire_kv_pair_t header_entries[HEADER_CAPACITY];
    hwire_kv_pair_t query_entries[QUERY_CAPACITY];
    hwire_table_index_t
        header_index[HWIRE_TABLE_INDEX_CAPACITY(
            HEADER_CAPACITY, HWIRE_TABLE_SLOTS_CAP_2N)];
    hwire_table_index_t
        query_index[HWIRE_TABLE_INDEX_CAPACITY(
            QUERY_CAPACITY, HWIRE_TABLE_SLOTS_CAP_2N)];
    char query_storage[256];
    hwire_table_code_t table_result;
    int query_result;
} app_request_t;

static int on_query(hwire_ctx_t *ctx, hwire_query_param_t *param)
{
    app_request_t *app = ctx->uctx;
    app->table_result = hwire_table_push(&app->query_params, param);
    return app->table_result == HWIRE_TABLE_OK ? 0 : -1;
}

static int on_request(hwire_ctx_t *ctx, hwire_request_t *request)
{
    app_request_t *app = ctx->uctx;
    app->request = *request;
    if (request->query.ptr == NULL) {
        return 0;
    }
    size_t pos = 0;
    app->query_result = hwire_parse_query(ctx, request->query.ptr,
                                         request->query.len, &pos,
                                         request->query.len);
    return app->query_result == HWIRE_OK ? 0 : -1;
}

static int on_header(hwire_ctx_t *ctx, hwire_header_t *header)
{
    app_request_t *app = ctx->uctx;
    app->table_result = hwire_table_push(&app->headers, header);
    return app->table_result == HWIRE_TABLE_OK ? 0 : -1;
}

int main(void)
{
    const char input[] =
        "GET /index.html?name=Alice+Smith&tag=A&tag=B HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "X-Tag: A\r\n"
        "x-tag: B\r\n"
        "X-Tag: C\r\n"
        "\r\n";
    app_request_t app;
    app.ctx = (hwire_ctx_t){0};
    app.table_result = HWIRE_TABLE_OK;
    app.query_result = HWIRE_OK;
    hwire_table_key_t key;
    hwire_table_key_init(&key, 42); /* deterministic example seed */
    if (hwire_table_init(&app.headers, &key, HWIRE_TABLE_CASE_INSENSITIVE,
                         app.header_entries, HEADER_CAPACITY, app.header_index,
                         HWIRE_TABLE_SLOTS_CAP_2N) != HWIRE_TABLE_OK ||
        hwire_table_init(&app.query_params, &key, HWIRE_TABLE_CASE_SENSITIVE,
                         app.query_entries, QUERY_CAPACITY, app.query_index,
                         HWIRE_TABLE_SLOTS_CAP_2N) != HWIRE_TABLE_OK) {
        return 1;
    }

    app.ctx.uctx = &app;
    app.ctx.request_cb = on_request;
    app.ctx.header_cb = on_header;
    app.ctx.query_cb = on_query;
    app.ctx.qrybuf.buf = app.query_storage;
    app.ctx.qrybuf.size = sizeof(app.query_storage);
    size_t pos = 0;
    int result = hwire_parse_request(&app.ctx, input, sizeof input - 1, &pos,
                                     sizeof input - 1);
    if (result != HWIRE_OK) {
        fprintf(stderr, "parse error: %d, query result: %d, table result: %d\n",
                result, app.query_result, app.table_result);
        return 1;
    }

    printf("%.*s %.*s\n", (int)app.request.method.len, app.request.method.ptr,
           (int)app.request.uri.len, app.request.uri.ptr);

    hwire_table_iter_t duplicates = {.table = NULL};
    printf("header x-tag: ");
    for (const hwire_kv_pair_t *e = hwire_table_get_ci(&app.headers, "X-TAG", 5, &duplicates);
         e != NULL; e = hwire_table_next_ci(&duplicates)) {
        fwrite(e->value.ptr, 1, e->value.len, stdout);
    }
    putchar('\n');

    const hwire_kv_pair_t *name = hwire_table_get(&app.query_params, "name", 4, NULL);
    if (name != NULL) {
        printf("query name: %.*s\n", (int)name->value.len, name->value.ptr);
    }
    printf("query tag: ");
    for (const hwire_kv_pair_t *e = hwire_table_get(&app.query_params, "tag", 3, &duplicates);
         e != NULL; e = hwire_table_next(&duplicates)) {
        fwrite(e->value.ptr, 1, e->value.len, stdout);
    }
    putchar('\n');

    puts("headers:");
    hwire_table_iter_t iter = {.table = NULL};
    const hwire_kv_pair_t *e;
    while ((e = hwire_table_iterate(&app.headers, &iter)) != NULL) {
        printf("%.*s: %.*s\n", (int)e->key.len, e->key.ptr,
               (int)e->value.len, e->value.ptr);
    }
    return 0;
}
```

Save the example as `table_example.c` and compile both sources. The AES flags
in [Building](#building-with-hwire_table) can be added for the target CPU:

```sh
cc -std=c99 -Isrc table_example.c src/hwire.c src/hwire_table.c -o table_example
```

Expected output:

```text
GET /index.html?name=Alice+Smith&tag=A&tag=B
header x-tag: ABC
query name: Alice Smith
query tag: AB
headers:
Host: example.com
X-Tag: A
x-tag: B
X-Tag: C
```
