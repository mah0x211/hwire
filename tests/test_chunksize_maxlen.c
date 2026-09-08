#define _GNU_SOURCE

#include "test_helpers.h"

#include <stdint.h>

#if defined(__APPLE__) || defined(__linux__)
# include <sys/mman.h>
# include <unistd.h>
#endif

#if defined(MAP_ANONYMOUS)
# define HWIRE_MAP_ANONYMOUS MAP_ANONYMOUS
#elif defined(MAP_ANON)
# define HWIRE_MAP_ANONYMOUS MAP_ANON
#endif

static int parse_chunksize(const char *buf, size_t len, size_t maxlen,
                           size_t *pos)
{
    hwire_ctx_t ctx = {.chunksize_cb     = mock_chunksize_cb,
                       .chunksize_ext_cb = mock_chunksize_ext_cb};

    *pos = 0;
    return hwire_parse_chunksize(&ctx, buf, len, pos, maxlen, 10);
}

/*
 * MUST: maxlen includes the complete line terminator. A complete line whose
 * wire length equals maxlen succeeds; an incomplete line returns EAGAIN when
 * input ends before maxlen, and ELEN when it consumes the available budget.
 */
void test_chunksize_maxlen_boundaries(void)
{
    TEST_START("test_chunksize_maxlen_boundaries");

    static const char *lines[] = {
        "0; x=abc\r\n",
        "0; x=\"a\\\"b\"\r\n",
    };
    static const char *incomplete_at_limit[] = {
        "000", "0; ", "0;x", "0;x=a", "0;x=\"a", "0;x=\"a\\", "0\r",
    };
    size_t pos = 0;

    ASSERT_EQ(parse_chunksize("", 0, 0, &pos), HWIRE_EAGAIN);
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        size_t line_len = strlen(lines[i]);

        ASSERT_EQ(parse_chunksize(lines[i], line_len, 0, &pos), HWIRE_ELEN);
        for (size_t len = 0; len < line_len; len++) {
            ASSERT_EQ(parse_chunksize(lines[i], len, line_len, &pos),
                      HWIRE_EAGAIN);
        }
        ASSERT_OK(parse_chunksize(lines[i], line_len, line_len, &pos));
        ASSERT_EQ(pos, line_len);

        for (size_t maxlen = 0; maxlen < line_len; maxlen++) {
            ASSERT_EQ(parse_chunksize(lines[i], line_len, maxlen, &pos),
                      HWIRE_ELEN);
        }
        ASSERT_OK(parse_chunksize(lines[i], line_len, line_len + 1, &pos));
        ASSERT_EQ(pos, line_len);
    }

    for (size_t i = 0;
         i < sizeof(incomplete_at_limit) / sizeof(incomplete_at_limit[0]);
         i++) {
        size_t len = strlen(incomplete_at_limit[i]);
        ASSERT_EQ(parse_chunksize(incomplete_at_limit[i], len, len, &pos),
                  HWIRE_ELEN);
    }

    /* Bytes after a complete line do not count against its maxlen budget. */
    ASSERT_OK(parse_chunksize("0\r\nbody", 7, 3, &pos));
    ASSERT_EQ(pos, 3);

    /* CR at the end of available input needs more data; CR at the end of an
     * exhausted budget cannot inspect the following LF. */
    ASSERT_EQ(parse_chunksize("0\r", 2, 3, &pos), HWIRE_EAGAIN);
    ASSERT_EQ(parse_chunksize("0\r", 2, 2, &pos), HWIRE_ELEN);
    ASSERT_EQ(parse_chunksize("0\r\n", 3, 2, &pos), HWIRE_ELEN);

    /* LF-only termination follows the same exact-boundary rule. */
    ASSERT_OK(parse_chunksize("0\n", 2, 2, &pos));
    ASSERT_EQ(pos, 2);
    ASSERT_EQ(parse_chunksize("0\n", 2, 1, &pos), HWIRE_ELEN);

    TEST_END();
}

/*
 * MUST: syntax errors inside the budget retain their specific error. Bytes
 * beyond maxlen are not examined and therefore cannot replace ELEN with a
 * syntax error.
 */
