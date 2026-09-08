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

typedef int (*parse_message_fn)(const char *buf, size_t len, size_t maxlen,
                                size_t *pos);

static int parse_request_message(const char *buf, size_t len, size_t maxlen,
                                 size_t *pos)
{
    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t ctx = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };

    *pos = 0;
    return hwire_parse_request(&ctx, buf, len, pos, maxlen, 10);
}

static int parse_response_message(const char *buf, size_t len, size_t maxlen,
                                  size_t *pos)
{
    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t ctx = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .response_cb = mock_response_cb,
        .header_cb   = mock_header_cb
    };

    *pos = 0;
    return hwire_parse_response(&ctx, buf, len, pos, maxlen, 10);
}

/*
 * MUST: leading CRLF/LF bytes count toward the cumulative message maxlen.
 * The terminating empty header line remains excluded from that budget.
 */
void test_leading_empty_lines_count_toward_maxlen(void)
{
    TEST_START("test_leading_empty_lines_count_toward_maxlen");

    static const struct {
        parse_message_fn parse;
        const char *message;
        size_t prefix_len;
        size_t counted_len;
    } cases[] = {
        {parse_request_message,  "\r\n\nGET / HTTP/1.1\r\nHost: x\r\n\r\n", 3,
         28},
        {parse_response_message, "\r\n\nHTTP/1.1 200 OK\r\nA: x\r\n\r\n",   3,
         26},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t len = strlen(cases[i].message);
        size_t pos = 0;

        ASSERT_OK(
            cases[i].parse(cases[i].message, len, cases[i].counted_len, &pos));
        ASSERT_EQ(pos, len);

        ASSERT_EQ(cases[i].parse(cases[i].message, len,
                                 cases[i].counted_len - 1, &pos),
                  HWIRE_EHDRLEN);
        ASSERT_EQ(pos, 0);

        ASSERT_OK(cases[i].parse(
            cases[i].message + cases[i].prefix_len, len - cases[i].prefix_len,
            cases[i].counted_len - cases[i].prefix_len, &pos));
        ASSERT_EQ(pos, len - cases[i].prefix_len);
    }

    TEST_END();
}

/*
 * MUST: input ending at or before maxlen remains retryable. If a byte exists
 * beyond a prefix that has exhausted maxlen, the parser returns HWIRE_ELEN
 * without examining that byte.
 */
void test_leading_empty_lines_incomplete_boundaries(void)
{
    TEST_START("test_leading_empty_lines_incomplete_boundaries");

    static const struct {
        const char *buf;
        size_t maxlen;
        int expected;
    } cases[] = {
        {"",        0, HWIRE_EAGAIN},
        {"G",       0, HWIRE_ELEN  },
        {"\n",      1, HWIRE_EAGAIN},
        {"\nG",     1, HWIRE_ELEN  },
        {"\r",      1, HWIRE_EAGAIN},
        {"\r\n",    1, HWIRE_ELEN  },
        {"\r\n",    2, HWIRE_EAGAIN},
        {"\r\nG",   2, HWIRE_ELEN  },
        {"\r\n\n",  3, HWIRE_EAGAIN},
        {"\r\n\nG", 3, HWIRE_ELEN  },
    };
    static const parse_message_fn parsers[] = {
        parse_request_message,
        parse_response_message,
    };

    for (size_t p = 0; p < sizeof(parsers) / sizeof(parsers[0]); p++) {
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            size_t pos = 0;

            ASSERT_EQ(parsers[p](cases[i].buf, strlen(cases[i].buf),
                                 cases[i].maxlen, &pos),
                      cases[i].expected);
            ASSERT_EQ(pos, 0);
        }
    }

    TEST_END();
}

#if defined(HWIRE_MAP_ANONYMOUS)
static void assert_guarded_prefix(parse_message_fn parse, int final_byte)
{
    long page_size = sysconf(_SC_PAGESIZE);
    ASSERT(page_size > 0);

    size_t maxlen = (size_t)page_size;
    ASSERT(maxlen <= SIZE_MAX / 2);

    size_t map_len        = maxlen * 2;
    unsigned char *region = mmap(NULL, map_len, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | HWIRE_MAP_ANONYMOUS, -1, 0);
    ASSERT(region != MAP_FAILED);
    ASSERT_EQ(mprotect(region + maxlen, maxlen, PROT_NONE), 0);

    memset(region, '\n', maxlen);
    region[maxlen - 1] = (unsigned char)final_byte;

    size_t pos = 0;
    ASSERT_EQ(parse((const char *)region, maxlen + 1, maxlen, &pos),
              HWIRE_ELEN);
    ASSERT_EQ(pos, 0);

    ASSERT_EQ(mprotect(region + maxlen, maxlen, PROT_READ | PROT_WRITE), 0);
    ASSERT_EQ(munmap(region, map_len), 0);
}
#endif

/*
 * MUST: the leading-prefix loop and CRLF lookahead do not read the byte at
 * maxlen, even when len advertises that byte as available.
 */
void test_leading_empty_lines_guard_page(void)
{
    TEST_START("test_leading_empty_lines_guard_page");

#if defined(HWIRE_MAP_ANONYMOUS)
    static const parse_message_fn parsers[] = {
        parse_request_message,
        parse_response_message,
    };

    for (size_t i = 0; i < sizeof(parsers) / sizeof(parsers[0]); i++) {
        assert_guarded_prefix(parsers[i], '\n');
        assert_guarded_prefix(parsers[i], '\r');
    }
#else
    fprintf(stdout, "[SKIP] anonymous mmap is unavailable\n");
#endif

    TEST_END();
}

int main(void)
{
    test_leading_empty_lines_count_toward_maxlen();
    test_leading_empty_lines_incomplete_boundaries();
    test_leading_empty_lines_guard_page();
    print_test_summary();
    return g_tests_failed;
}
