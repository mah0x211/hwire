#include "test_helpers.h"

/*
 * Covers: RFC 9112 §5.1  header-field = field-name ":" OWS field-value OWS CRLF
 *         RFC 9110 §5.1  field-name   = token = 1*tchar
 *         RFC 9110 §5.5  field-value  = *( field-content / obs-fold )
 *                        field-content = field-vchar [ 1*( SP / HTAB /
 * field-vchar ) field-vchar ] field-vchar  = VCHAR / obs-text OWS = *( SP /
 * HTAB )
 */
void test_parse_headers_valid(void)
{
    TEST_START("test_parse_headers_valid");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Basic valid headers */
    buf = "Host: example.com\r\nConnection: close\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* hwire: MUST return HWIRE_ENOBUFS if the max header count is exceeded. */
    buf = "H1: v1\r\nH2: v2\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 1);
    ASSERT_EQ(rv, HWIRE_ENOBUFS);

    /* hwire: maxlen limits total (key + value) length; MUST return
       HWIRE_EHDRLEN if exceeded. */
    buf = "VeryLongKey: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 5, 10);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    /* RFC 9110 §5.1: field-name = token = 1*tchar; '@' (0x40) is not tchar
       (MUST reject) */
    buf = "@Invalid: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* RFC 9112 §2.2: CRLF = CR LF; bare CR not followed by LF MUST be
       rejected → HWIRE_EEOL */
    buf = "Key: value\r\t\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EEOL);

    /* RFC 9110 §5.5: field-vchar starts at 0x21; CTL 0x01 is not field-vchar
       (MUST reject) */
    buf = "K: \x01\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EHDRVALUE);

    { /* hwire: MUST return HWIRE_EKEYLEN if the key_lc buffer is too small. */
        hwire_ctx_t cb_small = cb;
        cb_small.key_lc.size = 2;
        buf                  = "Key: val\r\n\r\n";
        pos                  = 0;
        rv = hwire_parse_headers(&cb_small, buf, strlen(buf), &pos, 1024, 10);
        ASSERT_EQ(rv, HWIRE_EKEYLEN);
    }

    /* RFC 9110 §5.5: HTAB is valid between field-vchar chars in field-content
       (MUST accept) */
    buf = "Key: val\tue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* RFC 9110 §5.5: field-value = *( field-content / obs-fold ); zero
       characters is valid (MUST accept) */
    buf = "H1:\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    TEST_END();
}

void test_parse_headers_fail(void)
{
    TEST_START("test_parse_headers_fail");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb_fail
    };
    size_t pos      = 0;
    const char *buf = "Key: Value\r\n\r\n";
    int rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
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
void test_parse_headers_ows_handling(void)
{
    TEST_START("test_parse_headers_ows_handling");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* OWS followed by VCHAR */
    buf = "Key: val  ue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* RFC 9110 §5.5: trailing OWS (SP before CRLF) MUST be stripped from
       field-value */
    buf = "Key: value  \r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    TEST_END();
}

