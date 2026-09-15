#include "test_helpers.h"

#include <stdint.h>

typedef struct {
    const char *keys[4];
    const char *values[4];
    uint32_t numbers[4];
    const char *buf;
    size_t buf_len;
    size_t count;
    size_t calls;
    int failed;
} cursor_expect_t;

static int slice_matches(hwire_str_t actual, const char *expected,
                         const char *buf, size_t buf_len)
{
    size_t expected_len = strlen(expected);

    return str_in_buf(actual, buf, buf_len) && actual.len == expected_len &&
           memcmp(actual.ptr, expected, expected_len) == 0;
}

static int capture_request_cb(hwire_ctx_t *ctx, hwire_request_t *req)
{
    cursor_expect_t *expect = (cursor_expect_t *)ctx->uctx;
    size_t idx              = expect->calls;

    if (idx >= expect->count ||
        !slice_matches(req->method, expect->keys[idx], expect->buf,
                       expect->buf_len) ||
        !slice_matches(req->uri, expect->values[idx], expect->buf,
                       expect->buf_len)) {
        expect->failed = 1;
    }
    expect->calls++;
    return 0;
}

static int capture_response_cb(hwire_ctx_t *ctx, hwire_response_t *rsp)
{
    cursor_expect_t *expect = (cursor_expect_t *)ctx->uctx;
    size_t idx              = expect->calls;

    if (idx >= expect->count || rsp->status != expect->numbers[idx] ||
        !slice_matches(rsp->reason, expect->values[idx], expect->buf,
                       expect->buf_len)) {
        expect->failed = 1;
    }
    expect->calls++;
    return 0;
}

static int capture_header_cb(hwire_ctx_t *ctx, hwire_header_t *header)
{
    cursor_expect_t *expect = (cursor_expect_t *)ctx->uctx;
    size_t idx              = expect->calls;

    if (idx >= expect->count ||
        !slice_matches(header->key, expect->keys[idx], expect->buf,
                       expect->buf_len) ||
        !slice_matches(header->value, expect->values[idx], expect->buf,
                       expect->buf_len)) {
        expect->failed = 1;
    }
    expect->calls++;
    return 0;
}

static int capture_parameter_cb(hwire_ctx_t *ctx, hwire_param_t *param)
{
    cursor_expect_t *expect = (cursor_expect_t *)ctx->uctx;
    size_t idx              = expect->calls;

    if (idx >= expect->count ||
        !slice_matches(param->key, expect->keys[idx], expect->buf,
                       expect->buf_len) ||
        !slice_matches(param->value, expect->values[idx], expect->buf,
                       expect->buf_len)) {
        expect->failed = 1;
    }
    expect->calls++;
    return 0;
}

static int capture_chunksize_cb(hwire_ctx_t *ctx, uint32_t size)
{
    cursor_expect_t *expect = (cursor_expect_t *)ctx->uctx;
    size_t idx              = expect->calls;

    if (idx >= expect->count || size != expect->numbers[idx]) {
        expect->failed = 1;
    }
    expect->calls++;
    return 0;
}

void test_quoted_string_cursor(void)
{
    TEST_START("test_quoted_string_cursor");

    const char *buf  = "\"one\"\"two\"";
    size_t first_len = strlen("\"one\"");
    size_t total_len = strlen(buf);
    size_t pos       = 0;
    int rv = hwire_parse_quoted_string(buf, total_len, &pos, first_len);

    ASSERT_OK(rv);
    ASSERT_EQ(pos, first_len);

    rv = hwire_parse_quoted_string(buf, total_len, &pos, first_len);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, total_len);

    pos = first_len;
    rv  = hwire_parse_quoted_string(buf, total_len - 1, &pos, first_len);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, total_len - 1);

    pos = first_len;
    rv  = hwire_parse_quoted_string(buf, total_len, &pos, first_len - 1);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, total_len - 1);

    TEST_END();
}

void test_parameter_cursor(void)
{
    TEST_START("test_parameter_cursor");

    const char *buf                 = "prefix;a=1,";
    size_t start                    = strlen("prefix");
    size_t end                      = strlen(buf) - 1;
    cursor_expect_t expect          = {.keys    = {"a"},
                                       .values  = {"1"},
                                       .buf     = buf,
                                       .buf_len = strlen(buf),
                                       .count   = 1,
                                       .calls   = 0,
                                       .failed  = 0};
    char key_storage[TEST_KEY_SIZE] = {0};
    hwire_ctx_t ctx                 = {
        .uctx     = &expect,
        .key_lc   = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .param_cb = capture_parameter_cb
    };
    size_t pos = start;
    int rv     = hwire_parse_parameters(&ctx, buf, strlen(buf), &pos,
                                        strlen(buf) - start, 1, 0);

    ASSERT_OK(rv);
    ASSERT_EQ(pos, end);
    ASSERT_EQ(expect.calls, 1);
    ASSERT_EQ(expect.failed, 0);

    ctx.param_cb = mock_param_cb;
    pos          = start;
    rv = hwire_parse_parameters(&ctx, buf, strlen(buf), &pos, 0, 1, 0);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, start);

    TEST_END();
}

