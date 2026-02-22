#include "test_helpers.h"

/*
 * Covers: RFC 9112 §3    request-line = method SP request-target SP
 * HTTP-version CRLF RFC 9112 §3.1  method       = token = 1*tchar RFC 9112 §2.3
 * HTTP-version = HTTP-name "/" DIGIT "." DIGIT HTTP-name    = %s"HTTP"
 * (case-sensitive) MUST: pos MUST equal total consumed bytes (request-line +
 * headers) after HWIRE_OK.
 */
void test_parse_request_valid(void)
{
    TEST_START("test_parse_request_valid");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos      = 0;
    const char *buf = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    int rv = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    TEST_END();
}

/*
 * Covers: hwire callback return value handling.
 * MUST: non-zero return from request_cb MUST cause hwire_parse_request() to
 * return HWIRE_ECALLBACK.
 */
void test_parse_request_cb_fail(void)
{
    TEST_START("test_parse_request_cb_fail");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb_fail,
        .header_cb  = mock_header_cb
    };
    size_t pos      = 0;
    const char *buf = "GET / HTTP/1.1\r\n\r\n";
    int rv = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

/*
 * Covers: RFC 9112 §3.1  method = token = 1*tchar
 * MUST reject: method starting with non-tchar character → HWIRE_EMETHOD
 * MUST reject: non-SP character after valid method token → HWIRE_EMETHOD
 * MUST return HWIRE_EAGAIN if method is incomplete (no SP encountered yet)
 * RFC 9112 §3.2: empty request-target (two consecutive SPs) MUST be rejected →
 * HWIRE_EURI
 */
void test_parse_request_method_errors(void)
{
    TEST_START("test_parse_request_method_errors");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* First char is not tchar */
    buf = "@GET / HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EMETHOD);

    /* Method without space (string ends) */
    buf = "GET";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Empty URI (GET  HTTP/1.1) */
    buf = "GET  HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    /* Should fail with HWIRE_EURI (empty request-target is invalid) */
    ASSERT_EQ(rv, HWIRE_EURI);

    /* Non-space char after method */
    buf = "GET@/ HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EMETHOD);

    TEST_END();
}

/*
 * Covers: RFC 9112 §2.3  HTTP-version = HTTP-name "/" DIGIT "." DIGIT
 *                        HTTP-name    = %s"HTTP"  (case-sensitive)
 * hwire accepts only HTTP/1.0 and HTTP/1.1; all other versions →
 * HWIRE_EVERSION. MUST reject: non-CRLF/LF character after the version string →
 * HWIRE_EVERSION.
 */
void test_parse_request_version_errors(void)
{
    TEST_START("test_parse_request_version_errors");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Unsupported HTTP version */
    buf = "GET / HTTP/2.0\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EVERSION);

    /* Non-CRLF char after version */
    buf = "GET / HTTP/1.1X";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EVERSION);

    TEST_END();
}

/*
 * Covers: RFC 9112 §3.2  request-target length limiting (hwire maxlen
 * parameter). MUST return HWIRE_ELEN if URI length (before SP) exceeds maxlen.
 * MUST return HWIRE_EAGAIN if SP after URI has not yet been received.
 */
void test_parse_request_uri_errors(void)
{
    TEST_START("test_parse_request_uri_errors");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* URI exceeds maxlen (no SP within maxlen) */
    buf = "GET /verylongpathwithoutspaces HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 10, 10);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* URI with SP within maxlen */
    buf = "GET /sp HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 5, 10);
    ASSERT_OK(rv);

    /* No space after URI */
    buf = "GET /path";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

/*
 * Covers: RFC 9112 §2.2  line termination
 *   A sender MUST use CRLF as line terminator.
 *   A recipient MUST recognize a single LF as a line terminator.
 *   RFC 9112 §3: leading empty lines (CRLF) before the request-line MUST be
 * ignored. MUST return HWIRE_EAGAIN on incomplete input (no CRLF yet). MUST
 * reject: bare CR not followed by LF → HWIRE_EEOL.
 */
void test_parse_request_eol_errors(void)
{
    TEST_START("test_parse_request_eol_errors");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Empty request */
    buf = "";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, 0, &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Leading CRLF skipped */
    buf = "\r\nGET / HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* Null after version (incomplete) */
    buf = "GET / HTTP/1.1";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by null */
    buf = "GET / HTTP/1.1\r";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by non-LF */
    buf = "GET / HTTP/1.1\rX";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EEOL);

    TEST_END();
}

/*
 * Covers: error propagation from hwire_parse_headers() into
 * hwire_parse_request(). MUST: header parsing errors MUST propagate unchanged
 * from the headers section.
 */
void test_parse_request_header_errors(void)
{
    TEST_START("test_parse_request_header_errors");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* Header parse error propagation */
    buf = "GET / HTTP/1.1\r\n@Invalid: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    TEST_END();
}