void test_parse_headers_cr_handling(void)
{
    TEST_START("test_parse_headers_cr_handling");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* CR followed by null terminator in value */
    buf = "Key: val\r";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

void test_parse_headers_invalid_values(void)
{
    TEST_START("test_parse_headers_invalid_values");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* Value exceeds maxlen */
    buf = "Key: verylongvalue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 8, 10);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    TEST_END();
}

void test_parse_headers_key_parsing(void)
{
    TEST_START("test_parse_headers_key_parsing");

    hwire_ctx_t cb_no_lc = {
        .key_lc    = {.buf = NULL, .size = 0, .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* No key_lc buffer */
    buf = "Key: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb_no_lc, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* Non-tchar char in key with no key_lc buffer */
    buf = "Ke@y: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb_no_lc, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };

    /* Key without colon, needs more data */
    buf = "KeyWithoutColon";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, strlen(buf), 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

void test_parse_headers_empty_and_eol(void)
{
    TEST_START("test_parse_headers_empty_and_eol");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Empty string */
    buf = "";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, 0, &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR at end of input (incomplete) */
    buf = "\r";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by non-LF */
    buf = "\rX";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    TEST_END();
}

void test_parse_headers_ows_maxlen(void)
{
    TEST_START("test_parse_headers_ows_maxlen");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* OWS skip exceeds maxlen */
    buf = "K:     value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 4, 10);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5  field-value scanning at maxlen boundary
 * MUST: a header value whose content is exactly (maxlen-1) bytes followed by
 *       CRLF MUST be accepted.  Previously, parse_hval checked
 *       `pos+1 < max` (scan limit) instead of `pos+1 < len` (buffer limit)
 *       and incorrectly returned HWIRE_EEOL when CR landed at max-1 while
 *       the corresponding LF was available at max (still within len).
 *
 * Internal detail: hwire_parse_headers passes vlen = maxlen - (key+colon+OWS)
 * to parse_hval as the value-content limit.  For "K: value\r\n":
 *   key="K"(1) + ":"(1) + " "(1) = 3 bytes consumed before value
 *   vlen = maxlen - 3
 * So with maxlen=10, vlen=7.  Boundary case: value is 6 bytes = vlen-1.
 */
void test_parse_headers_hval_maxlen_boundary(void)
{
    TEST_START("test_parse_headers_hval_maxlen_boundary");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;

    /* maxlen=10, vlen=7; value="123456" (6 bytes = vlen-1): boundary case
     * CR lands at pos=6 == vlen-1 == max-1.
     * Bug: `pos+1 < max` → 7 < 7 → false → fell through to HWIRE_EEOL.
     * Fix: `pos+1 < len` → 7 < 12 → true → str[7]=='\\n' → HWIRE_OK. */
    const char *buf = "K: 123456\r\n\r\n";
    pos             = 0;
    rv              = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 10, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* value="12345" (5 bytes, well within vlen-1): always worked */
    buf = "K: 12345\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 10, 10);
    ASSERT_OK(rv);

    /* value="1234567" (7 bytes = vlen, exceeds maxlen): HWIRE_EHDRLEN */
    buf = "K: 1234567\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 10, 10);
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
void test_parse_headers_allows_empty_value(void)
{
    TEST_START("test_parse_headers_allows_empty_value");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = check_empty_value_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Empty header value */
    buf = "Empty-Val:\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* OWS then empty */
    buf = "Empty-Val:   \r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    TEST_END();
}

void test_parse_headers_rfc_compliance(void)
{
    TEST_START("test_parse_headers_rfc_compliance");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
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
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* RFC 9112 §5.2: obs-fold is deprecated and MUST be rejected.
     *   obs-fold = OWS CRLF 1*( SP / HTAB )
     * A continuation line starting with SP/HTAB is parsed as a header whose
     * name begins with SP → not a valid tchar → HWIRE_EHDRNAME. */
    buf = "Key: Value\r\n Folded\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* hwire: bare LF as field-value line terminator (lenient; RFC 9112 §2.2
       SHOULD accept bare LF); bare LF also triggers end-of-headers on next
       iteration */
    buf = "Key: value\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* hwire: bare LF as end-of-headers marker (lenient; RFC 9112 §2.2 SHOULD
       accept bare LF in place of CRLF) */
    buf = "Key: value\r\n\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
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
void test_parse_headers_obstext(void)
{
    TEST_START("test_parse_headers_obstext");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* obs-text bytes as the entire field-value */
    buf = "X-Obs: \x80\xff\xa5\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* obs-text mixed with VCHAR */
    buf = "X-Mix: abc\x80xyz\xff\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* HTAB embedded between obs-text and VCHAR (field-content) */
    buf = "X-Tab: \x80\tvalue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
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

void test_parse_headers_ows_exact(void)
{
    TEST_START("test_parse_headers_ows_exact");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = capture_header_value_len_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* "value" (5 bytes) + 3 trailing SPs → stripped value.len MUST be 5 */
    buf                  = "K: value   \r\n\r\n";
    pos                  = 0;
    g_captured_value_len = 0;
    rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(g_captured_value_len, 5);

    /* "value" (5 bytes) + 1 trailing HTAB → stripped value.len MUST be 5 */
    buf                  = "K: value\t\r\n\r\n";
    pos                  = 0;
    g_captured_value_len = 0;
    rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(g_captured_value_len, 5);

    /* "value" (5 bytes) + mixed SP/HTAB trailing OWS → stripped value.len
     * MUST be 5 */
    buf                  = "K: value \t \r\n\r\n";
    pos                  = 0;
    g_captured_value_len = 0;
    rv = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
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
void test_parse_headers_simd_boundary(void)
{
    TEST_START("test_parse_headers_simd_boundary");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
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
        rv  = hwire_parse_headers(&cb, buf, nlen + 7, &pos, 1024, 10);
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
    rv  = hwire_parse_headers(&cb, buf, 24, &pos, 1024, 10);
    ASSERT_OK(rv);

    /* Field-values at SIMD boundary lengths: 16, 32 bytes */
    static const size_t val_lens[] = {16, 32};
    for (size_t i = 0; i < 2; i++) {
        size_t vlen = val_lens[i];
        memcpy(buf, "K: ", 3);
        memset(buf + 3, 'a', vlen);
        memcpy(buf + 3 + vlen, "\r\n\r\n", 4);
        pos = 0;
        rv  = hwire_parse_headers(&cb, buf, 3 + vlen + 4, &pos, 1024, 10);
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
void test_parse_headers_streaming(void)
{
    TEST_START("test_parse_headers_streaming");

    const char *full = "Host: example.com\r\nContent-Length: 0\r\n\r\n";
    size_t full_len  = strlen(full);
    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;

    /* Every prefix of length 1..full_len-1 MUST give HWIRE_EAGAIN */
    for (size_t i = 1; i < full_len; i++) {
        cb.key_lc.len = 0;
        pos           = 0;
        rv            = hwire_parse_headers(&cb, full, i, &pos, 1024, 10);
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
    cb.key_lc.len = 0;
    pos           = 0;
    rv            = hwire_parse_headers(&cb, full, full_len, &pos, 1024, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, full_len);

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
    print_test_summary();
    return g_tests_failed;
}
