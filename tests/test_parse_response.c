#include "test_helpers.h"

/*
 * Covers: RFC 9112 §4  status-line = HTTP-version SP status-code SP
 * [reason-phrase] CRLF status-code  = 3DIGIT reason-phrase = *( HTAB / SP /
 * VCHAR / obs-text ) hwire: restricts first digit of status-code to '1'-'5'
 * (informational through server error). RFC 9112 §4: reason-phrase is optional
 * — empty reason-phrase MUST be accepted. RFC 9112 §2.2: leading empty line
 * (CRLF) before status-line MUST be ignored.
 */
void test_parse_response_valid(void)
{
    TEST_START("test_parse_response_valid");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .response_cb = mock_response_cb,
        .header_cb   = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Standard response with status-code, reason-phrase, and a header */
    buf = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* MUST accept: HTTP/1.0 (RFC 9112 §2.3: hwire accepts 1.0 and 1.1) */
    buf = "HTTP/1.0 200 OK\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* RFC 9112 §4: reason-phrase is optional (zero characters after mandatory
       SP) */
    buf = "HTTP/1.1 200 \r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* RFC 9112 §4: SP is required after HTTP-version; non-SP character →
       HWIRE_EVERSION */
    buf = "HTTP/1.1X";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EVERSION);

    /* RFC 9112 §4: version string not matching HTTP/1.1 or HTTP/1.0 →
       HWIRE_EVERSION returned from parse_version */
    buf = "HTTP/2.0 200 OK\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EVERSION);

    /* EAGAIN (empty) */
    buf = "";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, 0, &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* hwire: first digit of status-code restricted to '1'-'5'; '9' →
       HWIRE_ESTATUS */
    buf = "HTTP/1.1 999 OK\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_ESTATUS);

    TEST_END();
}

/*
 * Covers: hwire callback return value handling.
 * MUST: non-zero return from response_cb MUST cause hwire_parse_response() to
 * return HWIRE_ECALLBACK.
 */
void test_parse_response_cb_fail(void)
{
    TEST_START("test_parse_response_cb_fail");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .response_cb = mock_response_cb_fail,
        .header_cb   = mock_header_cb
    };
    size_t pos      = 0;
    const char *buf = "HTTP/1.1 200 OK\r\n\r\n";
    int rv = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

/*
 * Covers: RFC 9112 §4  reason-phrase = *( HTAB / SP / VCHAR / obs-text )
 * MUST accept: SP and HTAB within reason-phrase.
 * MUST accept: obs-text (0x80-0xFF) within reason-phrase.
 * MUST return HWIRE_EAGAIN if CRLF has not yet been received.
 * MUST reject: CTL other than HTAB (e.g. 0x01) in reason-phrase → HWIRE_EILSEQ
 * MUST reject: bare CR not followed by LF → HWIRE_EEOL
 * MUST reject: reason-phrase exceeding maxlen parameter → HWIRE_ELEN
 * MUST reject: NUL byte (0x00) in reason-phrase → HWIRE_EILSEQ (not EAGAIN)
 */
void test_parse_response_reason_phrase(void)
{
    TEST_START("test_parse_response_reason_phrase");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .response_cb = mock_response_cb,
        .header_cb   = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* RFC 9112 §4: SP is valid in reason-phrase = *( HTAB / SP / VCHAR /
       obs-text ) */
    buf = "HTTP/1.1 200 OK  Text\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* RFC 9112 §4: HTAB is valid in reason-phrase = *( HTAB / SP / VCHAR /
       obs-text ) */
    buf = "HTTP/1.1 200 OK\tText\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* MUST return HWIRE_EAGAIN: CR received but LF not yet present
       (incomplete CRLF) */
    buf = "HTTP/1.1 200 OK\r";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* RFC 9112 §2.2: CR MUST be followed by LF; bare CR with non-LF →
       HWIRE_EEOL */
    buf = "HTTP/1.1 200 OK\rX";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EEOL);

    /* RFC 9112 §4: 0x01 is not HTAB, SP, VCHAR, or obs-text → HWIRE_EILSEQ */
    buf = "HTTP/1.1 200 \x01\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* hwire: reason-phrase length limited by maxlen parameter → HWIRE_ELEN if
       exceeded */
    buf = "HTTP/1.1 200 OKThis is a very long reason phrase\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 20, 10);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* MUST return HWIRE_EAGAIN: no CRLF yet received (incomplete status-line)
     */
    buf = "HTTP/1.1 200 OK";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* hwire: bare LF accepted as reason-phrase line terminator (lenient;
       RFC 9112 §2.2 SHOULD accept bare LF) */
    buf = "HTTP/1.1 200 OK\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* OOB fix: NUL byte in reason-phrase MUST return HWIRE_EILSEQ, not
       HWIRE_EAGAIN.  sizeof()-1 is used so the embedded \x00 is within the
       declared length (strlen would stop at the NUL).
       Without the fix parse_reason treated endc==0 as "need more data" and
       returned HWIRE_EAGAIN; with the fix it returns HWIRE_EILSEQ. */
    {
        const char nul_buf[] = "HTTP/1.1 200 OK \x00\r\n\r\n";
        pos                  = 0;
        rv = hwire_parse_response(&cb, nul_buf, sizeof(nul_buf) - 1, &pos, 1024,
                                  10);
        ASSERT_EQ(rv, HWIRE_EILSEQ);
    }

    TEST_END();
}

