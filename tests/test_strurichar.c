/*
 * Exhaustive equivalence tests for the URI-character whitelist scan
 * (strurichar) exercised through hwire_parse_request.
 *
 * The whitelist (URI_CHAR in src/hwire.c) is:
 *   '!'  '$'-';'  '='  '?'-'Z'  '_'  'a'-'z'  '~'
 * The SIMD implementations (NEON / SSE2 / SSE4.2 PCMPESTRI) must return
 * exactly the same stop position as the scalar reference for every byte
 * value and every offset, including the 16-byte block boundaries.
 */

#include "test_helpers.h"

/* captured request URI from the last request callback */
static hwire_str_t captured_uri;

static int capture_request_cb(hwire_ctx_t *ctx, hwire_request_t *req)
{
    (void)ctx;
    captured_uri = req->uri;
    return 0;
}

/* the URI whitelist as documented (must match URI_CHAR in src/hwire.c) */
static int uri_allowed(unsigned char c)
{
    return c == '!' || (c >= '$' && c <= ';') || c == '=' ||
           (c >= '?' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z') ||
           c == '~';
}

static hwire_ctx_t make_ctx(char *key_storage)
{
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = TEST_KEY_SIZE, .len = 0},
        .request_cb = capture_request_cb,
        .header_cb  = mock_header_cb
    };
    return cb;
}

/*
 * For every byte value placed at several offsets inside the request-target
 * (covering the 16-byte SIMD block boundaries), the parse verdict must
 * match the whitelist: allowed bytes parse OK, disallowed bytes yield
 * HWIRE_EURI (a disallowed SP acts as the URI delimiter instead).
 */
void test_strurichar_charset_exhaustive(void)
{
    TEST_START("test_strurichar_charset_exhaustive");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = make_ctx(key_storage);
    static const size_t offsets[] = {0, 1, 2, 14, 15, 16, 17, 30, 31, 32, 33};

    for (size_t oi = 0; oi < sizeof(offsets) / sizeof(offsets[0]); oi++) {
        size_t off = offsets[oi];
        for (int b = 1; b <= 255; b++) {
            unsigned char buf[128];
            size_t len = 0;
            /* "GET /" + 'a'*off + byte + 'a'*4 + " HTTP/1.1\r\n\r\n" */
            memcpy(buf, "GET /", 5);
            len = 5;
            memset(buf + len, 'a', off);
            len += off;
            buf[len++] = (unsigned char)b;
            memset(buf + len, 'a', 4);
            len += 4;
            memcpy(buf + len, " HTTP/1.1\r\n\r\n", 13);
            len += 13;

            size_t pos = 0;
            int rv = hwire_parse_request(&cb, (const char *)buf, len, &pos,
                                         1024, 10);
            int exp;
            if (uri_allowed((unsigned char)b)) {
                exp = HWIRE_OK;
            } else if (b == ' ') {
                /* SP terminates the target; the tail "a... HTTP/1.1" is
                 * then parsed as the version and fails */
                exp = HWIRE_EVERSION;
            } else {
                exp = HWIRE_EURI;
            }
            if (rv != exp) {
                fprintf(stderr,
                        "FAILED: byte 0x%02x at offset %zu: expected %d, "
                        "got %d\n",
                        b, off, exp, rv);
                g_tests_failed++;
                TEST_END();
                return;
            }
            if (rv == HWIRE_OK &&
                (captured_uri.len != 1 + off + 1 + 4 ||
                 captured_uri.ptr[0] != '/')) {
                fprintf(stderr,
                        "FAILED: byte 0x%02x at offset %zu: uri len %zu != "
                        "%zu\n",
                        b, off, captured_uri.len, 6 + off);
                g_tests_failed++;
                TEST_END();
                return;
            }
        }
    }

    TEST_END();
}

/*
 * URI slice length sweep: for every prefix length 0..48 the parser must
 * stop exactly at the SP and report the exact request-target length
 * (covers the scalar tail, the first SIMD block, and block crossings).
 */
void test_strurichar_length_sweep(void)
{
    TEST_START("test_strurichar_length_sweep");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = make_ctx(key_storage);

    for (size_t k = 1; k <= 48; k++) {
        unsigned char buf[128];
        size_t len = 0;
        memcpy(buf, "GET /", 5);
        len = 5;
        memset(buf + len, 'a', k);
        len += k;
        memcpy(buf + len, " HTTP/1.1\r\n\r\n", 13);
        len += 13;

        size_t pos = 0;
        int rv = hwire_parse_request(&cb, (const char *)buf, len, &pos, 1024,
                                     10);
        ASSERT_OK(rv);
        ASSERT_EQ(captured_uri.len, 1 + k);
    }

    /* empty target is rejected */
    {
        size_t pos = 0;
        int rv = hwire_parse_request(&cb, "GET  HTTP/1.1\r\n\r\n", 18, &pos,
                                     1024, 10);
        ASSERT_EQ(rv, HWIRE_EURI);
    }

    TEST_END();
}

/*
 * Deterministic fuzz: random target bytes drawn from the full byte range;
 * the expected verdict and URI length are derived from the documented
 * whitelist (first disallowed byte decides: SP ends the target, anything
 * else is HWIRE_EURI).
 */
void test_strurichar_fuzz(void)
{
    TEST_START("test_strurichar_fuzz");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = make_ctx(key_storage);
    unsigned long rng = 0x9E3779B97F4A7C15UL;

    for (int iter = 0; iter < 20000; iter++) {
        unsigned char buf[160];
        size_t tlen = (rng = rng * 6364136223846793005UL + 1442695040888963407UL,
                       (rng >> 33) % 56);
        size_t len = 0;
        memcpy(buf, "GET /", 5);
        len = 5;
        for (size_t i = 0; i < tlen; i++) {
            rng = rng * 6364136223846793005UL + 1442695040888963407UL;
            buf[len++] = (unsigned char)((rng >> 33) & 0xFF);
        }
        memcpy(buf + len, " HTTP/1.1\r\n\r\n", 13);
        len += 13;

        /* expected: scan the random target for the first disallowed byte */
        size_t first_bad = tlen;
        for (size_t i = 0; i < tlen; i++) {
            if (!uri_allowed(buf[5 + i])) {
                first_bad = i;
                break;
            }
        }
        size_t pos = 0;
        int rv = hwire_parse_request(&cb, (const char *)buf, len, &pos, 1024,
                                     10);
        if (first_bad == tlen) {
            /* all bytes allowed: the trailing SP ends the target "/" + tlen */
            ASSERT_OK(rv);
            ASSERT_EQ(captured_uri.len, 1 + tlen);
        } else if (buf[5 + first_bad] == ' ') {
            /* target = "/" + allowed prefix, SP ends it; the version parse
             * then sees the random tail (or the literal " HTTP/1.1") and
             * fails */
            ASSERT_EQ(rv, HWIRE_EVERSION);
        } else {
            ASSERT_EQ(rv, HWIRE_EURI);
        }
    }

    TEST_END();
}

int main(void)
{
    test_strurichar_charset_exhaustive();
    test_strurichar_length_sweep();
    test_strurichar_fuzz();
    print_test_summary();
    return g_tests_failed;
}
