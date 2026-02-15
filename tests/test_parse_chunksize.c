#include "test_helpers.h"

void test_parse_chunksize_valid(void) {
    TEST_START("test_parse_chunksize_valid");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    hwire_callbacks_t cb = {
        .chunksize_cb = mock_chunksize_cb,
        .chunksize_ext_cb = mock_chunksize_ext_cb
    };

    /* "1A\r\n" */
    strcpy(buf, "1A\r\n");
    int rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 4);

    /* "0\r\n" */
    pos = 0;
    strcpy(buf, "0\r\n");
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 3);

    /* Empty input -> EAGAIN */
    pos = 0;
    buf[0] = '\0';
    rv = hwire_parse_chunksize(buf, 0, &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Invalid hex -> EILSEQ */
    strcpy(buf, "G\r\n");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* EAGAIN: Partial number (no CRLF) */
    strcpy(buf, "1A");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* EAGAIN: Number + CR but no LF */
    strcpy(buf, "1A\r");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Extension errors */
    /* Empty extension name ";=val" -> EEXTNAME */
    strcpy(buf, "1A;=val\r\n");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEXTNAME);

    /* Invalid char in extension name -> EILSEQ */
    buf[0] = '1'; buf[1] = 'A'; buf[2] = ';'; buf[3] = 'k'; buf[4] = 0x01;
    buf[5] = '\r'; buf[6] = '\n'; buf[7] = 0;
    pos = 0;
    rv = hwire_parse_chunksize(buf, 6, &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* EAGAIN in extension (incomplete) */
    strcpy(buf, "1A;key");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Overflow -> ERANGE */
    strcpy(buf, "100000000\r\n");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ERANGE);

    /* Extensions */
    /* "1A; ext\r\n" (empty value) */
    strcpy(buf, "1A; ext\r\n");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_OK(rv);

    /* "1A; ext=val\r\n" */
    strcpy(buf, "1A; ext=val\r\n");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_OK(rv);

    /* Extension error: invalid char in name */
    strcpy(buf, "1A; @=val\r\n");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEXTNAME);

    /* Quoted extension value */
    strcpy(buf, "1A; ext=\"quoted\"\r\n");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_OK(rv);

    /* Max exts exceeded */
    strcpy(buf, "1A; e1; e2; x");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 1, &cb);
    ASSERT_EQ(rv, HWIRE_ENOBUFS);

    TEST_END();
}

void test_parse_chunksize_callback_errors(void) {
    TEST_START("test_parse_chunksize_callback_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    hwire_callbacks_t cb = {
        .chunksize_cb = mock_chunksize_cb_fail,
        .chunksize_ext_cb = mock_chunksize_ext_cb
    };

    /* chunksize callback failure */
    strcpy(buf, "1A\r\n");
    pos = 0;
    int rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

void test_parse_chunksize_crlf_errors(void) {
    TEST_START("test_parse_chunksize_crlf_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    hwire_callbacks_t cb = {
        .chunksize_cb = mock_chunksize_cb,
        .chunksize_ext_cb = mock_chunksize_ext_cb
    };

    /* CR not followed by LF */
    strcpy(buf, "1A\rX");
    pos = 0;
    int rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEOL);

    TEST_END();
}

void test_parse_chunksize_ext_callback_errors(void) {
    TEST_START("test_parse_chunksize_ext_callback_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    hwire_callbacks_t cb = {
        .chunksize_cb = mock_chunksize_cb,
        .chunksize_ext_cb = mock_chunksize_ext_cb_fail
    };

    /* ext callback fails at CRLF (klen > 0) */
    strcpy(buf, "1A;ext\r\n");
    pos = 0;
    int rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    /* ext callback fails before next extension */
    strcpy(buf, "1A;ext1;ext2\r\n");
    pos = 0;
    rv = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

void test_parse_chunksize_ext_value_errors(void) {
    TEST_START("test_parse_chunksize_ext_value_errors");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    hwire_callbacks_t cb = {
        .chunksize_cb = mock_chunksize_cb,
        .chunksize_ext_cb = mock_chunksize_ext_cb
    };

    /* EEXTVAL from quoted-string EILSEQ */
    buf[0] = '1'; buf[1] = 'A'; buf[2] = ';'; buf[3] = 'e'; buf[4] = '=';
    buf[5] = '"'; buf[6] = 0x01; buf[7] = '"'; buf[8] = '\r'; buf[9] = '\n'; buf[10] = 0;
    pos = 0;
    int rv = hwire_parse_chunksize(buf, 10, &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEXTVAL);

    TEST_END();
}

int main(void) {
    test_parse_chunksize_valid();
    test_parse_chunksize_callback_errors();
    test_parse_chunksize_crlf_errors();
    test_parse_chunksize_ext_callback_errors();
    test_parse_chunksize_ext_value_errors();
    print_test_summary();
    return g_tests_failed;
}