/*
 * Covers: RFC 9112 §4  status-code = 3DIGIT
 *                      status-line includes mandatory SP after status-code
 * MUST return HWIRE_EAGAIN if the 3-digit status-code has not been fully
 * received. MUST reject: non-SP character after status-code (missing delimiter)
 * → HWIRE_ESTATUS.
 */
void test_parse_response_status_errors(void)
{
    TEST_START("test_parse_response_status_errors");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .response_cb = mock_response_cb,
        .header_cb   = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* MUST return HWIRE_EAGAIN: status-code not fully received (incomplete
       input) */
    buf = "HTTP/1.1 200";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* RFC 9112 §4: SP is required after status-code; 'X' instead of SP →
       HWIRE_ESTATUS */
    buf = "HTTP/1.1 200X OK\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_ESTATUS);

    TEST_END();
}

/*
 * Covers: RFC 9112 §2.2  line termination and leading CRLF handling.
 *   RFC 9112 §2.2: leading empty lines before the status-line MUST be ignored.
 * MUST return HWIRE_EAGAIN for empty input, incomplete version string, or
 * incomplete status-line.
 * Header parsing errors MUST propagate unchanged from hwire_parse_headers().
 */
void test_parse_response_edge_cases(void)
{
    TEST_START("test_parse_response_edge_cases");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .response_cb = mock_response_cb,
        .header_cb   = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* MUST return HWIRE_EAGAIN: empty input, nothing to parse */
    buf = "";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, 0, &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* RFC 9112 §2.2: leading CRLF before status-line MUST be ignored */
    buf = "\r\nHTTP/1.1 200 OK\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* MUST return HWIRE_EAGAIN: version string complete but SP not yet
       received */
    buf = "HTTP/1.1";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* MUST return HWIRE_EAGAIN: version string incomplete (fewer than 8 bytes
       available) */
    buf = "HTTP/1.";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    // RFC 9112 §4: no SP after version → HWIRE_EVERSION (covered in
    // test_parse_response_valid)

    // RFC 9112 §4: EILSEQ in reason-phrase propagates (covered in
    // test_parse_response_reason_phrase)

    /* RFC 9110 §5.1: invalid header name propagates from
       hwire_parse_headers() → HWIRE_EHDRNAME */
    buf = "HTTP/1.1 200 OK\r\n@Invalid: value\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_EHDRNAME);

    TEST_END();
}

/*
 * Covers: RFC 9112 §4  reason-phrase = *( HTAB / SP / VCHAR / obs-text )
 *                      obs-text      = %x80-FF  (RFC 9110 §5.5)
 * MUST accept: obs-text bytes (0x80-0xFF) in reason-phrase.
 */
void test_parse_response_reason_obstext(void)
{
    TEST_START("test_parse_response_reason_obstext");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .response_cb = mock_response_cb,
        .header_cb   = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* obs-text bytes appended to a normal reason phrase */
    buf = "HTTP/1.1 200 OK \x80\xff\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* reason-phrase consisting entirely of obs-text */
    buf = "HTTP/1.1 200 \x80\xa5\xff\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    TEST_END();
}

/*
 * Covers: RFC 9112 §4  status-code = 3DIGIT
 * hwire restriction: first digit MUST be '1'-'5' (informational–server error).
 * MUST accept: 100 (lower boundary) and 599 (upper boundary).
 * MUST reject: 099 (first digit '0') and 600 (first digit '6') → HWIRE_ESTATUS.
 * MUST: pos MUST equal strlen(input) after HWIRE_OK (no extra bytes consumed).
 */
void test_parse_response_status_boundaries(void)
{
    TEST_START("test_parse_response_status_boundaries");

    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t cb = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .response_cb = mock_response_cb,
        .header_cb   = mock_header_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* 100 — lowest accepted status code (first digit '1') */
    buf = "HTTP/1.1 100 Continue\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* 599 — highest accepted status code (first digit '5') */
    buf = "HTTP/1.1 599 \r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);

    /* 600 — first digit '6', not in '1'-'5' → MUST reject */
    buf = "HTTP/1.1 600 \r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_ESTATUS);

    /* 099 — first digit '0', not in '1'-'5' → MUST reject */
    buf = "HTTP/1.1 099 \r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_EQ(rv, HWIRE_ESTATUS);

    /* pos exactness: multi-header response pos MUST equal full input length */
    buf = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nX-Hdr: val\r\n\r\n";
    pos = 0;
    rv  = hwire_parse_response(&cb, buf, strlen(buf), &pos, 1024, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    TEST_END();
}

int main(void)
{
    test_parse_response_valid();
    test_parse_response_cb_fail();
    test_parse_response_reason_phrase();
    test_parse_response_status_errors();
    test_parse_response_edge_cases();
    test_parse_response_reason_obstext();
    test_parse_response_status_boundaries();
    print_test_summary();
    return g_tests_failed;
}