/*
 * Covers: RFC 9112 §3.2  request-target forms:
 *   origin-form    = absolute-path [ "?" query ]  — standard requests
 *   absolute-form  = absolute-URI                  — proxy requests
 *   authority-form = uri-host ":" port             — CONNECT method
 *   asterisk-form  = "*"                           — OPTIONS method
 * MUST: all four request-target forms MUST be accepted.
 */
void test_parse_request_uri_forms(void)
{
    TEST_START("test_parse_request_uri_forms");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* absolute-form */
    buf = "GET http://example.org/pub/WWW/TheProject.html HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* authority-form */
    buf = "CONNECT www.example.com:80 HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* asterisk-form */
    buf = "OPTIONS * HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    TEST_END();
}

/*
 * Covers: hwire URI character validation (URI_CHAR[] lookup table).
 * RFC 3986 §2.2: reserved = gen-delims / sub-delims
 * RFC 3986 §2.3: unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~"
 * MUST reject: CTL characters (0x01-0x1F, 0x7F) → HWIRE_EURI
 * MUST reject: '{' (0x7B), '}' (0x7D), '|' (0x7C) — not in URI character set →
 * HWIRE_EURI Note: '~' (0x7E) IS a valid unreserved character (RFC 3986 §2.3)
 * and MUST be accepted.
 */
void test_parse_request_uri_invalid_chars(void)
{
    TEST_START("test_parse_request_uri_invalid_chars");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* RFC 3986: 0x01 is a CTL, not a URI character → MUST reject: HWIRE_EURI.
     */
    buf = "GET /p\x01 HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EURI);

    /* RFC 3986: '{' (0x7B), '}' (0x7D), '|' (0x7C) are not URI characters →
       MUST reject: HWIRE_EURI. */
    buf = "GET /path{json}|pipe HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EURI);

    /* RFC 3986: '|' (0x7C) is not a URI character → MUST reject: HWIRE_EURI. */
    buf = "GET /foo|bar HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EURI);

    /* Maxlen boundary check: URI "12345" (5 bytes) with maxlen=5 →
       HWIRE_EAGAIN (no SP yet) */
    buf = "GET 12345";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 5, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Long header value (SIMD coverage): 64 VCHAR characters */
    buf = "Long: "
          "1234567890123456789012345678901234567890123456789012345678901234\r\n"
          "\r\n";
    pos = 0;
    rv  = hwire_parse_headers(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    TEST_END();
}

/*
 * Covers: RFC 9112 §2.2  line termination
 *   A recipient MUST recognize a single LF as a line terminator (lenient).
 * MUST accept: "GET / HTTP/1.1\n\r\n" (bare LF after version) → HWIRE_OK.
 */
void test_parse_request_lf_eol(void)
{
    TEST_START("test_parse_request_lf_eol");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos      = 0;
    const char *buf = "GET / HTTP/1.1\n\r\n";
    int rv = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    TEST_END();
}

/*
 * Covers: RFC 3986 §2.3  unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~"
 *         RFC 9110 §9.1  method = token = 1*tchar
 * MUST accept: '~' (0x7E, unreserved per RFC 3986 §2.3) in request-target.
 * MUST reject: raw bytes > 0x7F (obs-text) in request-target → HWIRE_EURI
 *   (RFC 3986 requires percent-encoding for non-ASCII bytes).
 * MUST accept: non-alpha tchar characters ('!' '#' '$' etc.) as method chars,
 *   since method = token = 1*tchar (RFC 9110 §9.1).
 */
void test_parse_request_uri_chars(void)
{
    TEST_START("test_parse_request_uri_chars");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* RFC 3986 §2.3: '~' (0x7E) is an unreserved character — MUST accept */
    buf = "GET /path~file HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* '~' at the start of the path — MUST accept */
    buf = "GET /~user HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* RFC 3986: raw byte 0x80 (obs-text) is not a URI character; must be
     * percent-encoded → MUST reject with HWIRE_EURI */
    buf = "GET /\x80 HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EURI);

    /* RFC 9110 §9.1: method = token = 1*tchar; '!' is tchar → MUST accept */
    buf = "!GET / HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* '#' is tchar → MUST accept as part of method */
    buf = "#tag / HTTP/1.1\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_request(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    TEST_END();
}

int main(void)
{
    test_parse_request_valid();
    test_parse_request_cb_fail();
    test_parse_request_method_errors();
    test_parse_request_version_errors();
    test_parse_request_uri_errors();
    test_parse_request_eol_errors();
    test_parse_request_header_errors();
    test_parse_request_uri_forms();
    test_parse_request_uri_invalid_chars();
    test_parse_request_lf_eol();
    test_parse_request_uri_chars();
    print_test_summary();
    return g_tests_failed;
}
