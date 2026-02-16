#include "test_helpers.h"

void test_parse_headers_valid(void) {
    TEST_START("test_parse_headers_valid");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .header_cb = mock_header_cb
    };

    strcpy(buf, "Host: example.com\r\nConnection: close\r\n\r\n");
    int rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* Max headers exceeded */
    strcpy(buf, "H1: v1\r\nH2: v2\r\n\r\n");
    pos = 0;
    rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 1, &cb);
    ASSERT_EQ(rv, HWIRE_ENOBUFS);

    /* Max header length exceeded (key) */
    strcpy(buf, "VeryLongKey: value\r\n\r\n");
    pos = 0;
    rv = hwire_parse_headers(buf, strlen(buf), &pos, 5, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    /* Invalid header name */
    strcpy(buf, "@Invalid: value\r\n\r\n");
    pos = 0;
    rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* EEOL: CR not followed by LF in value */
    strcpy(buf, "Key: value\r\t\n\r\n");
    pos = 0;
    rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEOL);

    /* EHDRVALUE: Invalid char in value */
    buf[0] = 'K'; buf[1] = ':'; buf[2] = ' '; buf[3] = 0x01;
    buf[4] = '\r'; buf[5] = '\n'; buf[6] = 0;
    pos = 0;
    rv = hwire_parse_headers(buf, 6, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRVALUE);

    /* EKEYLEN: Key buffer too small */
    hwire_callbacks_t cb_small = cb;
    cb_small.key_lc.size = 2;
    strcpy(buf, "Key: val\r\n\r\n");
    pos = 0;
    rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb_small);
    ASSERT_EQ(rv, HWIRE_EKEYLEN);

    /* OWS in value */
    strcpy(buf, "Key:   value  \r\n\r\n");
    pos = 0;
    rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    /* Empty header value */
    strcpy(buf, "H1:\r\n\r\n");
    pos = 0;
    rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    TEST_END();
}

void test_parse_headers_fail(void) {
    TEST_START("test_parse_headers_fail");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .header_cb = mock_header_cb_fail
    };

    strcpy(buf, "Key: Value\r\n\r\n");
    int rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

void test_parse_headers_ows_handling(void) {
    TEST_START("test_parse_headers_ows_handling");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .header_cb = mock_header_cb
    };

    /* OWS followed by VCHAR */
    strcpy(buf, "Key: val  ue\r\n\r\n");
    pos = 0;
    int rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    TEST_END();
}

void test_parse_headers_cr_handling(void) {
    TEST_START("test_parse_headers_cr_handling");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .header_cb = mock_header_cb
    };

    /* CR followed by null terminator in value */
    buf[0] = 'K'; buf[1] = 'e'; buf[2] = 'y'; buf[3] = ':'; buf[4] = ' ';
    buf[5] = 'v'; buf[6] = 'a'; buf[7] = 'l'; buf[8] = '\r'; buf[9] = '\0';
    pos = 0;
    int rv = hwire_parse_headers(buf, 9, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

void test_parse_headers_invalid_values(void) {
    TEST_START("test_parse_headers_invalid_values");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .header_cb = mock_header_cb
    };

    /* Value exceeds maxlen */
    strcpy(buf, "Key: verylongvalue\r\n\r\n");
    pos = 0;
    int rv = hwire_parse_headers(buf, strlen(buf), &pos, 8, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    TEST_END();
}

void test_parse_headers_key_parsing(void) {
    TEST_START("test_parse_headers_key_parsing");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    hwire_callbacks_t cb_no_lc = {
        .key_lc = { .buf = NULL, .size = 0, .len = 0 },
        .header_cb = mock_header_cb
    };

    /* No key_lc buffer */
    strcpy(buf, "Key: value\r\n\r\n");
    pos = 0;
    int rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb_no_lc);
    ASSERT_OK(rv);

    /* Non-tchar char in key with no key_lc buffer */
    strcpy(buf, "Ke@y: value\r\n\r\n");
    pos = 0;
    rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb_no_lc);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .header_cb = mock_header_cb
    };

    /* Key without colon, needs more data */
    strcpy(buf, "KeyWithoutColon");
    pos = 0;
    rv = hwire_parse_headers(buf, strlen(buf), &pos, strlen(buf), 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

void test_parse_headers_empty_and_eol(void) {
    TEST_START("test_parse_headers_empty_and_eol");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .header_cb = mock_header_cb
    };

    /* Empty string */
    buf[0] = '\0';
    pos = 0;
    int rv = hwire_parse_headers(buf, 0, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by null terminator */
    buf[0] = '\r'; buf[1] = '\0';
    pos = 0;
    rv = hwire_parse_headers(buf, 1, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by non-LF */
    buf[0] = '\r'; buf[1] = 'X';
    pos = 0;
    rv = hwire_parse_headers(buf, 2, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    TEST_END();
}

void test_parse_headers_ows_maxlen(void) {
    TEST_START("test_parse_headers_ows_maxlen");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .header_cb = mock_header_cb
    };

    /* OWS skip exceeds maxlen */
    strcpy(buf, "K:     value\r\n\r\n");
    pos = 0;
    int rv = hwire_parse_headers(buf, strlen(buf), &pos, 4, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    TEST_END();
}

static int check_empty_value_cb(hwire_callbacks_t *cb, hwire_header_t *header) {
    (void)cb;
    if (header->value.len == 0) {
        return 0;
    }
    return 1; // Fail if not empty
}

void test_parse_headers_allows_empty_value(void) {
    TEST_START("test_parse_headers_allows_empty_value");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .header_cb = check_empty_value_cb
    };

    /* Empty header value */
    strcpy(buf, "Empty-Val:\r\n\r\n");
    int rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    /* OWS then empty */
    strcpy(buf, "Empty-Val:   \r\n\r\n");
    pos = 0;
    rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    TEST_END();
}

int main(void) {
    test_parse_headers_valid();
    test_parse_headers_fail();
    test_parse_headers_ows_handling();
    test_parse_headers_cr_handling();
    test_parse_headers_invalid_values();
    test_parse_headers_key_parsing();
    test_parse_headers_empty_and_eol();
    test_parse_headers_ows_maxlen();
    test_parse_headers_allows_empty_value();
    print_test_summary();
    return g_tests_failed;
}