void test_chunksize_cursor(void)
{
    TEST_START("test_chunksize_cursor");

    const char *buf        = "A\r\n1;foo=bar\r\n";
    size_t first_len       = strlen("A\r\n");
    size_t second_len      = strlen("1;foo=bar\r\n");
    size_t total_len       = strlen(buf);
    cursor_expect_t expect = {
        .numbers = {10, 1},
        .buf     = buf,
        .buf_len = total_len,
        .count   = 2,
        .calls   = 0,
        .failed  = 0
    };
    hwire_ctx_t ctx = {.uctx             = &expect,
                       .chunksize_cb     = capture_chunksize_cb,
                       .chunksize_ext_cb = mock_chunksize_ext_cb};
    size_t pos      = 0;
    int rv = hwire_parse_chunksize(&ctx, buf, total_len, &pos, SIZE_MAX, 1);

    ASSERT_OK(rv);
    ASSERT_EQ(pos, first_len);

    rv = hwire_parse_chunksize(&ctx, buf, total_len, &pos, SIZE_MAX, 1);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, total_len);
    ASSERT_EQ(expect.calls, 2);
    ASSERT_EQ(expect.failed, 0);

    ctx.chunksize_cb = mock_chunksize_cb;
    pos              = first_len;
    rv               = hwire_parse_chunksize(&ctx, buf, total_len, &pos, 0, 1);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, first_len);

    pos = first_len;
    rv  = hwire_parse_chunksize(&ctx, buf, total_len, &pos, second_len - 1, 1);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, first_len);

    pos = first_len;
    rv  = hwire_parse_chunksize(&ctx, buf, total_len - 1, &pos, second_len, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, first_len);

    pos = total_len;
    rv  = hwire_parse_chunksize(&ctx, buf, total_len, &pos, 0, 1);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, total_len);

    pos = total_len;
    rv  = hwire_parse_chunksize(&ctx, buf, total_len, &pos, 1, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, total_len);

    pos = total_len + 1;
    rv  = hwire_parse_chunksize(&ctx, buf, total_len, &pos, SIZE_MAX, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, total_len + 1);

    TEST_END();
}

void test_header_cursor(void)
{
    TEST_START("test_header_cursor");

    const char *buf        = "A: one\r\n\r\nB: two\r\n\r\n";
    size_t first_len       = strlen("A: one\r\n\r\n");
    size_t second_len      = strlen("B: two\r\n\r\n");
    size_t total_len       = strlen(buf);
    cursor_expect_t expect = {
        .keys    = {"A",   "B"  },
        .values  = {"one", "two"},
        .buf     = buf,
        .buf_len = total_len,
        .count   = 2,
        .calls   = 0,
        .failed  = 0
    };
    char key_storage[TEST_KEY_SIZE] = {0};
    hwire_ctx_t ctx                 = {
        .uctx   = &expect,
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .header_cb = capture_header_cb
    };
    size_t pos = 0;
    int rv     = hwire_parse_headers(&ctx, buf, total_len, &pos, SIZE_MAX, 1);

    ASSERT_OK(rv);
    ASSERT_EQ(pos, first_len);

    rv = hwire_parse_headers(&ctx, buf, total_len, &pos, SIZE_MAX, 1);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, total_len);
    ASSERT_EQ(expect.calls, 2);
    ASSERT_EQ(expect.failed, 0);

    ctx.header_cb = mock_header_cb;
    pos           = first_len;
    rv            = hwire_parse_headers(&ctx, buf, total_len, &pos, 0, 1);
    ASSERT(rv != HWIRE_OK);
    ASSERT_EQ(pos, first_len);

    pos = first_len;
    rv  = hwire_parse_headers(&ctx, buf, total_len, &pos, second_len, 1);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, total_len);

    pos = first_len;
    rv  = hwire_parse_headers(&ctx, buf, total_len, &pos, second_len - 1, 1);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);
    ASSERT_EQ(pos, first_len);

    pos = first_len;
    rv = hwire_parse_headers(&ctx, buf, total_len - 1, &pos, second_len, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, first_len);

    pos = total_len + 1;
    rv  = hwire_parse_headers(&ctx, buf, total_len, &pos, SIZE_MAX, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, total_len + 1);

    {
        const char *multi = "A: 1\r\nB: 2\r\n\r\n";
        size_t multi_len  = strlen(multi);

        pos = 0;
        rv = hwire_parse_headers(&ctx, multi, multi_len, &pos, multi_len, 2);
        ASSERT_OK(rv);
        ASSERT_EQ(pos, multi_len);

        pos = 0;
        rv  = hwire_parse_headers(&ctx, multi, multi_len, &pos, multi_len - 1,
                                  2);
        ASSERT_EQ(rv, HWIRE_EHDRLEN);
        ASSERT_EQ(pos, 0);

        pos = 0;
        rv  = hwire_parse_headers(&ctx, multi, multi_len, &pos,
                                  strlen("A: 1\r\n"), 2);
        ASSERT_EQ(rv, HWIRE_EHDRLEN);
        ASSERT_EQ(pos, 0);
    }

    TEST_END();
}

