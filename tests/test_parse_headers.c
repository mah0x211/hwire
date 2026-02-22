#include "test_helpers.h"

/*
 * Covers: RFC 9112 §5.1  header-field = field-name ":" OWS field-value OWS CRLF
 *         RFC 9110 §5.1  field-name   = token = 1*tchar
 *         RFC 9110 §5.5  field-value  = *( field-content / obs-fold )
 *                        field-content = field-vchar [ 1*( SP / HTAB /
 * field-vchar ) field-vchar ] field-vchar  = VCHAR / obs-text OWS = *( SP /
 * HTAB )
 */
void test_parse_headers_valid(void)
{
    TEST_START("test_parse_headers_valid");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Basic valid headers */
    buf = "Host: example.com\r\nConnection: close\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* hwire: MUST return HWIRE_ENOBUFS if the max header count is exceeded. */
    buf = "H1: v1\r\nH2: v2\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 1, &cb);
    ASSERT_EQ(rv, HWIRE_ENOBUFS);

    /* hwire: maxlen limits total (key + value) length; MUST return
       HWIRE_EHDRLEN if exceeded. */
    buf = "VeryLongKey: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 5, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    /* RFC 9110 §5.1: field-name = token = 1*tchar; '@' (0x40) is not tchar
       (MUST reject) */
    buf = "@Invalid: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* RFC 9112 §2.2: CRLF = CR LF; bare CR not followed by LF MUST be
       rejected → HWIRE_EEOL */
    buf = "Key: value\r\t\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EEOL);

    /* RFC 9110 §5.5: field-vchar starts at 0x21; CTL 0x01 is not field-vchar
       (MUST reject) */
    buf = "K: \x01\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRVALUE);

    { /* hwire: MUST return HWIRE_EKEYLEN if the key_lc buffer is too small. */
        hwire_callbacks_t cb_small = cb;
        cb_small.key_lc.size       = 2;
        buf                        = "Key: val\r\n\r\n";
        pos                        = 0;
        rv = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb_small);
        ASSERT_EQ(rv, HWIRE_EKEYLEN);
    }

    /* RFC 9110 §5.5: HTAB is valid between field-vchar chars in field-content
       (MUST accept) */
    buf = "Key: val\tue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    /* RFC 9110 §5.5: field-value = *( field-content / obs-fold ); zero
       characters is valid (MUST accept) */
    buf = "H1:\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    TEST_END();
}

void test_parse_headers_fail(void)
{
    TEST_START("test_parse_headers_fail");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb_fail
    };
    size_t pos         = 0;
    const char *buf    = "Key: Value\r\n\r\n";
    int rv             = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5  OWS = *( SP / HTAB )
 * MUST: SP/HTAB embedded between field-vchar characters is part of
 * field-content (not stripped). OWS stripping only applies to leading/trailing
 * whitespace of the field-value.
 * MUST: trailing OWS (SP/HTAB before CRLF) MUST be stripped from field-value.
 */
void test_parse_headers_ows_handling(void)
{
    TEST_START("test_parse_headers_ows_handling");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* OWS followed by VCHAR */
    buf = "Key: val  ue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    /* RFC 9110 §5.5: trailing OWS (SP before CRLF) MUST be stripped from
       field-value */
    buf = "Key: value  \r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    TEST_END();
}

void test_parse_headers_cr_handling(void)
{
    TEST_START("test_parse_headers_cr_handling");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* CR followed by null terminator in value */
    buf = "Key: val\r";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

void test_parse_headers_invalid_values(void)
{
    TEST_START("test_parse_headers_invalid_values");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* Value exceeds maxlen */
    buf = "Key: verylongvalue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 8, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    TEST_END();
}

void test_parse_headers_key_parsing(void)
{
    TEST_START("test_parse_headers_key_parsing");

    hwire_callbacks_t cb_no_lc = {
        .key_lc    = {.buf = NULL, .size = 0, .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* No key_lc buffer */
    buf = "Key: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb_no_lc);
    ASSERT_OK(rv);

    /* Non-tchar char in key with no key_lc buffer */
    buf = "Ke@y: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb_no_lc);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };

    /* Key without colon, needs more data */
    buf = "KeyWithoutColon";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, strlen(buf), 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

void test_parse_headers_empty_and_eol(void)
{
    TEST_START("test_parse_headers_empty_and_eol");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Empty string */
    buf = "";
    pos = 0;
    rv  = hwire_parse_headers(buf, 0, &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR at end of input (incomplete) */
    buf = "\r";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* CR followed by non-LF */
    buf = "\rX";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    TEST_END();
}

void test_parse_headers_ows_maxlen(void)
{
    TEST_START("test_parse_headers_ows_maxlen");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos = 0;
    const char *buf;
    int rv;

    /* OWS skip exceeds maxlen */
    buf = "K:     value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 4, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);

    TEST_END();
}

static int check_empty_value_cb(hwire_callbacks_t *cb, hwire_header_t *header)
{
    (void)cb;
    if (header->value.len == 0) {
        return 0;
    }
    return 1; // Fail if not empty
}

/*
 * Covers: RFC 9110 §5.5  field-value = *( field-content / obs-fold )
 *         OWS = *( SP / HTAB )
 * MUST: empty field-value (zero content after OWS stripping) MUST be accepted.
 * MUST: field-value consisting entirely of OWS results in an empty value after
 * stripping.
 */
void test_parse_headers_allows_empty_value(void)
{
    TEST_START("test_parse_headers_allows_empty_value");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = check_empty_value_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Empty header value */
    buf = "Empty-Val:\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    /* OWS then empty */
    buf = "Empty-Val:   \r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    TEST_END();
}

void test_parse_headers_rfc_compliance(void)
{
    TEST_START("test_parse_headers_rfc_compliance");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* RFC 9112 §5.1: No whitespace is allowed between field-name and ":".
     * MUST reject: SP before ":" — the SP terminates the token before ":" is
     * seen → HWIRE_EHDRNAME. */
    buf = "Key : Value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* RFC 9112 §5.2: obs-fold is deprecated and MUST be rejected.
     *   obs-fold = OWS CRLF 1*( SP / HTAB )
     * A continuation line starting with SP/HTAB is parsed as a header whose
     * name begins with SP → not a valid tchar → HWIRE_EHDRNAME. */
    buf = "Key: Value\r\n Folded\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    /* hwire: bare LF as field-value line terminator (lenient; RFC 9112 §2.2
       SHOULD accept bare LF); bare LF also triggers end-of-headers on next
       iteration */
    buf = "Key: value\n\r\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    /* hwire: bare LF as end-of-headers marker (lenient; RFC 9112 §2.2 SHOULD
       accept bare LF in place of CRLF) */
    buf = "Key: value\r\n\n";
    pos = 0;
    rv  = hwire_parse_headers(buf, strlen(buf), &pos, 1024, 10, &cb);
    ASSERT_OK(rv);

    TEST_END();
}

int main(void)
{
    test_parse_headers_valid();
    test_parse_headers_fail();
    test_parse_headers_ows_handling();
    test_parse_headers_cr_handling();
    test_parse_headers_invalid_values();
    test_parse_headers_key_parsing();
    test_parse_headers_empty_and_eol();
    test_parse_headers_ows_maxlen();
    test_parse_headers_allows_empty_value();
    test_parse_headers_rfc_compliance();
    print_test_summary();
    return g_tests_failed;
}
