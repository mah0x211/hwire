#include "test_helpers.h"

void test_parse_response_valid(void) {
    TEST_START("test_parse_response_valid");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .response_cb = mock_response_cb,
        .header_cb = mock_header_cb
    };

    // "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n"
    strcpy(buf, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    int rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    // HTTP/1.0
    strcpy(buf, "HTTP/1.0 200 OK\r\n\r\n");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    // Missing reason phrase (just status)
    // "HTTP/1.1 200 \r\n" or "HTTP/1.1 200\r\n"
    strcpy(buf, "HTTP/1.1 200 \r\n\r\n");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    // EVERSION (missing SP after version)
    strcpy(buf, "HTTP/1.1"); // EAGAIN if len < 8 or if incomplete, but check parse_version ret.
    // parse_version needs 8 chars. if str is just "HTTP/1.1", it returns OK, pos=8.
    // then check ustr[cur].
    // if len=8, ustr[cur] is 0 or out of bounds?
    // "HTTP/1.1X"
    strcpy(buf, "HTTP/1.1X");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EVERSION);

    // EAGAIN (empty)
    pos = 0;
    rv = hwire_parse_response(buf, 0, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    // ESTATUS (invalid status code)
    strcpy(buf, "HTTP/1.1 999 OK\r\n\r\n"); // 9 is > '5'? Logic says 1-5 allowed for first digit.
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ESTATUS);

    TEST_END();
}

void test_parse_response_cb_fail(void) {
    TEST_START("test_parse_response_cb_fail");
    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .response_cb = mock_response_cb_fail,
        .header_cb = mock_header_cb
    };
    strcpy(buf, "HTTP/1.1 200 OK\r\n\r\n");
    int rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);
    TEST_END();
}

void test_parse_response_reason_phrase(void) {
    TEST_START("test_parse_response_reason_phrase");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .response_cb = mock_response_cb,
        .header_cb = mock_header_cb
    };

    // Line 1616-1620: OWS in reason phrase
    strcpy(buf, "HTTP/1.1 200 OK  Text\r\n\r\n");
    pos = 0;
    int rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    // OWS with HT in reason phrase
    strcpy(buf, "HTTP/1.1 200 OK\tText\r\n\r\n");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    // Line 1624: CR followed by null in reason
    strcpy(buf, "HTTP/1.1 200 OK\r");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    // Line 1627: Invalid CRLF in reason (CR not followed by LF)
    strcpy(buf, "HTTP/1.1 200 OK\rX");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEOL);

    // Line 1635: Invalid char in reason
    buf[0] = 'H'; buf[1] = 'T'; buf[2] = 'T'; buf[3] = 'P'; buf[4] = '/';
    buf[5] = '1'; buf[6] = '.'; buf[7] = '1'; buf[8] = ' '; buf[9] = '2';
    buf[10] = '0'; buf[11] = '0'; buf[12] = ' '; buf[13] = 0x01;
    buf[14] = '\r'; buf[15] = '\n'; buf[16] = '\r'; buf[17] = '\n'; buf[18] = 0;
    pos = 0;
    rv = hwire_parse_response(buf, 18, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    // Line 1640-1641: Reason too long (exceeds maxlen)
    strcpy(buf, "HTTP/1.1 200 OKThis is a very long reason phrase\r\n\r\n");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 20, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    // Line 1644: Incomplete reason phrase (no CRLF)
    strcpy(buf, "HTTP/1.1 200 OK");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

void test_parse_response_status_errors(void) {
    TEST_START("test_parse_response_status_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .response_cb = mock_response_cb,
        .header_cb = mock_header_cb
    };

    // Line 1664: Incomplete status code (len <= STATUS_LEN)
    strcpy(buf, "HTTP/1.1 200");
    pos = 0;
    int rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    // Line 1666: No space after status code
    strcpy(buf, "HTTP/1.1 200X OK\r\n\r\n");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ESTATUS);

    TEST_END();
}

void test_parse_response_edge_cases(void) {
    TEST_START("test_parse_response_edge_cases");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .response_cb = mock_response_cb,
        .header_cb = mock_header_cb
    };

    // Line 1705: Empty response
    buf[0] = '\0';
    pos = 0;
    int rv = hwire_parse_response(buf, 0, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    // Line 1711: Leading CRLF
    strcpy(buf, "\r\nHTTP/1.1 200 OK\r\n\r\n");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    // Line 1719: Null terminator after version
    strcpy(buf, "HTTP/1.1");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    // Line 1721: No space after version (covered in valid test as EVERSION)
    // Already tested

    // Line 1739: Reason phrase error propagation (EILSEQ)
    // Already covered in test_parse_response_reason_phrase

    // Line 1753: Header parse error propagation
    strcpy(buf, "HTTP/1.1 200 OK\r\n@Invalid: value\r\n\r\n");
    pos = 0;
    rv = hwire_parse_response(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    TEST_END();
}

int main(void) {
    test_parse_response_valid();
    test_parse_response_cb_fail();
    test_parse_response_reason_phrase();
    test_parse_response_status_errors();
    test_parse_response_edge_cases();
    print_test_summary();
    return g_tests_failed;
}