void test_request_cursor(void)
{
    TEST_START("test_request_cursor");

    const char *buf   = "GET /one HTTP/1.1\r\n\r\nPOST /two HTTP/1.0\r\n\r\n";
    size_t first_len  = strlen("GET /one HTTP/1.1\r\n\r\n");
    size_t second_len = strlen("POST /two HTTP/1.0\r\n\r\n");
    size_t total_len  = strlen(buf);
    cursor_expect_t expect = {
        .keys    = {"GET",  "POST"},
        .values  = {"/one", "/two"},
        .buf     = buf,
        .buf_len = total_len,
        .count   = 2,
        .calls   = 0,
        .failed  = 0
    };
    char key_storage[TEST_KEY_SIZE] = {0};
    hwire_ctx_t ctx                 = {
        .uctx   = &expect,
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = capture_request_cb,
        .header_cb  = mock_header_cb
    };
    size_t pos = 0;
    int rv     = hwire_parse_request(&ctx, buf, total_len, &pos, SIZE_MAX, 1);

    ASSERT_OK(rv);
    ASSERT_EQ(pos, first_len);

    rv = hwire_parse_request(&ctx, buf, total_len, &pos, SIZE_MAX, 1);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, total_len);
    ASSERT_EQ(expect.calls, 2);
    ASSERT_EQ(expect.failed, 0);

    ctx.request_cb = mock_request_cb;
    pos            = first_len;
    rv             = hwire_parse_request(&ctx, buf, total_len, &pos, 0, 1);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, first_len);

    pos = first_len;
    rv  = hwire_parse_request(&ctx, buf, total_len, &pos, second_len, 1);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, total_len);

    pos = first_len;
    rv  = hwire_parse_request(&ctx, buf, total_len, &pos, second_len - 1, 1);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);
    ASSERT_EQ(pos, first_len);

    pos = first_len;
    rv  = hwire_parse_request(&ctx, buf, total_len - 1, &pos, second_len, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, first_len);

    pos = total_len;
    rv  = hwire_parse_request(&ctx, buf, total_len, &pos, 0, 1);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, total_len);

    pos = total_len;
    rv  = hwire_parse_request(&ctx, buf, total_len, &pos, 1, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, total_len);

    pos = total_len + 1;
    rv  = hwire_parse_request(&ctx, buf, total_len, &pos, SIZE_MAX, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, total_len + 1);

    TEST_END();
}

void test_response_cursor(void)
{
    TEST_START("test_response_cursor");

    const char *buf   = "HTTP/1.1 200 OK\r\n\r\nHTTP/1.0 404 Missing\r\n\r\n";
    size_t first_len  = strlen("HTTP/1.1 200 OK\r\n\r\n");
    size_t second_len = strlen("HTTP/1.0 404 Missing\r\n\r\n");
    size_t total_len  = strlen(buf);
    cursor_expect_t expect = {
        .values  = {"OK", "Missing"},
        .numbers = {200,  404      },
        .buf     = buf,
        .buf_len = total_len,
        .count   = 2,
        .calls   = 0,
        .failed  = 0
    };
    char key_storage[TEST_KEY_SIZE] = {0};
    hwire_ctx_t ctx                 = {
        .uctx   = &expect,
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .response_cb = capture_response_cb,
        .header_cb   = mock_header_cb
    };
    size_t pos = 0;
    int rv     = hwire_parse_response(&ctx, buf, total_len, &pos, SIZE_MAX, 1);

    ASSERT_OK(rv);
    ASSERT_EQ(pos, first_len);

    rv = hwire_parse_response(&ctx, buf, total_len, &pos, SIZE_MAX, 1);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, total_len);
    ASSERT_EQ(expect.calls, 2);
    ASSERT_EQ(expect.failed, 0);

    ctx.response_cb = mock_response_cb;
    pos             = first_len;
    rv              = hwire_parse_response(&ctx, buf, total_len, &pos, 0, 1);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, first_len);

    pos = first_len;
    rv  = hwire_parse_response(&ctx, buf, total_len, &pos, second_len, 1);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, total_len);

    pos = first_len;
    rv  = hwire_parse_response(&ctx, buf, total_len, &pos, second_len - 1, 1);
    ASSERT_EQ(rv, HWIRE_EHDRLEN);
    ASSERT_EQ(pos, first_len);

    pos = first_len;
    rv  = hwire_parse_response(&ctx, buf, total_len - 1, &pos, second_len, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, first_len);

    pos = total_len;
    rv  = hwire_parse_response(&ctx, buf, total_len, &pos, 0, 1);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, total_len);

    pos = total_len;
    rv  = hwire_parse_response(&ctx, buf, total_len, &pos, 1, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, total_len);

    pos = total_len + 1;
    rv  = hwire_parse_response(&ctx, buf, total_len, &pos, SIZE_MAX, 1);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, total_len + 1);

    TEST_END();
}

int main(void)
{
    test_quoted_string_cursor();
    test_parameter_cursor();
    test_chunksize_cursor();
    test_header_cursor();
    test_request_cursor();
    test_response_cursor();
    print_test_summary();
    return g_tests_failed;
}
