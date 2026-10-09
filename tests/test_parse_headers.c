#include "test_helpers.h"

/*
 * Covers: RFC 9112 §5.1  header-field = field-name ":" OWS field-value OWS CRLF
 *         RFC 9110 §5.1  field-name   = token = 1*tchar
 *         RFC 9110 §5.5  field-value  = *( field-content / obs-fold )
 *                        field-content = field-vchar [ 1*( SP / HTAB /
 * field-vchar ) field-vchar ] field-vchar  = VCHAR / obs-text OWS = *( SP /
 * HTAB )
 */
static void test_parse_headers_valid(void)
{
    TEST_START("test_parse_headers_valid");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Basic valid headers */
    buf = "Host: example.com\r\nConnection: close\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* Capacity failure is reported through the application callback. */
    {
        test_capacity_t storage = {.capacity = 1};
        hwire_ctx_t limited = cb;
        limited.uctx = &storage;
        limited.header_cb = capacity_pair_cb;
        buf = "H1: v1\r\nH2: v2\r\n\r\n";
        pos = 0;
        rv = hwire_parse_headers(&limited, buf, strlen(buf), &pos, 1024);
        ASSERT_EQ(rv, HWIRE_ECALLBACK);
        ASSERT_EQ(storage.error, HWIRE_ENOBUFS);
        ASSERT_EQ(storage.count, 1);
        ASSERT_EQ(storage.calls, 2);
    }

    /* hwire: maxlen limits total (key + value) length; MUST return
       HWIRE_EHDRLEN if exceeded. */
    buf = "VeryLongKey: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 5);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    /* RFC 9110 §5.1: field-name = token = 1*tchar; '@' (0x40) is not tchar
       (MUST reject) */
    buf = "@Invalid: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* RFC 9112 §2.2: CRLF = CR LF; bare CR not followed by LF MUST be
       rejected → HWIRE_EEOL */
    buf = "Key: value\r\t\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_EQ(rv, HWIRE_EEOL);

    /* RFC 9110 §5.5: field-vchar starts at 0x21; CTL 0x01 is not field-vchar
       (MUST reject) */
    buf = "K: \x01\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_EQ(rv, HWIRE_EHDRVALUE);

    /* RFC 9110 §5.5: HTAB is valid between field-vchar chars in field-content
       (MUST accept) */
    buf = "Key: val\tue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    /* RFC 9110 §5.5: field-value = *( field-content / obs-fold ); zero
       characters is valid (MUST accept) */
    buf = "H1:\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    TEST_END();
}

static void test_parse_headers_fail(void)
{
    TEST_START("test_parse_headers_fail");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb_fail
    };
    size_t pos      = 0;
    const char *buf = "Key: Value\r\n\r\n";
    int rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5  OWS = *( SP / HTAB )
 * MUST: SP/HTAB embedded between field-vchar characters is part of
 * field-content (not stripped). OWS stripping only applies to leading/trailing
 * whitespace of the field-value.
 * MUST: trailing OWS (SP/HTAB before CRLF) MUST be stripped from field-value.
 */
static void test_parse_headers_ows_handling(void)
{
    TEST_START("test_parse_headers_ows_handling");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* OWS followed by VCHAR */
    buf = "Key: val  ue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    /* RFC 9110 §5.5: trailing OWS (SP before CRLF) MUST be stripped from
       field-value */
    buf = "Key: value  \r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    TEST_END();
}

static void test_parse_headers_cr_handling(void)
{
    TEST_START("test_parse_headers_cr_handling");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* CR followed by null terminator in value */
    buf = "Key: val\r";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* bare LF as field-value terminator: pos MUST consume the entire input
     * including the LF and the following CRLF end-of-headers marker */
    buf = "Key: value\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf)); /* all 13 bytes consumed */

    /* bare LF as field-value terminator: pos MUST consume the entire input
     * when end-of-headers is also a bare LF */
    buf = "Key: value\n\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf)); /* all 12 bytes consumed */

    /* multiple headers separated by bare LF: all headers MUST be parsed
     * correctly and pos MUST equal the full input length */
    buf = "Key1: v1\nKey2: v2\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf)); /* all 20 bytes consumed */

    TEST_END();
}

static void test_parse_headers_invalid_values(void)
{
    TEST_START("test_parse_headers_invalid_values");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* Value exceeds maxlen */
    buf = "Key: verylongvalue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 8);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    TEST_END();
}

static void test_parse_headers_key_parsing(void)
{
    TEST_START("test_parse_headers_key_parsing");

    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Valid token key */
    buf = "Key: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    /* Non-tchar character in key */
    buf = "Ke@y: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* Key without colon exhausts the available header budget. */
    buf = "KeyWithoutColon";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, strlen(buf));
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    TEST_END();
}

