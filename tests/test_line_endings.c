#include "test_helpers.h"

/*
 * All parsed HTTP line boundaries accept exactly CR?LF: either CRLF or LF.
 * A CR at the end of available input is incomplete; a CR followed by any
 * other byte is invalid.
 */

void test_request_line_endings(void)
{
    TEST_START("test_request_line_endings");

    static const struct {
        const char *input;
        int expected;
    } cases[] = {
        {"GET / HTTP/1.1\r\n\r\n",   HWIRE_OK    },
        {"GET / HTTP/1.1\n\n",       HWIRE_OK    },
        {"\r\n\nGET / HTTP/1.1\n\n", HWIRE_OK    },
        {"\r",                       HWIRE_EAGAIN},
        {"\n\r",                     HWIRE_EAGAIN},
        {"\rGET / HTTP/1.1\r\n\r\n", HWIRE_EEOL  },
        {"GET / HTTP/1.1\rX",        HWIRE_EEOL  },
    };
    hwire_ctx_t ctx = {.header_cb  = mock_header_cb,
                       .request_cb = mock_request_cb};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t pos = 0;
        int rv = hwire_parse_request(&ctx, cases[i].input,
                                     strlen(cases[i].input), &pos, 1024, 10);
        ASSERT_EQ(rv, cases[i].expected);
        if (rv == HWIRE_OK) {
            ASSERT_EQ(pos, strlen(cases[i].input));
        }
    }

    TEST_END();
}

void test_response_line_endings(void)
{
    TEST_START("test_response_line_endings");

    static const struct {
        const char *input;
        int expected;
    } cases[] = {
        {"HTTP/1.1 200 OK\r\n\r\n",   HWIRE_OK    },
        {"HTTP/1.1 200 OK\n\n",       HWIRE_OK    },
        {"\n\r\nHTTP/1.1 200 OK\n\n", HWIRE_OK    },
        {"\r",                        HWIRE_EAGAIN},
        {"\rHTTP/1.1 200 OK\r\n\r\n", HWIRE_EEOL  },
        {"HTTP/1.1 200 OK\rX",        HWIRE_EEOL  },
    };
    hwire_ctx_t ctx = {.header_cb   = mock_header_cb,
                       .response_cb = mock_response_cb};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t pos = 0;
        int rv = hwire_parse_response(&ctx, cases[i].input,
                                      strlen(cases[i].input), &pos, 1024, 10);
        ASSERT_EQ(rv, cases[i].expected);
        if (rv == HWIRE_OK) {
            ASSERT_EQ(pos, strlen(cases[i].input));
        }
    }

    TEST_END();
}

void test_header_line_endings(void)
{
    TEST_START("test_header_line_endings");

    static const struct {
        const char *input;
        int expected;
    } cases[] = {
        {"Key: value\r\n\r\n", HWIRE_OK    },
        {"Key: value\n\n",     HWIRE_OK    },
        {"Key: value\r",       HWIRE_EAGAIN},
        {"Key: value\rX",      HWIRE_EEOL  },
        {"\r",                 HWIRE_EAGAIN},
        {"\rX",                HWIRE_EEOL  },
        {"\n",                 HWIRE_OK    },
    };
    hwire_ctx_t ctx = {.header_cb = mock_header_cb};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t pos = 0;
        int rv = hwire_parse_headers(&ctx, cases[i].input,
                                     strlen(cases[i].input), &pos, 1024, 10);
        ASSERT_EQ(rv, cases[i].expected);
        if (rv == HWIRE_OK) {
            ASSERT_EQ(pos, strlen(cases[i].input));
        }
    }

    TEST_END();
}

void test_chunksize_line_endings(void)
{
    TEST_START("test_chunksize_line_endings");

    static const struct {
        const char *input;
        int expected;
    } cases[] = {
        {"1\r\n", HWIRE_OK    },
        {"1\n",   HWIRE_OK    },
        {"1\r",   HWIRE_EAGAIN},
        {"1\rX",  HWIRE_EEOL  },
    };
    hwire_ctx_t ctx = {.chunksize_cb     = mock_chunksize_cb,
                       .chunksize_ext_cb = mock_chunksize_ext_cb};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t pos = 0;
        int rv = hwire_parse_chunksize(&ctx, cases[i].input,
                                       strlen(cases[i].input), &pos, 1024, 10);
        ASSERT_EQ(rv, cases[i].expected);
        if (rv == HWIRE_OK) {
            ASSERT_EQ(pos, strlen(cases[i].input));
        }
    }

    TEST_END();
}

int main(void)
{
    test_request_line_endings();
    test_response_line_endings();
    test_header_line_endings();
    test_chunksize_line_endings();
    print_test_summary();
    return g_tests_failed;
}
