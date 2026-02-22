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

    hwire_ctx_t cb = {.chunksize_cb     = mock_chunksize_cb,
                      .chunksize_ext_cb = mock_chunksize_ext_cb};
    size_t pos;
    int rv;
    const char *buf;

    /* RFC 9112 §7.1: HEXDIG includes A-F (uppercase) */
    buf = "1A\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 4);

    /* RFC 9112 §7.1: chunk-size "0" terminates the chunked body (last-chunk) */
    buf = "0\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 3);

    /* MUST return HWIRE_EAGAIN: empty input */
    buf = "";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, 0, &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* MUST reject: 'G' is not a valid HEXDIG → HWIRE_EILSEQ */
    buf = "G\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* MUST return HWIRE_EAGAIN: chunk-size digits present but no CRLF yet */
    buf = "1A";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* MUST return HWIRE_EAGAIN: chunk-size and CR received but LF not yet
       received */
    buf = "1A\r";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* RFC 9112 §7.1.1: empty extension name (";=val") → HWIRE_EEXTNAME */
    buf = "1A;=val\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_EEXTNAME);

    /* RFC 9112 §7.1.1: 0x01 is not tchar → invalid extension name →
       HWIRE_EILSEQ */
    buf = "1A;k\x01\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* MUST return HWIRE_EAGAIN: extension name started but input ends (no
       CRLF) */
    buf = "1A;key";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* MUST return HWIRE_ERANGE: chunk-size value overflows uint32_t */
    buf = "100000000\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_ERANGE);

    /* MUST return HWIRE_ELEN: OWS in extension exceeds maxlen */
    buf = "1A;      \r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 5, 10);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* RFC 9112 §7.1.1: extension with no value (name-only) MUST be accepted */
    buf = "1A; ext\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_OK(rv);

    /* RFC 9112 §7.1.1: extension with token value MUST be accepted */
    buf = "1A; ext=val\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_OK(rv);

    /* RFC 9112 §7.1.1: '@' is not tchar → invalid extension name →
       HWIRE_EEXTNAME */
    buf = "1A; @=val\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_EEXTNAME);

    /* RFC 9112 §7.1.1: extension with quoted-string value MUST be accepted */
    buf = "1A; ext=\"quoted\"\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_OK(rv);

    /* MUST return HWIRE_ENOBUFS if max extension count is exceeded */
    buf = "1A; e1; e2; x";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 1);
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

    hwire_ctx_t cb = {.chunksize_cb     = mock_chunksize_cb_fail,
                      .chunksize_ext_cb = mock_chunksize_ext_cb};
    size_t pos     = 0;
    const char *buf;
    int rv;

    /* chunksize callback failure */
    buf = "1A\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
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

    hwire_ctx_t cb = {.chunksize_cb     = mock_chunksize_cb,
                      .chunksize_ext_cb = mock_chunksize_ext_cb};
    size_t pos     = 0;
    const char *buf;
    int rv;

    /* RFC 9112 §2.2: CR MUST be followed by LF; bare CR with non-LF →
       HWIRE_EEOL */
    buf = "1A\rX";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
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

    hwire_ctx_t cb = {.chunksize_cb     = mock_chunksize_cb,
                      .chunksize_ext_cb = mock_chunksize_ext_cb_fail};
    size_t pos;
    int rv;
    const char *buf;

    /* ext callback fails when CRLF is reached (extension name present, no
       value) */
    buf = "1A;ext\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    /* ext callback fails before parsing the next extension */
    buf = "1A;ext1;ext2\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
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

    hwire_ctx_t cb = {.chunksize_cb     = mock_chunksize_cb,
                      .chunksize_ext_cb = mock_chunksize_ext_cb};
    size_t pos     = 0;
    const char *buf;
    int rv;

    /* RFC 9110 §5.6.4: 0x01 is not qdtext or valid quoted-pair target →
       HWIRE_EEXTVAL */
    buf = "1A;e=\"\x01\"\r\n";
    pos = 0;
    rv  = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
    ASSERT_EQ(rv, HWIRE_EEXTVAL);

    TEST_END();
}

typedef struct {
    uint32_t size;
    int called;
    int failed;
} chunk_size_expect_t;

static int verify_chunksize_cb(hwire_ctx_t *ctx, uint32_t size)
{
    chunk_size_expect_t *e = (chunk_size_expect_t *)ctx->uctx;
    e->called++;
    if (size != e->size) {
        fprintf(stderr, "chunksize: expected %u, got %u\n", (unsigned)e->size,
                (unsigned)size);
        e->failed = 1;
    }
    return 0;
}

typedef struct {
    uint32_t size;
    const char *ext_key;
    size_t ext_key_len;
    const char *ext_value;
    size_t ext_value_len;
    const char *buf;
    size_t buf_len;
    int size_called;
    int ext_called;
    int failed;
} chunk_ext_expect_t;

static int verify_chunksize_with_ext_cb(hwire_ctx_t *ctx, uint32_t size)
{
    chunk_ext_expect_t *e = (chunk_ext_expect_t *)ctx->uctx;
    e->size_called++;
    if (size != e->size) {
        fprintf(stderr, "chunksize: expected %u, got %u\n", (unsigned)e->size,
                (unsigned)size);
        e->failed = 1;
    }
    return 0;
}

