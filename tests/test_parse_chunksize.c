#include "test_helpers.h"

/*
 * Covers: RFC 9112 §7.1   chunk-size = 1*HEXDIG
 *         RFC 9112 §7.1.1 chunk-ext  = *( BWS ";" BWS chunk-ext-name
 *                                          [ BWS "=" BWS chunk-ext-val ] )
 *                         chunk-ext-name = token = 1*tchar
 *                         chunk-ext-val  = token / quoted-string
 * MUST accept: valid hex chunk sizes (uppercase and lowercase) with optional
 * extensions. MUST return HWIRE_EAGAIN for incomplete input (no CRLF yet). MUST
 * reject: non-hex character → HWIRE_EILSEQ MUST reject: empty extension name →
 * HWIRE_EEXTNAME MUST reject: chunk-size exceeding uint32_t range →
 * HWIRE_ERANGE MUST return HWIRE_ENOBUFS if max extension count is exceeded.
 */
void test_parse_chunksize_valid(void)
{
    TEST_START("test_parse_chunksize_valid");

    hwire_callbacks_t cb = {.chunksize_cb     = mock_chunksize_cb,
                            .chunksize_ext_cb = mock_chunksize_ext_cb};
    size_t pos;
    int rv;
    const char *buf;

    /* RFC 9112 §7.1: HEXDIG includes A-F (uppercase) */
    buf = "1A\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 4);

    /* RFC 9112 §7.1: chunk-size "0" terminates the chunked body (last-chunk) */
    buf = "0\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 3);

    /* MUST return HWIRE_EAGAIN: empty input */
    buf = "";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, 0, &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* MUST reject: 'G' is not a valid HEXDIG → HWIRE_EILSEQ */
    buf = "G\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* MUST return HWIRE_EAGAIN: chunk-size digits present but no CRLF yet */
    buf = "1A";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* MUST return HWIRE_EAGAIN: chunk-size and CR received but LF not yet
       received */
    buf = "1A\r";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* RFC 9112 §7.1.1: empty extension name (";=val") → HWIRE_EEXTNAME */
    buf = "1A;=val\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEXTNAME);

    /* RFC 9112 §7.1.1: 0x01 is not tchar → invalid extension name →
       HWIRE_EILSEQ */
    buf = "1A;k\x01\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* MUST return HWIRE_EAGAIN: extension name started but input ends (no
       CRLF) */
    buf = "1A;key";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* MUST return HWIRE_ERANGE: chunk-size value overflows uint32_t */
    buf = "100000000\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ERANGE);

    /* MUST return HWIRE_ELEN: OWS in extension exceeds maxlen */
    buf = "1A;      \r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 5, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* RFC 9112 §7.1.1: extension with no value (name-only) MUST be accepted */
    buf = "1A; ext\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_OK(rv);

    /* RFC 9112 §7.1.1: extension with token value MUST be accepted */
    buf = "1A; ext=val\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_OK(rv);

    /* RFC 9112 §7.1.1: '@' is not tchar → invalid extension name →
       HWIRE_EEXTNAME */
    buf = "1A; @=val\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEXTNAME);

    /* RFC 9112 §7.1.1: extension with quoted-string value MUST be accepted */
    buf = "1A; ext=\"quoted\"\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_OK(rv);

    /* MUST return HWIRE_ENOBUFS if max extension count is exceeded */
    buf = "1A; e1; e2; x";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 1, &cb);
    ASSERT_EQ(rv, HWIRE_ENOBUFS);

    TEST_END();
}

/*
 * Covers: hwire callback return value handling for chunk-size.
 * MUST: non-zero return from chunksize_cb MUST cause hwire_parse_chunksize() to
 * return HWIRE_ECALLBACK.
 */
void test_parse_chunksize_callback_errors(void)
{
    TEST_START("test_parse_chunksize_callback_errors");

    hwire_callbacks_t cb = {.chunksize_cb     = mock_chunksize_cb_fail,
                            .chunksize_ext_cb = mock_chunksize_ext_cb};
    size_t pos           = 0;
    const char *buf;
    int rv;

    /* chunksize callback failure */
    buf = "1A\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

/*
 * Covers: RFC 9112 §2.2  line termination
 * MUST reject: CR not followed by LF → HWIRE_EEOL.
 */
void test_parse_chunksize_crlf_errors(void)
{
    TEST_START("test_parse_chunksize_crlf_errors");

    hwire_callbacks_t cb = {.chunksize_cb     = mock_chunksize_cb,
                            .chunksize_ext_cb = mock_chunksize_ext_cb};
    size_t pos           = 0;
    const char *buf;
    int rv;

    /* RFC 9112 §2.2: CR MUST be followed by LF; bare CR with non-LF →
       HWIRE_EEOL */
    buf = "1A\rX";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEOL);

    TEST_END();
}

/*
 * Covers: hwire callback return value handling for chunk extensions.
 * MUST: non-zero return from chunksize_ext_cb MUST cause
 * hwire_parse_chunksize() to return HWIRE_ECALLBACK.
 */
void test_parse_chunksize_ext_callback_errors(void)
{
    TEST_START("test_parse_chunksize_ext_callback_errors");

    hwire_callbacks_t cb = {.chunksize_cb     = mock_chunksize_cb,
                            .chunksize_ext_cb = mock_chunksize_ext_cb_fail};
    size_t pos;
    int rv;
    const char *buf;

    /* ext callback fails when CRLF is reached (extension name present, no
       value) */
    buf = "1A;ext\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    /* ext callback fails before parsing the next extension */
    buf = "1A;ext1;ext2\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

/*
 * Covers: RFC 9112 §7.1.1  chunk-ext-val = token / quoted-string
 *         RFC 9110 §5.6.4  quoted-string = DQUOTE *( qdtext / quoted-pair )
 * DQUOTE qdtext includes: HTAB / SP / %x21 / %x23-5B / %x5D-7E / obs-text MUST
 * reject: invalid character inside quoted-string extension value →
 * HWIRE_EEXTVAL.
 */
void test_parse_chunksize_ext_value_errors(void)
{
    TEST_START("test_parse_chunksize_ext_value_errors");

    hwire_callbacks_t cb = {.chunksize_cb     = mock_chunksize_cb,
                            .chunksize_ext_cb = mock_chunksize_ext_cb};
    size_t pos           = 0;
    const char *buf;
    int rv;

    /* RFC 9110 §5.6.4: 0x01 is not qdtext or valid quoted-pair target →
       HWIRE_EEXTVAL */
    buf = "1A;e=\"\x01\"\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(buf, strlen(buf), &pos, 100, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEXTVAL);

    TEST_END();
}

int main(void)
{
    test_parse_chunksize_valid();
    test_parse_chunksize_callback_errors();
    test_parse_chunksize_crlf_errors();
    test_parse_chunksize_ext_callback_errors();
    test_parse_chunksize_ext_value_errors();
    print_test_summary();
    return g_tests_failed;
}
