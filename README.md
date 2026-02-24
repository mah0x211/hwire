# hwire

[![test](https://github.com/mah0x211/hwire/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/hwire/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/hwire/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/hwire)

Zero-allocation `HTTP/1.1` parser written in `C99` or later.


---

## Features

- **Zero allocation** — no internal heap allocation; the caller owns all buffers.
- **Non-destructive** — the input buffer is never modified; `hwire_str_t` fields reference it directly.
- **EAGAIN-based streaming** — returns `HWIRE_EAGAIN` when the buffer is incomplete; re-submit the original buffer with more data appended and call again from offset 0. Stateless re-scan: no per-call resume state is maintained.
- **SIMD acceleration** — auto-selects `AVX2` / `SSE4.2` / `SSSE3` / `SSE2` on `x86-64`, `NEON` on `ARM64`; falls back to scalar code when none of those instruction sets are available at compile time.
- **HTTP/1.x grammar** — validates method tokens, URIs, header field names, header field values (including `obs-text`), `chunk-size`, and quoted-strings per **RFC 9110**, **RFC 9112**, **RFC 7230**, and **RFC 3986**.
- **`C99` or later / `C++` compatible** — single header + single source file; `extern "C"` guard included.


## RFC Compliance

This library parses `HTTP/1.x` **message framing and header field syntax**. Application-level HTTP semantics (content negotiation, conditional requests, authentication, caching) are outside its scope.

### Compliant

The following rules from [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110) and [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112) are fully implemented:

- **Token characters** (**RFC 9110** §5.6.2): all 76 `tchar` values recognized.
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

- **Bare `LF` as line terminator**: although senders MUST use `CRLF`, a bare `LF` (without a preceding `CR`) is also accepted at every line boundary — request-line, status-line, each header field line, and the end-of-headers blank line.
- **Leading `CR`/`LF` before message start**: any leading `CR` or `LF` bytes before the request-line or status-line are silently discarded.

### Partial

- **request-target** ([RFC 3986](https://www.rfc-editor.org/rfc/rfc3986) §2–3): allowed characters are validated, but structural parsing (scheme, authority, path, query) is not performed.
- **Chunked transfer encoding** ([RFC 9112](https://www.rfc-editor.org/rfc/rfc9112) §7): the chunk-size line (§7.1.1) is parsed; chunk body data and trailer fields are not handled.

### Out of scope

Message body parsing, transfer-coding, and connection management (**RFC 9112** §6–9), and all application-level HTTP semantics (**RFC 9110** §6–12) are not implemented. This library parses the request/response line and header fields only.

---

## Requirements

- Any `C99` or later compiler; `stddef.h` and `stdint.h` are required
- For `SIMD` code paths: `GCC` ≥ 4.9, `clang` ≥ 3.5, or `MSVC` (with `<intrin.h>`
  for `_BitScanForward`/`_BitScanForward64`); `SIMD` is detected automatically via
  predefined macros (`__AVX2__`, `__SSE4_2__`, `__aarch64__`, etc.) and silently
  disabled on unsupported compilers, falling back to the scalar implementation

---

## Building

Copy `src/hwire.h` and `src/hwire.c` into your project and compile `hwire.c` together with your sources:

```sh
cc -std=c99 -Isrc -o myapp myapp.c src/hwire.c  # C11 or later also works
```

`SIMD` code paths are selected automatically at compile time based on the target architecture. To force a specific instruction set, pass the appropriate compiler flag (e.g., `-mavx2` for `AVX2` on `x86-64`); scalar fallback is used when no supported `SIMD` macro is defined.

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
    /* ctx->key_lc.buf holds the lowercase field name */
    printf("  %.*s: %.*s\n",
           (int)ctx->key_lc.len, ctx->key_lc.buf,
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

    char keybuf[256];
    hwire_ctx_t ctx = {0};
    ctx.key_lc.buf  = keybuf;
    ctx.key_lc.size = sizeof(keybuf);
    ctx.request_cb  = on_request;
    ctx.header_cb   = on_header;

    size_t pos = 0;
    int rc = hwire_parse_request(&ctx, data, strlen(data), &pos,
                                 UINT16_MAX, UINT8_MAX);
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
  host: example.com
  connection: close
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
| `HWIRE_EEOL` | −5 | Invalid end-of-line (expected `CRLF`) |
| `HWIRE_EHDRNAME` | −6 | Invalid header field name |
| `HWIRE_EHDRVALUE` | −7 | Invalid header field value |
| `HWIRE_EHDRLEN` | −8 | Header length exceeded `maxlen` |
| `HWIRE_ESTATUS` | −9 | Invalid HTTP status code |
| `HWIRE_EILSEQ` | −10 | Invalid byte sequence |
| `HWIRE_ERANGE` | −11 | Value out of range (e.g., chunk size) |
| `HWIRE_EEXTNAME` | −12 | Invalid chunk extension name |
| `HWIRE_EEXTVAL` | −13 | Invalid chunk extension value or missing EOL |
| `HWIRE_ENOBUFS` | −14 | Too many headers / parameters / extensions |
| `HWIRE_EKEYLEN` | −15 | Key length exceeds `ctx->key_lc.size` |
| `HWIRE_ECALLBACK` | −16 | A callback returned non-zero |
| `HWIRE_EURI` | −17 | Invalid URI character |

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

Used for `ctx.key_lc` to receive the lowercase header field name. When `size == 0` (zero-initialized default), lowercase conversion is skipped entirely. To enable lowercase key storage, set `buf` to a caller-allocated buffer and `size` to its capacity before calling any parse function.

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
    hwire_str_t          uri;
    hwire_http_version_t version; /* HWIRE_HTTP_V10 or HWIRE_HTTP_V11 */
} hwire_request_t;
```

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
    hwire_buf_t  key_lc; /* Lowercase-key buffer; set buf and size before parsing */

    int (*param_cb      )(struct hwire_ctx_st *ctx, hwire_param_t         *param);
    int (*chunksize_cb  )(struct hwire_ctx_st *ctx, uint32_t               size);
    int (*chunksize_ext_cb)(struct hwire_ctx_st *ctx, hwire_chunksize_ext_t *ext);
    int (*header_cb     )(struct hwire_ctx_st *ctx, hwire_header_t        *header);
    int (*request_cb    )(struct hwire_ctx_st *ctx, hwire_request_t       *req);
    int (*response_cb   )(struct hwire_ctx_st *ctx, hwire_response_t      *rsp);
} hwire_ctx_t;
```

**Required fields** per function:

| Parse function | Required callbacks | `key_lc` |
|---|---|:---:|
| `hwire_parse_parameters` | `param_cb` | optional |
| `hwire_parse_chunksize` | `chunksize_cb` | — |
| `hwire_parse_headers` | `header_cb` | optional |
| `hwire_parse_request` | `request_cb`, `header_cb` | optional |
| `hwire_parse_response` | `response_cb`, `header_cb` | optional |

> **`key_lc`**: when `key_lc.size > 0`, `key_lc.buf` must point to a caller-allocated buffer of at least `key_lc.size` bytes; the library writes the lowercase field/parameter name there before each callback. Set `size = 0` (zero-initialized default) to disable lowercase key storage.

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
ctx.key_lc.buf   = keybuf;
ctx.key_lc.size  = sizeof(keybuf);
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
- `pos` — in/out: start offset on entry, first non-tchar offset on return (must not be NULL).

#### `hwire_parse_vchar`

```c
size_t hwire_parse_vchar(const char *str, size_t len, size_t *pos);
```

Same as `hwire_parse_tchar` but for vchar (visible ASCII + obs-text).

**Parameters**

- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: start offset on entry, first non-vchar offset on return (must not be NULL).


#### `hwire_parse_fcchar`

```c
size_t hwire_parse_fcchar(const char *str, size_t len, size_t *pos);
```

Advances `*pos` past consecutive field-content characters (VCHAR, obs-text, SP, HTAB) starting at `str[*pos]`. Returns the number of characters consumed (`0` if `str[*pos]` is not fcchar). Stops at CR, LF, NUL, DEL, or any other CTL.

This is the superset of `hwire_parse_vchar`: it additionally accepts SP and HTAB, which are valid within an HTTP field-value per **RFC 9110 §5.5**.

**Parameters**

- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: start offset on entry, first non-fcchar offset on return (must not be NULL).

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
- `maxlen` — maximum content length (bytes between the quotes).

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | Valid quoted-string consumed |
| `HWIRE_EAGAIN` | No closing `"` seen yet |
| `HWIRE_EILSEQ` | Invalid character inside the string |
| `HWIRE_ELEN` | Content length exceeds `maxlen` |

#### `hwire_parse_parameters`

```c
int hwire_parse_parameters(hwire_ctx_t *ctx, const char *str, size_t len,
                           size_t *pos, size_t maxlen, uint8_t maxnparams,
                           int skip_leading_semicolon);
```

Parses a semicolon-separated parameter list per **RFC 7230**:

```
parameters = *( OWS ";" OWS [ parameter ] )
parameter  = parameter-name "=" parameter-value
```

`ctx->param_cb` is called for each parameter; `ctx->key_lc` receives the lowercase parameter name. On `HWIRE_OK`, inspect `str[*pos]` to determine what follows (e.g., `\r\n`, end of data).

**Parameters**

- `ctx` — parser context (`param_cb` must not be NULL).
- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — in/out: start offset on entry, end offset on return (must not be NULL).
- `maxlen` — maximum string length.
- `maxnparams` — maximum number of parameters.
- `skip_leading_semicolon` — non-zero to accept the first parameter without a leading `;`.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | All parameters consumed |
| `HWIRE_EAGAIN` | More data needed |
| `HWIRE_EILSEQ` | Invalid byte sequence |
| `HWIRE_ELEN` | Length exceeds `maxlen` |
| `HWIRE_EKEYLEN` | Key length exceeds `ctx->key_lc.size` |
| `HWIRE_ECALLBACK` | Callback returned non-zero |
| `HWIRE_ENOBUFS` | Parameter count exceeds `maxnparams` |

---

### HTTP Parsing

#### `hwire_parse_chunksize`

```c
int hwire_parse_chunksize(hwire_ctx_t *ctx, const char *str, size_t len,
                          size_t *pos, size_t maxlen, uint8_t maxexts);
```

Parses a chunked-encoding size line per [RFC 9112 §7.1](https://www.rfc-editor.org/rfc/rfc9112#section-7.1):

```
chunk-size = 1*HEXDIG
chunk-ext  = *( BWS ";" BWS chunk-ext-name [ BWS "=" BWS chunk-ext-val ] )
```

`ctx->chunksize_cb` is called once with the parsed size; `ctx->chunksize_ext_cb` is called for each extension (optional). On success, `*pos` is advanced past the trailing `CRLF`.

**Parameters**

- `ctx` — parser context (`chunksize_cb` must not be NULL).
- `str` — input string (must not be NULL; `*pos` must be `0` on entry).
- `len` — total bytes in `str`.
- `pos` — out: bytes consumed from `str[0]` including the trailing `CRLF` (must not be NULL).
- `maxlen` — maximum line length in bytes.
- `maxexts` — maximum number of chunk extensions.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | Chunk-size line consumed including `CRLF` |
| `HWIRE_EAGAIN` | More data needed |
| `HWIRE_ELEN` | Length exceeds `maxlen` |
| `HWIRE_ERANGE` | Chunk size exceeds `HWIRE_MAX_CHUNKSIZE` |
| `HWIRE_EILSEQ` | Invalid byte sequence |
| `HWIRE_EEOL` | Invalid end-of-line terminator |
| `HWIRE_EEXTNAME` | Invalid extension name |
| `HWIRE_EEXTVAL` | Invalid extension value or missing `CRLF` |
| `HWIRE_ECALLBACK` | Callback returned non-zero |
| `HWIRE_ENOBUFS` | Extension count exceeds `maxexts` |

#### `hwire_parse_headers`

```c
int hwire_parse_headers(hwire_ctx_t *ctx, const char *str, size_t len,
                        size_t *pos, size_t maxlen, uint8_t maxnhdrs);
```

Parses HTTP header fields until the empty line (`CRLF CRLF` boundary). `ctx->header_cb` is called for each field; `ctx->key_lc.buf` is populated with the lowercase field name before each callback.

**Parameters**

- `ctx` — parser context (`header_cb` must not be NULL).
- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — out: bytes consumed from `str[0]` including the empty-line `CRLF` (must not be NULL).
- `maxlen` — maximum individual header length in bytes.
- `maxnhdrs` — maximum number of header fields.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | All headers consumed including the empty line |
| `HWIRE_EAGAIN` | More data needed |
| `HWIRE_EHDRNAME` | Invalid header field name |
| `HWIRE_EHDRVALUE` | Invalid header field value |
| `HWIRE_EHDRLEN` | Header length exceeds `maxlen` |
| `HWIRE_EEOL` | Invalid end-of-line in header value (CR without LF) |
| `HWIRE_ENOBUFS` | Header count exceeds `maxnhdrs` |
| `HWIRE_EKEYLEN` | Key length exceeds `ctx->key_lc.size` |
| `HWIRE_ECALLBACK` | Callback returned non-zero |

#### `hwire_parse_request`

```c
int hwire_parse_request(hwire_ctx_t *ctx, const char *str, size_t len,
                        size_t *pos, size_t maxlen, uint8_t maxnhdrs);
```

Parses a full `HTTP/1.x` request (request-line + headers). `ctx->request_cb` is called once for the request line, then `ctx->header_cb` for each header field. Returns `HWIRE_OK` when the empty line terminating the headers has been consumed.

```
GET /index.html HTTP/1.1\r\n
Host: example.com\r\n
\r\n
```

**Parameters**

- `ctx` — parser context (`request_cb` and `header_cb` must not be NULL).
- `str` — input string (must not be NULL).
- `len` — total bytes in `str`.
- `pos` — out: bytes consumed from `str[0]`; pass `0` on entry, reset to `0` when retrying after `HWIRE_EAGAIN` (must not be NULL).
- `maxlen` — maximum message length in bytes.
- `maxnhdrs` — maximum number of header fields.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | Full request consumed including the empty line |
| `HWIRE_EAGAIN` | More data needed |
| `HWIRE_EMETHOD` | Invalid method (not tchar or missing SP) |
| `HWIRE_EVERSION` | Unsupported HTTP version |
| `HWIRE_EEOL` | Invalid end-of-line |
| `HWIRE_ELEN` | Length exceeds `maxlen` |
| `HWIRE_EURI` | Invalid URI character |
| `HWIRE_EHDRNAME` | Invalid header field name |
| `HWIRE_EHDRVALUE` | Invalid header field value |
| `HWIRE_EHDRLEN` | Header length exceeds `maxlen` |
| `HWIRE_EKEYLEN` | Key length exceeds `ctx->key_lc.size` |
| `HWIRE_ECALLBACK` | Callback returned non-zero |
| `HWIRE_ENOBUFS` | Header count exceeds `maxnhdrs` |

#### `hwire_parse_response`

```c
int hwire_parse_response(hwire_ctx_t *ctx, const char *str, size_t len,
                         size_t *pos, size_t maxlen, uint8_t maxnhdrs);
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
- `pos` — out: bytes consumed from `str[0]`; pass `0` on entry, reset to `0` when retrying after `HWIRE_EAGAIN` (must not be NULL).
- `maxlen` — maximum message length in bytes.
- `maxnhdrs` — maximum number of header fields.

**Returns**

| Return | Condition |
|--------|-----------|
| `HWIRE_OK` | Full response consumed including the empty line |
| `HWIRE_EAGAIN` | More data needed |
| `HWIRE_ESTATUS` | Invalid HTTP status code |
| `HWIRE_EVERSION` | Unsupported HTTP version |
| `HWIRE_EEOL` | Invalid end-of-line |
| `HWIRE_EILSEQ` | Invalid character in reason phrase |
| `HWIRE_ELEN` | Length exceeds `maxlen` |
| `HWIRE_EHDRNAME` | Invalid header field name |
| `HWIRE_EHDRVALUE` | Invalid header field value |
| `HWIRE_EHDRLEN` | Header length exceeds `maxlen` |
| `HWIRE_EKEYLEN` | Key length exceeds `ctx->key_lc.size` |
| `HWIRE_ECALLBACK` | Callback returned non-zero |
| `HWIRE_ENOBUFS` | Header count exceeds `maxnhdrs` |

---

## Streaming and EAGAIN

`hwire` is designed for incremental I/O. When the input buffer does not yet contain a complete message, the parse function returns `HWIRE_EAGAIN`. The caller must then:

1. Keep the data received so far.
2. Read more bytes from the network and append them to the buffer.
3. Call the parse function again, passing the buffer from its start (`buf[0]`) with the updated `filled` length.

The function re-scans from the start on each call, so there is no resume state to manage. The trade-off is that re-scanning is inexpensive compared to the complexity of maintaining per-call state.

```c
char buf[4096];
size_t filled = 0;

for (;;) {
    ssize_t n = recv(fd, buf + filled, sizeof(buf) - filled, 0);
    if (n <= 0) break; /* connection closed or error */
    filled += (size_t)n;

    size_t pos = 0;
    int rc = hwire_parse_response(&ctx, buf, filled, &pos,
                                  UINT16_MAX, UINT8_MAX);
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

> **Note:** `hwire_parse_parameters` is designed for parsing a single header field value that has already been fully received. It does not return `HWIRE_EAGAIN`.

