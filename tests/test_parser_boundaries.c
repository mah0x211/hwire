#include "test_helpers.h"

/* Each non-leading status digit must independently reject bytes outside DIGIT. */
static void test_status_digit_rejection(void)
{
    TEST_START("test_status_digit_rejection");
    static const char *messages[] = {
        "HTTP/1.1 2/0 OK\r\n\r\n",
        "HTTP/1.1 2:0 OK\r\n\r\n",
        "HTTP/1.1 20/ OK\r\n\r\n",
        "HTTP/1.1 20: OK\r\n\r\n",
    };
    hwire_ctx_t ctx = {
        .response_cb = mock_response_cb,
        .header_cb = mock_header_cb,
    };
    for (size_t i = 0; i < sizeof(messages) / sizeof(messages[0]); i++) {
        size_t pos = 0;
        ASSERT_EQ(hwire_parse_response(&ctx, messages[i], strlen(messages[i]),
                                       &pos, 1024), HWIRE_ESTATUS);
    }
    TEST_END();
}

/* IP literals must accept each absolute-URI delimiter after both host and port. */
static void test_ip_literal_delimiters(void)
{
    TEST_START("test_ip_literal_delimiters");
    static const char *messages[] = {
        "GET http://[::1] HTTP/1.1\r\n\r\n",
        "GET http://[::1]?x=y HTTP/1.1\r\n\r\n",
        "GET http://[::1]/ HTTP/1.1\r\n\r\n",
        "GET http://[::1]:80 HTTP/1.1\r\n\r\n",
        "GET http://[::1]:80?x=y HTTP/1.1\r\n\r\n",
        "GET http://[::1]:80/ HTTP/1.1\r\n\r\n",
    };
    hwire_ctx_t ctx = {
        .request_cb = mock_request_cb,
        .header_cb = mock_header_cb,
    };
    for (size_t i = 0; i < sizeof(messages) / sizeof(messages[0]); i++) {
        size_t len = strlen(messages[i]);
        size_t pos = 0;
        ASSERT_OK(hwire_parse_request(&ctx, messages[i], len, &pos, 1024));
        ASSERT_EQ(pos, len);
    }
    TEST_END();
}

/* Compression must replace at least one group, and an empty method is invalid. */
static void test_invalid_request_boundaries(void)
{
    TEST_START("test_invalid_request_boundaries");
    static const struct {
        const char *message;
        int expected;
    } cases[] = {
        {" / HTTP/1.1\r\n\r\n", HWIRE_EMETHOD},
        {"CONNECT [1:2:3:4:5:6:7::8]:443 HTTP/1.1\r\n\r\n", HWIRE_EURI},
    };
    hwire_ctx_t ctx = {
        .request_cb = mock_request_cb,
        .header_cb = mock_header_cb,
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        size_t pos = 0;
        ASSERT_EQ(hwire_parse_request(&ctx, cases[i].message,
                                      strlen(cases[i].message), &pos, 1024),
                  cases[i].expected);
    }
    /* A complete version still requires an end-of-line within the budget. */
    const char *message = "GET / HTTP/1.1";
    size_t len = strlen(message);
    size_t pos = 0;
    ASSERT_EQ(hwire_parse_request(&ctx, message, len, &pos, len), HWIRE_ELEN);
    pos = 0;
    ASSERT_EQ(hwire_parse_request(&ctx, message, len, &pos, len + 1),
              HWIRE_EAGAIN);
    TEST_END();
}

/* quoted-pair explicitly permits escaped SP and HTAB, but rejects NUL. */
static void test_quoted_pair_whitespace(void)
{
    TEST_START("test_quoted_pair_whitespace");
    static const char *messages[] = {"\"\\ \"", "\"\\\t\""};
    for (size_t i = 0; i < sizeof(messages) / sizeof(messages[0]); i++) {
        size_t pos = 0;
        size_t len = strlen(messages[i]);
        ASSERT_OK(hwire_parse_quoted_string(messages[i], len, &pos, len));
        ASSERT_EQ(pos, len);
    }
    const char invalid[] = {'"', '\\', '\0', '"'};
    size_t pos = 0;
    ASSERT_EQ(hwire_parse_quoted_string(invalid, sizeof(invalid), &pos,
                                        sizeof(invalid)), HWIRE_EILSEQ);
    /* The cursor stays at the start of the invalid quoted-pair. */
    ASSERT_EQ(pos, 1);
    TEST_END();
}

int main(void)
{
    test_status_digit_rejection();
    test_ip_literal_delimiters();
    test_invalid_request_boundaries();
    test_quoted_pair_whitespace();
    print_test_summary();
    return g_tests_failed;
}
