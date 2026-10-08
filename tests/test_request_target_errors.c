#include "test_helpers.h"

static int parse_request(const char *buf, size_t len, size_t maxlen)
{
    hwire_ctx_t ctx = {
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos = 0;

    return hwire_parse_request(&ctx, buf, len, &pos, maxlen);
}

static void test_request_target_error_branches(void)
{
    TEST_START("test_request_target_error_branches");

    static const char *invalid[] = {
        /* IPv6 group-count boundaries around an embedded IPv4 address. */
        "CONNECT [1:2:3:4:5:6:7:192.0.2.1]:443 HTTP/1.1\r\n\r\n",
        "CONNECT [1:2:3:4:5:192.0.2.1]:443 HTTP/1.1\r\n\r\n",
        "CONNECT [1::2:3:4:5:6:192.0.2.1]:443 HTTP/1.1\r\n\r\n",

        /* IPvFuture requires a non-empty, valid value after the dot. */
        "CONNECT [v1.]:443 HTTP/1.1\r\n\r\n",
        "CONNECT [v1.a^]:443 HTTP/1.1\r\n\r\n",

        /* CONNECT authority-form requires a colon after a bracketed host. */
        "CONNECT [::1] HTTP/1.1\r\n\r\n",

        /* Invalid delimiters after an absolute URI IP-literal and its port. */
        "GET http://[::1]@/ HTTP/1.1\r\n\r\n",
        "GET http://[::1]:80@/ HTTP/1.1\r\n\r\n",

        /* A percent-encoded provisional port is userinfo only before '@'. */
        "GET http://host:12%GG/ HTTP/1.1\r\n\r\n",

        /* Exercise the invalid scheme-character path after its first byte. */
        "GET a0+-.^:path HTTP/1.1\r\n\r\n",
    };

    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        ASSERT_EQ(parse_request(invalid[i], strlen(invalid[i]), 1024),
                  HWIRE_EURI);
    }

    /* All RFC 3986 scheme continuation character classes remain accepted. */
    const char *valid = "GET a0+-.:path HTTP/1.1\r\n\r\n";
    ASSERT_OK(parse_request(valid, strlen(valid), 1024));

    /* A percent-encoded provisional port becomes valid userinfo at '@'. */
    valid = "GET http://user:12%20@example.com/ HTTP/1.1\r\n\r\n";
    ASSERT_OK(parse_request(valid, strlen(valid), 1024));

    TEST_END();
}

static void test_request_target_incomplete_branches(void)
{
    TEST_START("test_request_target_incomplete_branches");

    static const char *incomplete[] = {
        "CONNECT [:",       "CONNECT example.com",    "CONNECT [::1]",
        "GET http://",      "GET http://example.com", "GET http://host:12%",
        "GET http://[::1]", "GET http://[::1]:80",    "GET a0+-.",
    };

    for (size_t i = 0; i < sizeof(incomplete) / sizeof(incomplete[0]); i++) {
        size_t len = strlen(incomplete[i]);

        ASSERT_EQ(parse_request(incomplete[i], len, 1024), HWIRE_EAGAIN);
        ASSERT_EQ(parse_request(incomplete[i], len, len), HWIRE_ELEN);
    }

    TEST_END();
}

int main(void)
{
    test_request_target_error_branches();
    test_request_target_incomplete_branches();
    print_test_summary();
    return g_tests_failed;
}
