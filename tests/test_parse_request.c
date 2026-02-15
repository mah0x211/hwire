#include "test_helpers.h"

void test_parse_request_valid(void) {
    TEST_START("test_parse_request_valid");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .request_cb = mock_request_cb,
        .header_cb = mock_header_cb
    };

    strcpy(buf, "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    TEST_END();
}

void test_parse_request_cb_fail(void) {
    TEST_START("test_parse_request_cb_fail");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .request_cb = mock_request_cb_fail,
        .header_cb = mock_header_cb
    };

    strcpy(buf, "GET / HTTP/1.1\r\n\r\n");
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

void test_parse_request_method_errors(void) {
    TEST_START("test_parse_request_method_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .request_cb = mock_request_cb,
        .header_cb = mock_header_cb
    };

    /* First char is not tchar */
    strcpy(buf, "@GET / HTTP/1.1\r\n\r\n");
    pos = 0;
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EMETHOD);

    /* Method without space (string ends) */
    strcpy(buf, "GET");
    pos = 0;
    rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Non-space char after method */
    strcpy(buf, "GET@/ HTTP/1.1\r\n\r\n");
    pos = 0;
    rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EMETHOD);

    TEST_END();
}

void test_parse_request_version_errors(void) {
    TEST_START("test_parse_request_version_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .request_cb = mock_request_cb,
        .header_cb = mock_header_cb
    };

    /* Unsupported HTTP version */
    strcpy(buf, "GET / HTTP/2.0\r\n\r\n");
    pos = 0;
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EVERSION);

    /* Non-CRLF char after version */
    strcpy(buf, "GET / HTTP/1.1X");
    pos = 0;
    rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EVERSION);

    TEST_END();
}

void test_parse_request_uri_errors(void) {
    TEST_START("test_parse_request_uri_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .request_cb = mock_request_cb,
        .header_cb = mock_header_cb
    };

    /* URI exceeds maxlen (no SP within maxlen) */
    strcpy(buf, "GET /verylongpathwithoutspaces HTTP/1.1\r\n\r\n");
    pos = 0;
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 10, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* URI with SP within maxlen */
    strcpy(buf, "GET /sp HTTP/1.1\r\n\r\n");
    pos = 0;
    rv = hwire_parse_request(buf, strlen(buf), &pos, 5, 10, &cb);
    ASSERT_OK(rv);

    /* No space after URI */
    strcpy(buf, "GET /path");
    pos = 0;
    rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

void test_parse_request_eol_errors(void) {
    TEST_START("test_parse_request_eol_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .request_cb = mock_request_cb,
        .header_cb = mock_header_cb
    };

    /* Empty request */
    buf[0] = '\0';
    pos = 0;
    int rv = hwire_parse_request(buf, 0, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Leading CRLF skipped */
    strcpy(buf, "\r\nGET / HTTP/1.1\r\n\r\n");
    pos = 0;
    rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    /* Null after version (incomplete) */
    strcpy(buf, "GET / HTTP/1.1");
    pos = 0;
    rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by null */
    strcpy(buf, "GET / HTTP/1.1\r");
    pos = 0;
    rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by non-LF */
    strcpy(buf, "GET / HTTP/1.1\rX");
    pos = 0;
    rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEOL);

    TEST_END();
}

void test_parse_request_header_errors(void) {
    TEST_START("test_parse_request_header_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .request_cb = mock_request_cb,
        .header_cb = mock_header_cb
    };

    /* Header parse error propagation */
    strcpy(buf, "GET / HTTP/1.1\r\n@Invalid: value\r\n\r\n");
    pos = 0;
    int rv = hwire_parse_request(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    TEST_END();
}

int main(void) {
    test_parse_request_valid();
    test_parse_request_cb_fail();
    test_parse_request_method_errors();
    test_parse_request_version_errors();
    test_parse_request_uri_errors();
    test_parse_request_eol_errors();
    test_parse_request_header_errors();
    print_test_summary();
    return g_tests_failed;
}
