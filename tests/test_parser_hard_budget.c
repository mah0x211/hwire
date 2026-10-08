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

static int parse_request(const char *buf, size_t len, size_t maxlen)
{
    hwire_ctx_t ctx                 = {
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos = 0;

    return hwire_parse_request(&ctx, buf, len, &pos, maxlen);
}

static int parse_response(const char *buf, size_t len, size_t maxlen)
{
    hwire_ctx_t ctx                 = {
        .response_cb = mock_response_cb,
        .header_cb   = mock_header_cb
    };
    size_t pos = 0;

    return hwire_parse_response(&ctx, buf, len, &pos, maxlen);
}

static int parse_headers(const char *buf, size_t len, size_t maxlen)
{
    hwire_ctx_t ctx                 = {
        .header_cb = mock_header_cb
    };
    size_t pos = 0;

    return hwire_parse_headers(&ctx, buf, len, &pos, maxlen);
}

static int parse_parameters(const char *buf, size_t len, size_t maxlen)
{
    hwire_ctx_t ctx = {.param_cb = mock_param_cb};
    size_t pos      = 0;

    return hwire_parse_parameters(&ctx, buf, len, &pos, maxlen, 0);
}

void test_request_start_line_hard_budget(void)
{
    TEST_START("test_request_start_line_hard_budget");

    ASSERT_EQ(parse_request("", 0, 0), HWIRE_ELEN);

    /* Incomplete input remains retryable only before the budget is exhausted.
     */
    ASSERT_EQ(parse_request("GET", 3, 4), HWIRE_EAGAIN);
    ASSERT_EQ(parse_request("GET", 3, 3), HWIRE_ELEN);

    /* An invalid method or URI byte outside the budget must not be examined. */
    ASSERT_EQ(parse_request("GET@", 4, 3), HWIRE_ELEN);
    ASSERT_EQ(parse_request("GE@", 3, 3), HWIRE_EMETHOD);
    ASSERT_EQ(parse_request("GET /abc\x01", 9, 8), HWIRE_ELEN);
    ASSERT_EQ(parse_request("GET /ab\x01", 8, 8), HWIRE_EURI);

    /* Fixed-length version parsing and CRLF lookahead stay within the tail. */
    ASSERT_EQ(parse_request("GET / HTTP/1.X\r\n\r\n", 18, 13), HWIRE_ELEN);
    ASSERT_EQ(parse_request("GET / HTTP/1.X\r\n\r\n", 18, 14), HWIRE_EVERSION);
    ASSERT_EQ(parse_request("GET / HTTP/1.", 13, 14), HWIRE_EAGAIN);
    ASSERT_EQ(parse_request("GET / HTTP/1.1\rX", 16, 15), HWIRE_ELEN);
    ASSERT_EQ(parse_request("GET / HTTP/1.1\rX", 16, 16), HWIRE_EEOL);
    ASSERT_EQ(parse_request("GET / HTTP/1.1\r", 15, 16), HWIRE_EAGAIN);

    /* The terminating empty line counts toward the total request budget. */
    ASSERT_EQ(parse_request("GET / HTTP/1.1\r\n", 16, 18), HWIRE_EAGAIN);
    ASSERT_EQ(parse_request("GET / HTTP/1.1\r\n\r\n", 18, 16), HWIRE_EHDRLEN);
    ASSERT_EQ(parse_request("GET / HTTP/1.1\r\n\r\n", 18, 17), HWIRE_EHDRLEN);
    ASSERT_OK(parse_request("GET / HTTP/1.1\r\n\r\n", 18, 18));

    TEST_END();
}

void test_response_start_line_hard_budget(void)
{
    TEST_START("test_response_start_line_hard_budget");

    ASSERT_EQ(parse_response("", 0, 0), HWIRE_ELEN);

    /* Version, its delimiter, status, and status delimiter are budgeted. */
    ASSERT_EQ(parse_response("HTTP/1.X 200 OK\r\n\r\n", 19, 7), HWIRE_ELEN);
    ASSERT_EQ(parse_response("HTTP/1.X 200 OK\r\n\r\n", 19, 8), HWIRE_EVERSION);
    ASSERT_EQ(parse_response("HTTP/1.1X", 9, 8), HWIRE_ELEN);
    ASSERT_EQ(parse_response("HTTP/1.1X", 9, 9), HWIRE_EVERSION);
    ASSERT_EQ(parse_response("HTTP/1.1 20X ", 13, 11), HWIRE_ELEN);
    ASSERT_EQ(parse_response("HTTP/1.1 20X ", 13, 12), HWIRE_ELEN);
    ASSERT_EQ(parse_response("HTTP/1.1 20X ", 13, 13), HWIRE_ESTATUS);
    ASSERT_EQ(parse_response("HTTP/1.1 200X", 13, 12), HWIRE_ELEN);
    ASSERT_EQ(parse_response("HTTP/1.1 200X", 13, 13), HWIRE_ESTATUS);

    /* Reason-phrase scanning and CRLF lookahead stay within the tail. */
    ASSERT_EQ(parse_response("HTTP/1.1 200 OK\x01", 16, 15), HWIRE_ELEN);
    ASSERT_EQ(parse_response("HTTP/1.1 200 OK\x01", 16, 16), HWIRE_EILSEQ);
    ASSERT_EQ(parse_response("HTTP/1.1 200 OK", 15, 16), HWIRE_EAGAIN);
    ASSERT_EQ(parse_response("HTTP/1.1 200 OK\rX", 17, 16), HWIRE_ELEN);
    ASSERT_EQ(parse_response("HTTP/1.1 200 OK\rX", 17, 17), HWIRE_EEOL);
    ASSERT_EQ(parse_response("HTTP/1.1 200 OK\r", 16, 17), HWIRE_EAGAIN);

    /* The terminating empty line counts toward the total response budget. */
    ASSERT_EQ(parse_response("HTTP/1.1 200 OK\r\n", 17, 19), HWIRE_EAGAIN);
    ASSERT_EQ(parse_response("HTTP/1.1 200 OK\r\n\r\n", 19, 17), HWIRE_EHDRLEN);
    ASSERT_EQ(parse_response("HTTP/1.1 200 OK\r\n\r\n", 19, 18), HWIRE_EHDRLEN);
    ASSERT_OK(parse_response("HTTP/1.1 200 OK\r\n\r\n", 19, 19));

    TEST_END();
}

void test_header_field_hard_budget(void)
{
    TEST_START("test_header_field_hard_budget");

    ASSERT_EQ(parse_headers("", 0, 0), HWIRE_EAGAIN);
    ASSERT_EQ(parse_headers("\r\n", 2, 0), HWIRE_EHDRLEN);
    ASSERT_EQ(parse_headers("\r\n", 2, 1), HWIRE_EHDRLEN);
    ASSERT_OK(parse_headers("\r\n", 2, 2));
    ASSERT_OK(parse_headers("\n", 1, 1));

    /* Name and value scanners cannot use a byte immediately past maxlen. */
    ASSERT_EQ(parse_headers("Key@\r\n\r\n", 8, 3), HWIRE_EHDRLEN);
    ASSERT_EQ(parse_headers("Key@\r\n\r\n", 8, 4), HWIRE_EHDRNAME);
    ASSERT_EQ(parse_headers("K: a\x01\r\n\r\n", 9, 4), HWIRE_EHDRLEN);
    ASSERT_EQ(parse_headers("K: a\x01\r\n\r\n", 9, 5), HWIRE_EHDRVALUE);

    /* OWS and CRLF lookahead obey the same cumulative block tail. */
    ASSERT_EQ(parse_headers("K:  X\r\n\r\n", 9, 4), HWIRE_EHDRLEN);
    ASSERT_EQ(parse_headers("K: a\rX\r\n", 8, 5), HWIRE_EHDRLEN);
    ASSERT_EQ(parse_headers("K: a\rX\r\n", 8, 6), HWIRE_EEOL);
    ASSERT_EQ(parse_headers("K: a\r", 5, 6), HWIRE_EAGAIN);

    /* The field and terminating empty line share one block budget. */
    ASSERT_EQ(parse_headers("K: a\r\n\r\n", 8, 6), HWIRE_EHDRLEN);
    ASSERT_EQ(parse_headers("K: a\r\n\r\n", 8, 7), HWIRE_EHDRLEN);
    ASSERT_OK(parse_headers("K: a\r\n\r\n", 8, 8));

    TEST_END();
}

void test_parser_hard_budget_guard_page(void)
{
    TEST_START("test_parser_hard_budget_guard_page");

#if defined(HWIRE_MAP_ANONYMOUS)
    long page_size = sysconf(_SC_PAGESIZE);
    ASSERT(page_size > 0);

    size_t maxlen = (size_t)page_size;
    ASSERT(maxlen <= SIZE_MAX / 2);

    size_t map_len        = maxlen * 2;
    unsigned char *region = mmap(NULL, map_len, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | HWIRE_MAP_ANONYMOUS, -1, 0);
    ASSERT(region != MAP_FAILED);
    ASSERT_EQ(mprotect(region + maxlen, maxlen, PROT_NONE), 0);

    memset(region, 'a', maxlen);
    memcpy(region, "GET /", 5);
    ASSERT_EQ(parse_request((const char *)region, maxlen + 1, maxlen),
              HWIRE_ELEN);

    memset(region, 'a', maxlen);
    memcpy(region, "HTTP/1.1 200 ", 13);
    ASSERT_EQ(parse_response((const char *)region, maxlen + 1, maxlen),
              HWIRE_ELEN);

    memset(region, 'a', maxlen);
    memcpy(region, "K: ", 3);
    ASSERT_EQ(parse_headers((const char *)region, maxlen + 1, maxlen),
              HWIRE_EHDRLEN);

    memset(region, 'a', maxlen);
    region[0] = ';';
    ASSERT_EQ(parse_parameters((const char *)region, maxlen + 1, maxlen),
              HWIRE_ELEN);

    ASSERT_EQ(mprotect(region + maxlen, maxlen, PROT_READ | PROT_WRITE), 0);
    ASSERT_EQ(munmap(region, map_len), 0);
#else
    fprintf(stdout, "[SKIP] anonymous mmap is unavailable\n");
#endif

    TEST_END();
}

int main(void)
{
    test_request_start_line_hard_budget();
    test_response_start_line_hard_budget();
    test_header_field_hard_budget();
    test_parser_hard_budget_guard_page();
    print_test_summary();
    return g_tests_failed;
}