static int verify_chunksize_ext_content_cb(hwire_ctx_t *ctx,
                                           hwire_chunksize_ext_t *ext)
{
    chunk_ext_expect_t *e = (chunk_ext_expect_t *)ctx->uctx;
    e->ext_called++;
    if (!str_in_buf(ext->key, e->buf, e->buf_len)) {
        fprintf(stderr, "ext key: ptr out of range\n");
        e->failed = 1;
    }
    if (!str_in_buf(ext->value, e->buf, e->buf_len)) {
        fprintf(stderr, "ext value: ptr out of range\n");
        e->failed = 1;
    }
    if (ext->key.len != e->ext_key_len ||
        strncmp(ext->key.ptr, e->ext_key, e->ext_key_len) != 0) {
        fprintf(stderr, "ext key: expected '%.*s', got '%.*s'\n",
                (int)e->ext_key_len, e->ext_key, (int)ext->key.len,
                ext->key.ptr);
        e->failed = 1;
    }
    if (ext->value.len != e->ext_value_len) {
        fprintf(stderr, "ext value len: expected %zu, got %zu\n",
                e->ext_value_len, ext->value.len);
        e->failed = 1;
    } else if (e->ext_value_len > 0 &&
               strncmp(ext->value.ptr, e->ext_value, e->ext_value_len) != 0) {
        fprintf(stderr, "ext value: expected '%.*s', got '%.*s'\n",
                (int)e->ext_value_len, e->ext_value, (int)ext->value.len,
                ext->value.ptr);
        e->failed = 1;
    }
    return 0;
}

/*
 * Covers: exact numeric value of parsed chunk-size and extension key/value
 * content. MUST: chunksize_cb size parameter MUST equal the decimal value of
 * the hex digits. MUST: ext.key.ptr/len and ext.value.ptr/len MUST reference
 * the original input bytes.
 */
void test_parse_chunksize_content_verification(void)
{
    TEST_START("test_parse_chunksize_content_verification");

    /* Case 1: "1a\r\n" → size=26 */
    {
        chunk_size_expect_t exp = {26, 0, 0};
        hwire_ctx_t cb          = {.uctx             = &exp,
                                   .chunksize_cb     = verify_chunksize_cb,
                                   .chunksize_ext_cb = mock_chunksize_ext_cb};
        size_t pos              = 0;
        const char *buf         = "1a\r\n";
        int rv = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
        ASSERT_OK(rv);
        ASSERT_EQ(exp.called, 1);
        ASSERT_EQ(exp.failed, 0);
    }

    /* Case 2: "FF\r\n" → size=255 */
    {
        chunk_size_expect_t exp = {255, 0, 0};
        hwire_ctx_t cb          = {.uctx             = &exp,
                                   .chunksize_cb     = verify_chunksize_cb,
                                   .chunksize_ext_cb = mock_chunksize_ext_cb};
        size_t pos              = 0;
        const char *buf         = "FF\r\n";
        int rv = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
        ASSERT_OK(rv);
        ASSERT_EQ(exp.called, 1);
        ASSERT_EQ(exp.failed, 0);
    }

    /* Case 3: "0\r\n" → size=0 */
    {
        chunk_size_expect_t exp = {0, 0, 0};
        hwire_ctx_t cb          = {.uctx             = &exp,
                                   .chunksize_cb     = verify_chunksize_cb,
                                   .chunksize_ext_cb = mock_chunksize_ext_cb};
        size_t pos              = 0;
        const char *buf         = "0\r\n";
        int rv = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
        ASSERT_OK(rv);
        ASSERT_EQ(exp.called, 1);
        ASSERT_EQ(exp.failed, 0);
    }

    /* Case 4: "1a;ext=val\r\n" → size=26, ext key="ext"(3), value="val"(3) */
    {
        chunk_ext_expect_t exp = {26, "ext", 3, "val", 3, NULL, 0, 0, 0, 0};
        hwire_ctx_t cb         = {.uctx             = &exp,
                                  .chunksize_cb     = verify_chunksize_with_ext_cb,
                                  .chunksize_ext_cb = verify_chunksize_ext_content_cb};
        size_t pos             = 0;
        const char *buf        = "1a;ext=val\r\n";
        exp.buf                = buf;
        exp.buf_len            = strlen(buf);
        int rv = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
        ASSERT_OK(rv);
        ASSERT_EQ(exp.size_called, 1);
        ASSERT_EQ(exp.ext_called, 1);
        ASSERT_EQ(exp.failed, 0);
    }

    /* Case 5: "1a;flag\r\n" → size=26, ext key="flag"(4), value.len=0 */
    {
        chunk_ext_expect_t exp = {26, "flag", 4, "", 0, NULL, 0, 0, 0, 0};
        hwire_ctx_t cb         = {.uctx             = &exp,
                                  .chunksize_cb     = verify_chunksize_with_ext_cb,
                                  .chunksize_ext_cb = verify_chunksize_ext_content_cb};
        size_t pos             = 0;
        const char *buf        = "1a;flag\r\n";
        exp.buf                = buf;
        exp.buf_len            = strlen(buf);
        int rv = hwire_parse_chunksize(&cb, buf, strlen(buf), &pos, 100, 10);
        ASSERT_OK(rv);
        ASSERT_EQ(exp.size_called, 1);
        ASSERT_EQ(exp.ext_called, 1);
        ASSERT_EQ(exp.failed, 0);
    }

    TEST_END();
}

int main(void)
{
    test_parse_chunksize_valid();
    test_parse_chunksize_callback_errors();
    test_parse_chunksize_crlf_errors();
    test_parse_chunksize_ext_callback_errors();
    test_parse_chunksize_ext_value_errors();
    test_parse_chunksize_content_verification();
    print_test_summary();
    return g_tests_failed;
}
