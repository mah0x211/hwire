#include "test_helpers.h"

/*
 * Covers: RFC 9112 §7.1 chunk-size line termination and whitespace.
 *   chunk      = chunk-size [ chunk-ext ] CRLF chunk-data CRLF
 *   last-chunk = 1*("0") [ chunk-ext ] CRLF
 *   chunk-ext  = *( BWS ";" BWS chunk-ext-name
 *                   [ BWS "=" BWS chunk-ext-val ] )
 * MUST: the chunk-size line ends with CRLF; a bare LF is HWIRE_EEOL.
 * MUST: BWS appears only around ";" and "="; whitespace directly before the
 * line terminator is HWIRE_EILSEQ.
 */

typedef struct {
    int ext_calls;
} ext_counter_t;

static int count_chunksize_cb(hwire_ctx_t *ctx, uint32_t size)
{
    (void)ctx;
    (void)size;
    return 0;
}

static int count_ext_cb(hwire_ctx_t *ctx, hwire_chunksize_ext_t *ext)
{
    (void)ext;
    ((ext_counter_t *)ctx->uctx)->ext_calls++;
    return 0;
}

static int parse_line(const char *input, size_t *pos, ext_counter_t *counter)
{
    hwire_ctx_t ctx = {.uctx             = counter,
                       .chunksize_cb     = count_chunksize_cb,
                       .chunksize_ext_cb = count_ext_cb};

    *pos = 0;
    return hwire_parse_chunksize(&ctx, input, strlen(input), pos, 1024);
}

static void test_chunksize_bare_lf_rejected(void)
{
    TEST_START("test_chunksize_bare_lf_rejected");

    static const char *cases[] = {
        "2\n", "2;a\n", "2;a=b\n", "2;a=\"b\"\n", "0\n", "0;a\n",
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        ext_counter_t counter = {0};
        size_t pos            = 0;

        ASSERT_EQ(parse_line(cases[i], &pos, &counter), HWIRE_EEOL);
        ASSERT_EQ(pos, 0);
        /* the last extension is not delivered for a rejected line */
        ASSERT_EQ(counter.ext_calls, 0);
    }

    TEST_END();
}

static void test_chunksize_whitespace_before_eol_rejected(void)
{
    TEST_START("test_chunksize_whitespace_before_eol_rejected");

    static const char *cases[] = {
        "2 \r\n",         "2\t\r\n",     "0 \r\n", "2;a \r\n", "2;a=b \r\n",
        "2;a=\"b\" \r\n", "2;a=b\t\r\n", "2 \n",   "2;a \n",
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        ext_counter_t counter = {0};
        size_t pos            = 0;

        ASSERT_EQ(parse_line(cases[i], &pos, &counter), HWIRE_EILSEQ);
        ASSERT_EQ(pos, 0);
        ASSERT_EQ(counter.ext_calls, 0);
    }

    TEST_END();
}

static void test_chunksize_bws_around_delimiters_accepted(void)
{
    TEST_START("test_chunksize_bws_around_delimiters_accepted");

    static const struct {
        const char *input;
        int ext_calls;
    } cases[] = {
        {"2\r\n",         0},
        {"0\r\n",         0},
        {"2;a\r\n",       1},
        {"2 ;a\r\n",      1},
        {"2\t;a\r\n",     1},
        {"2; a\r\n",      1},
        {"2;a =b\r\n",    1},
        {"2;a= b\r\n",    1},
        {"2;a ;b\r\n",    2},
        {"2;a=b ;c\r\n",  2},
        {"2;a=\"b\"\r\n", 1},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        ext_counter_t counter = {0};
        size_t pos            = 0;

        ASSERT_OK(parse_line(cases[i].input, &pos, &counter));
        ASSERT_EQ(pos, strlen(cases[i].input));
        ASSERT_EQ(counter.ext_calls, cases[i].ext_calls);
    }

    TEST_END();
}

/*
 * MUST: an incomplete line keeps the EAGAIN/ELEN budget rules; whitespace
 * may still be followed by ";" and is not rejected until a terminator
 * arrives.
 */
static void test_chunksize_incomplete_lines(void)
{
    TEST_START("test_chunksize_incomplete_lines");

    hwire_ctx_t ctx            = {.chunksize_cb = count_chunksize_cb};
    static const char *cases[] = {"2 ", "2\r", "2;a ", "2;a=b "};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t len = strlen(cases[i]);
        size_t pos = 0;

        ASSERT_EQ(hwire_parse_chunksize(&ctx, cases[i], len, &pos, 1024),
                  HWIRE_EAGAIN);
        ASSERT_EQ(hwire_parse_chunksize(&ctx, cases[i], len, &pos, len),
                  HWIRE_ELEN);
        ASSERT_EQ(pos, 0);
    }

    TEST_END();
}

int main(void)
{
    test_chunksize_bare_lf_rejected();
    test_chunksize_whitespace_before_eol_rejected();
    test_chunksize_bws_around_delimiters_accepted();
    test_chunksize_incomplete_lines();
    print_test_summary();
    return g_tests_failed;
}