void test_chunksize_maxlen_error_precedence(void)
{
    TEST_START("test_chunksize_maxlen_error_precedence");

    size_t pos = 0;

    ASSERT_EQ(parse_chunksize("00G0", 4, 3, &pos), HWIRE_EILSEQ);
    ASSERT_EQ(parse_chunksize("000G", 4, 3, &pos), HWIRE_ELEN);
    ASSERT_EQ(parse_chunksize("0;x=@", 5, 5, &pos), HWIRE_EILSEQ);
    ASSERT_EQ(parse_chunksize("0;x=a@", 6, 5, &pos), HWIRE_ELEN);
    ASSERT_EQ(parse_chunksize("0;x=\"\x01\"", 7, 7, &pos), HWIRE_EEXTVAL);
    ASSERT_EQ(parse_chunksize("0;x=\"a\x01", 7, 6, &pos), HWIRE_ELEN);

    TEST_END();
}

#if defined(HWIRE_MAP_ANONYMOUS)
static void assert_guarded_budget(const unsigned char *pattern,
                                  size_t pattern_len, unsigned char fill,
                                  size_t maxlen)
{
    long page_size = sysconf(_SC_PAGESIZE);
    ASSERT(page_size > 0);
    ASSERT((size_t)page_size >= maxlen);
    ASSERT((size_t)page_size <= SIZE_MAX / 2);

    size_t map_len        = (size_t)page_size * 2;
    unsigned char *region = mmap(NULL, map_len, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | HWIRE_MAP_ANONYMOUS, -1, 0);
    ASSERT(region != MAP_FAILED);
    ASSERT_EQ(
        mprotect(region + (size_t)page_size, (size_t)page_size, PROT_NONE), 0);

    unsigned char *buf = region + (size_t)page_size - maxlen;
    memset(buf, fill, maxlen);
    memcpy(buf, pattern, pattern_len);

    size_t pos = 0;
    ASSERT_EQ(parse_chunksize((const char *)buf, maxlen + 1, maxlen, &pos),
              HWIRE_ELEN);

    ASSERT_EQ(mprotect(region + (size_t)page_size, (size_t)page_size,
                       PROT_READ | PROT_WRITE),
              0);
    ASSERT_EQ(munmap(region, map_len), 0);
}
#endif

/*
 * MUST: every scanner used by hwire_parse_chunksize stays in [0, maxlen),
 * even when len advertises more input. The byte immediately after the budget
 * is an inaccessible guard page, so an out-of-budget read terminates the test.
 */
void test_chunksize_maxlen_guard_page(void)
{
    TEST_START("test_chunksize_maxlen_guard_page");

#if defined(HWIRE_MAP_ANONYMOUS)
    static const size_t maxlen             = 64;
    static const unsigned char ext_name[]  = "0;";
    static const unsigned char ext_token[] = "0;x=";
    static const unsigned char ext_quote[] = "0;x=\"";

    /* chunk-size digits, BWS, extension name, token value, and qdtext */
    assert_guarded_budget((const unsigned char *)"", 0, '0', maxlen);
    assert_guarded_budget(ext_name, sizeof(ext_name) - 1, ' ', maxlen);
    assert_guarded_budget(ext_name, sizeof(ext_name) - 1, 'a', maxlen);
    assert_guarded_budget(ext_token, sizeof(ext_token) - 1, 'a', maxlen);
    assert_guarded_budget(ext_quote, sizeof(ext_quote) - 1, 'a', maxlen);

    /* quoted-pair and CRLF lookahead at the final allowed byte */
    {
        unsigned char pattern[64] = "0;x=\"";
        size_t prefix_len         = sizeof(ext_quote) - 1;
        memset(pattern + prefix_len, 'a', sizeof(pattern) - prefix_len);
        pattern[sizeof(pattern) - 1] = '\\';
        assert_guarded_budget(pattern, sizeof(pattern), 'a', maxlen);
    }
    {
        unsigned char pattern[64];
        memset(pattern, '0', sizeof(pattern));
        pattern[sizeof(pattern) - 1] = '\r';
        assert_guarded_budget(pattern, sizeof(pattern), '0', maxlen);
    }
#else
    fprintf(stdout, "[SKIP] anonymous mmap is unavailable\n");
#endif

    TEST_END();
}

int main(void)
{
    test_chunksize_maxlen_boundaries();
    test_chunksize_maxlen_error_precedence();
    test_chunksize_maxlen_guard_page();
    print_test_summary();
    return g_tests_failed;
}