static void test_parse_headers_empty_and_eol(void)
{
    TEST_START("test_parse_headers_empty_and_eol");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Empty string */
    buf = "";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, 0, &pos, 1024);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR at end of input (incomplete) */
    buf = "\r";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by non-LF is an invalid empty-line terminator */
    buf = "\rX";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_EQ(rv, HWIRE_EEOL);

    TEST_END();
}

static void test_parse_headers_ows_maxlen(void)
{
    TEST_START("test_parse_headers_ows_maxlen");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* OWS skip exceeds maxlen */
    buf = "K:     value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 4);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5 field-value scanning at the header-block maxlen
 * boundary. The field line and the empty line terminating the block must both
 * fit within maxlen.
 */
static void test_parse_headers_hval_maxlen_boundary(void)
{
    TEST_START("test_parse_headers_hval_maxlen_boundary");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    int rv     = 0;

    /* "K: 123456\r\n\r\n" is exactly 13 bytes. */
    const char *buf = "K: 123456\r\n\r\n";
    pos             = 0;
    rv              = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 13);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 12);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    /* "K: 12345\r\n\r\n" is exactly 12 bytes. */
    buf = "K: 12345\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 12);
    ASSERT_OK(rv);

    /* "K: 1234567\r\n\r\n" exceeds maxlen=13. */
    buf = "K: 1234567\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 13);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    TEST_END();
}

static int check_empty_value_cb(hwire_ctx_t *ctx, hwire_header_t *header)
{
    (void)ctx;
    if (header->value.len == 0) {
        return 0;
    }
    return 1; // Fail if not empty
}

/*
 * Covers: RFC 9110 §5.5  field-value = *( field-content / obs-fold )
 *         OWS = *( SP / HTAB )
 * MUST: empty field-value (zero content after OWS stripping) MUST be accepted.
 * MUST: field-value consisting entirely of OWS results in an empty value after
 * stripping.
 */
static void test_parse_headers_allows_empty_value(void)
{
    TEST_START("test_parse_headers_allows_empty_value");
    hwire_ctx_t cb = {
        .header_cb = check_empty_value_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Empty header value */
    buf = "Empty-Val:\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    /* OWS then empty */
    buf = "Empty-Val:   \r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    TEST_END();
}

static void test_parse_headers_rfc_compliance(void)
{
    TEST_START("test_parse_headers_rfc_compliance");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* RFC 9112 §5.1: No whitespace is allowed between field-name and ":".
     * MUST reject: SP before ":" — the SP terminates the token before ":" is
     * seen → HWIRE_EHDRNAME. */
    buf = "Key : Value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* RFC 9112 §5.2: obs-fold is deprecated and MUST be rejected.
     *   obs-fold = OWS CRLF 1*( SP / HTAB )
     * A continuation line starting with SP/HTAB is parsed as a header whose
     * name begins with SP → not a valid tchar → HWIRE_EHDRNAME. */
    buf = "Key: Value\r\n Folded\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* hwire: bare LF as field-value line terminator (lenient; RFC 9112 §2.2
       SHOULD accept bare LF); bare LF also triggers end-of-headers on next
       iteration */
    buf = "Key: value\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    /* hwire: bare LF as end-of-headers marker (lenient; RFC 9112 §2.2 SHOULD
       accept bare LF in place of CRLF) */
    buf = "Key: value\r\n\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5  field-vchar = VCHAR / obs-text
 *                        obs-text    = %x80-FF
 * MUST: obs-text bytes (0x80-0xFF) MUST be accepted as field-vchar in
 * field-value.  MUST: HTAB embedded between field-vchar characters MUST be
 * accepted as part of field-content.
 */
