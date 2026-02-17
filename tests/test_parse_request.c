#include "test_helpers.h"

void test_parse_request_valid(void)
{
    TEST_START("test_parse_request_valid");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };

    strcpy(buf, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    TEST_END();
}

void test_parse_request_cb_fail(void)
{
    TEST_START("test_parse_request_cb_fail");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb_fail,
        .header_cb  = mock_header_cb
    };

    strcpy(buf, "GET / HTTP/1.1\r\n\r\n");
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

void test_parse_request_method_errors(void)
{
    TEST_START("test_parse_request_method_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };

    /* First char is not tchar */
    strcpy(buf, "@GET / HTTP/1.1\r\n\r\n");
    pos    = 0;
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EMETHOD);

    /* Method without space (string ends) */
    strcpy(buf, "GET");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Empty URI (GET  HTTP/1.1) */
    strcpy(buf, "GET  HTTP/1.1\r\n\r\n");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    /* Should fail with HWIRE_EURI (empty request-target is invalid) */
    ASSERT_EQ(rv, HWIRE_EURI);

    /* Non-space char after method */
    strcpy(buf, "GET@/ HTTP/1.1\r\n\r\n");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EMETHOD);

    TEST_END();
}

void test_parse_request_version_errors(void)
{
    TEST_START("test_parse_request_version_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };

    /* Unsupported HTTP version */
    strcpy(buf, "GET / HTTP/2.0\r\n\r\n");
    pos    = 0;
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EVERSION);

    /* Non-CRLF char after version */
    strcpy(buf, "GET / HTTP/1.1X");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EVERSION);

    TEST_END();
}

void test_parse_request_uri_errors(void)
{
    TEST_START("test_parse_request_uri_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };

    /* URI exceeds maxlen (no SP within maxlen) */
    strcpy(buf, "GET /verylongpathwithoutspaces HTTP/1.1\r\n\r\n");
    pos    = 0;
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 10, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* URI with SP within maxlen */
    strcpy(buf, "GET /sp HTTP/1.1\r\n\r\n");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 5, 10, &cb);
    ASSERT_OK(rv);

    /* No space after URI */
    strcpy(buf, "GET /path");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

void test_parse_request_eol_errors(void)
{
    TEST_START("test_parse_request_eol_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };

    /* Empty request */
    buf[0] = '\0';
    pos    = 0;
    int rv = hwire_parse_request(buf, 0, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Leading CRLF skipped */
    strcpy(buf, "\r\nGET / HTTP/1.1\r\n\r\n");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    /* Null after version (incomplete) */
    strcpy(buf, "GET / HTTP/1.1");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by null */
    strcpy(buf, "GET / HTTP/1.1\r");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by non-LF */
    strcpy(buf, "GET / HTTP/1.1\rX");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEOL);

    TEST_END();
}

void test_parse_request_header_errors(void)
{
    TEST_START("test_parse_request_header_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };

    /* Header parse error propagation */
    strcpy(buf, "GET / HTTP/1.1\r\n@Invalid: value\r\n\r\n");
    pos    = 0;
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    TEST_END();
}

void test_parse_request_uri_forms(void)
{
    TEST_START("test_parse_request_uri_forms");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };

    /* absolute-form */
    strcpy(buf,
           "GET http://example.org/pub/WWW/TheProject.html HTTP/1.1\r\n\r\n");
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    /* authority-form */
    strcpy(buf, "CONNECT www.example.com:80 HTTP/1.1\r\n\r\n");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    /* asterisk-form */
    strcpy(buf, "OPTIONS * HTTP/1.1\r\n\r\n");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    TEST_END();
}

void test_parse_request_uri_invalid_chars(void)
{
    TEST_START("test_parse_request_uri_invalid_chars");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = mock_request_cb,
        .header_cb  = mock_header_cb
    };

    /* CTL in URI */
    /* Strict parsing: RFC 3986 allowed chars only.
     * \x01 is NOT allowed. Expect HWIRE_EURI. */
    buf[0] = 'G';
    buf[1] = 'E';
    buf[2] = 'T';
    buf[3] = ' ';
    buf[4] = '/';
    buf[5] = 'p';
    buf[6] = 0x01;
    buf[7] = ' '; // Invalid char \x01
    strcpy(buf + 8, "HTTP/1.1\r\n\r\n");

    pos    = 0;
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EURI);

    /* Unsafe chars: { } | */
    strcpy(buf, "GET /path{json}|pipe HTTP/1.1\r\n\r\n");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EURI);

    /* Invalid chars parsing */
    strcpy(buf, "GET /foo|bar HTTP/1.1\r\n\r\n");
    pos = 0;
    rv  = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EURI);

    /* Maxlen boundary check (len == maxlen) */
    /* URI is "12345", maxlen=5. Should be EAGAIN (waiting for SP), not ELEN */
    strcpy(buf, "GET 12345");
    pos = 0;
    /* len=9 ("GET 12345"), parse consumes "GET ", remainder "12345" len=5.
       maxlen for URI is 5.
       parse_uri called with len=5, maxlen=5.
    */
    /* Note: hwire_parse_request maxlen argument is for URI?
       No, header maxlen?
       Let's check hwire_parse_request signature:
       int hwire_parse_request(char *str, size_t len, size_t *pos, size_t maxlen, ...
       The 'maxlen' param is passed to parse_uri.
    */
    rv = hwire_parse_request(buf, 9, &pos, 5, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Long header value (SIMD coverage) */
    /* 64 chars of VCHAR */
    strcpy(buf, "Long: 1234567890123456789012345678901234567890123456789012345678901234\r\n\r\n");
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
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
    print_test_summary();
    return g_tests_failed;
}