static void test_parse_headers_obstext(void)
{
    TEST_START("test_parse_headers_obstext");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* obs-text bytes as the entire field-value */
    buf = "X-Obs: \x80\xff\xa5\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    /* obs-text mixed with VCHAR */
    buf = "X-Mix: abc\x80xyz\xff\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    /* HTAB embedded between obs-text and VCHAR (field-content) */
    buf = "X-Tab: \x80\tvalue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5  OWS = *( SP / HTAB )
 * MUST: trailing OWS (SP / HTAB before CRLF) MUST be stripped; the
 * field-value length reported via the callback MUST reflect the stripped
 * length exactly.
 */
static size_t g_captured_value_len = 0;
static int capture_header_value_len_cb(hwire_ctx_t *ctx, hwire_header_t *header)
{
    (void)ctx;
    g_captured_value_len = header->value.len;
    return 0;
}

static void test_parse_headers_ows_exact(void)
{
    TEST_START("test_parse_headers_ows_exact");
    hwire_ctx_t cb = {
        .header_cb = capture_header_value_len_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* "value" (5 bytes) + 3 trailing SPs → stripped value.len MUST be 5 */
    buf                  = "K: value   \r\n\r\n";
    pos                  = 0;
    g_captured_value_len = 0;
    rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(g_captured_value_len, 5);

    /* "value" (5 bytes) + 1 trailing HTAB → stripped value.len MUST be 5 */
    buf                  = "K: value\t\r\n\r\n";
    pos                  = 0;
    g_captured_value_len = 0;
    rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(g_captured_value_len, 5);

    /* "value" (5 bytes) + mixed SP/HTAB trailing OWS → stripped value.len
     * MUST be 5 */
    buf                  = "K: value \t \r\n\r\n";
    pos                  = 0;
    g_captured_value_len = 0;
    rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(g_captured_value_len, 5);

    TEST_END();
}

/*
 * Covers: SIMD boundary behaviour in header name and value scanning.
 * MUST: header names of exactly 15/16/17 tchar characters MUST be fully
 * parsed.  MUST: field-values of 16/32 field-vchar characters MUST be fully
 * consumed.  MUST: '|' (0x7C) and '~' (0x7E) — highest valid tchar — MUST be
 * accepted in header names at boundary positions.
 */
static void test_parse_headers_simd_boundary(void)
{
    TEST_START("test_parse_headers_simd_boundary");
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    char buf[256];
    size_t pos;
    int rv;

    /* Header names at SIMD boundary lengths: 15, 16, 17 bytes */
    static const size_t name_lens[] = {15, 16, 17};
    for (size_t i = 0; i < 3; i++) {
        size_t nlen = name_lens[i];
        memset(buf, 'a', nlen);
        memcpy(buf + nlen, ": v\r\n\r\n", 7);
        pos = 0;
        rv  = hwire_parse_headers(&cb, buf, nlen + 7, &pos, 1024);
        if (rv != HWIRE_OK) {
            fprintf(stderr, "FAILED: %s:%d: name_len=%zu gave rv=%d\n",
                    __FILE__, __LINE__, nlen, rv);
            g_tests_failed++;
            return;
        }
    }

    /* '|' and '~' in header name at SIMD boundary positions */
    memset(buf, 'a', 17);
    buf[7]  = '|'; /* mid-name */
    buf[14] = '~'; /* last byte of 15-char chunk */
    memcpy(buf + 17, ": v\r\n\r\n", 7);
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, 24, &pos, 1024);
    ASSERT_OK(rv);

    /* Field-values at SIMD boundary lengths: 16, 32 bytes */
    static const size_t val_lens[] = {16, 32};
    for (size_t i = 0; i < 2; i++) {
        size_t vlen = val_lens[i];
        memcpy(buf, "K: ", 3);
        memset(buf + 3, 'a', vlen);
        memcpy(buf + 3 + vlen, "\r\n\r\n", 4);
        pos = 0;
        rv  = hwire_parse_headers(&cb, buf, 3 + vlen + 4, &pos, 1024);
        if (rv != HWIRE_OK) {
            fprintf(stderr, "FAILED: %s:%d: val_len=%zu gave rv=%d\n", __FILE__,
                    __LINE__, vlen, rv);
            g_tests_failed++;
            return;
        }
    }

    TEST_END();
}

/*
 * Covers: streaming / incremental delivery compatibility.
 * MUST: hwire_parse_headers() MUST return HWIRE_EAGAIN for every prefix of a
 * valid complete header block shorter than the full block.  MUST: return
 * HWIRE_OK with pos == strlen(full) once the complete block is provided.
 *
 * Simulates a TCP receiver that accumulates bytes and re-presents the full
 * buffer (pos=0) on each new arrival.  Uses the raw cumulative buffer without
 * NUL-padding; correct behaviour relies on the len-bounded bounds checks fixed
 * in hwire_parse_headers (RETRY len==0 guard, IS_OWS cur<len guard).
 */
static void test_parse_headers_streaming(void)
{
    TEST_START("test_parse_headers_streaming");

    const char *full = "Host: example.com\r\nContent-Length: 0\r\n\r\n";
    size_t full_len  = strlen(full);
    hwire_ctx_t cb = {
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;

    /* Every prefix of length 1..full_len-1 MUST give HWIRE_EAGAIN */
    for (size_t i = 1; i < full_len; i++) {
        pos           = 0;
        rv            = hwire_parse_headers(&cb, full, i, &pos, 1024);
        if (rv != HWIRE_EAGAIN) {
            fprintf(stderr,
                    "FAILED: %s:%d: expected HWIRE_EAGAIN at len=%zu, got "
                    "%d\n",
                    __FILE__, __LINE__, i, rv);
            g_tests_failed++;
            return;
        }
    }

    /* Full block MUST succeed with pos == full_len */
    pos           = 0;
    rv            = hwire_parse_headers(&cb, full, full_len, &pos, 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, full_len);

    TEST_END();
}

typedef struct {
    const char *name;
    size_t name_len;
    const char *value;
    size_t value_len;
    const char *buf;
    size_t buf_len;
    int called;
    int failed;
} hdr_verify_expect_t;

static int verify_hdr_content_cb(hwire_ctx_t *ctx, hwire_header_t *header)
{
    hdr_verify_expect_t *e = (hdr_verify_expect_t *)ctx->uctx;
    e->called++;
    if (!str_in_buf(header->key, e->buf, e->buf_len)) {
        fprintf(stderr, "header key: ptr out of range\n");
        e->failed = 1;
    }
    if (!str_in_buf(header->value, e->buf, e->buf_len)) {
        fprintf(stderr, "header value: ptr out of range\n");
        e->failed = 1;
    }
    if (header->key.len != e->name_len ||
        strncmp(header->key.ptr, e->name, e->name_len) != 0) {
        fprintf(stderr, "header name: expected '%.*s'(%zu), got '%.*s'(%zu)\n",
                (int)e->name_len, e->name, e->name_len, (int)header->key.len,
                header->key.ptr, header->key.len);
        e->failed = 1;
    }
    if (header->value.len != e->value_len ||
        strncmp(header->value.ptr, e->value, e->value_len) != 0) {
        fprintf(stderr, "header value: expected '%.*s'(%zu), got '%.*s'(%zu)\n",
                (int)e->value_len, e->value, e->value_len,
                (int)header->value.len, header->value.ptr, header->value.len);
        e->failed = 1;
    }
    return 0;
}

/*
 * Covers: exact content of parsed header field-name and field-value.
 * MUST: key.ptr/len MUST reference the original input bytes.
 * MUST: value.ptr/len MUST reflect the OWS-stripped value.
 */
static void test_parse_headers_content_verification(void)
{
    TEST_START("test_parse_headers_content_verification");

    /* Case 1: Content-Type: text/html */
    {
        hdr_verify_expect_t exp = {
            .name = "Content-Type",
            .name_len = 12,
            .value = "text/html",
            .value_len = 9
        };
        hwire_ctx_t cb = {
            .uctx      = &exp,
            .header_cb = verify_hdr_content_cb
        };
        size_t pos      = 0;
        const char *buf = "Content-Type: text/html\r\n\r\n";
        exp.buf         = buf;
        exp.buf_len     = strlen(buf);
        int rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
        ASSERT_OK(rv);
        ASSERT_EQ(exp.called, 1);
        ASSERT_EQ(exp.failed, 0);
    }

    /* Case 2: X-Custom: hello world */
    {
        hdr_verify_expect_t exp = {
            .name = "X-Custom",
            .name_len = 8,
            .value = "hello world",
            .value_len = 11
        };
        hwire_ctx_t cb = {
            .uctx      = &exp,
            .header_cb = verify_hdr_content_cb
        };
        size_t pos      = 0;
        const char *buf = "X-Custom: hello world\r\n\r\n";
        exp.buf         = buf;
        exp.buf_len     = strlen(buf);
        int rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
        ASSERT_OK(rv);
        ASSERT_EQ(exp.called, 1);
        ASSERT_EQ(exp.failed, 0);
    }

    /* Case 3: OWS-Key:   trimmed   → leading+trailing OWS stripped → "trimmed"
     */
    {
        hdr_verify_expect_t exp = {
            .name = "OWS-Key",
            .name_len = 7,
            .value = "trimmed",
            .value_len = 7
        };
        hwire_ctx_t cb          = {
            .uctx      = &exp,
            .header_cb = verify_hdr_content_cb
        };
        size_t pos      = 0;
        const char *buf = "OWS-Key:   trimmed   \r\n\r\n";
        exp.buf         = buf;
        exp.buf_len     = strlen(buf);
        int rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024);
        ASSERT_OK(rv);
        ASSERT_EQ(exp.called, 1);
        ASSERT_EQ(exp.failed, 0);
    }

    TEST_END();
}

int main(void)
{
    test_parse_headers_valid();
    test_parse_headers_fail();
    test_parse_headers_ows_handling();
    test_parse_headers_cr_handling();
    test_parse_headers_invalid_values();
    test_parse_headers_key_parsing();
    test_parse_headers_empty_and_eol();
    test_parse_headers_ows_maxlen();
    test_parse_headers_hval_maxlen_boundary();
    test_parse_headers_allows_empty_value();
    test_parse_headers_rfc_compliance();
    test_parse_headers_obstext();
    test_parse_headers_ows_exact();
    test_parse_headers_simd_boundary();
    test_parse_headers_streaming();
    test_parse_headers_content_verification();
    print_test_summary();
    return g_tests_failed;
}
