#include "test_helpers.h"

typedef struct {
    hwire_query_param_t params[16];
    size_t count;
    size_t capacity;
    size_t fail_at;
    int error;
} query_capture_t;

static int capture_query(hwire_ctx_t *ctx, hwire_query_param_t *param)
{
    query_capture_t *capture = (query_capture_t *)ctx->uctx;
    if (capture->count == capture->fail_at) {
        return 1;
    }
    if (capture->count >= capture->capacity ||
        capture->count >= sizeof(capture->params) / sizeof(capture->params[0])) {
        capture->error = HWIRE_ENOBUFS;
        return 1;
    }
    capture->params[capture->count++] = *param;
    return 0;
}

#define ASSERT_SLICE(slice, literal)                                           \
    do {                                                                       \
        ASSERT_EQ((slice).len, sizeof(literal) - 1);                           \
        ASSERT(memcmp((slice).ptr, literal, sizeof(literal) - 1) == 0);        \
    } while (0)

static void test_pairs_and_structure(void)
{
    TEST_START("test_pairs_and_structure");
    const char *query = "a=1&&a=2;3&=empty&bare&a=&encoded=%26%3D+&x=y=z";
    char storage[sizeof("a=1&&a=2;3&=empty&bare&a=&encoded=%26%3D+&x=y=z")];
    hwire_buf_t decoded     = {.buf = storage, .size = strlen(query)};
    query_capture_t capture = {.capacity = 16, .fail_at = SIZE_MAX};
    hwire_ctx_t ctx         = {.uctx = &capture, .query_cb = capture_query};
    ctx.qrybuf              = decoded;
    size_t pos              = 0;

    ASSERT_OK(
        hwire_parse_query(&ctx, query, strlen(query), &pos, strlen(query)));
    ASSERT_EQ(pos, strlen(query));
    ASSERT_EQ(capture.count, 7);
    ASSERT_SLICE(capture.params[0].key, "a");
    ASSERT_SLICE(capture.params[0].value, "1");
    ASSERT_SLICE(capture.params[1].key, "a");
    ASSERT_SLICE(capture.params[1].value, "2;3");
    ASSERT_EQ(capture.params[2].key.len, 0);
    ASSERT(capture.params[2].key.ptr != NULL);
    ASSERT_SLICE(capture.params[2].value, "empty");
    ASSERT_SLICE(capture.params[3].key, "bare");
    ASSERT(capture.params[3].value.ptr == NULL);
    ASSERT_SLICE(capture.params[4].key, "a");
    ASSERT(capture.params[4].value.ptr != NULL);
    ASSERT_EQ(capture.params[4].value.len, 0);
    ASSERT_SLICE(capture.params[5].value, "&= ");
    ASSERT_SLICE(capture.params[6].value, "y=z");
    for (size_t i = 0; i < capture.count; i++) {
        ASSERT(capture.params[i].key.ptr >= storage);
        ASSERT(capture.params[i].key.ptr <= storage + ctx.qrybuf.len);
        if (capture.params[i].value.ptr != NULL) {
            ASSERT(capture.params[i].value.ptr >= storage);
            ASSERT(capture.params[i].value.ptr <= storage + ctx.qrybuf.len);
        }
    }
    TEST_END();
}

static void test_decoded_pairs(void)
{
    TEST_START("test_decoded_pairs");
    const char *query = "a+b=x%26y%3Dz&nul=%00&semi=1;2&v=+&%3D=%26";
    char storage[sizeof("a+b=x%26y%3Dz&nul=%00&semi=1;2&v=+&%3D=%26")];
    hwire_buf_t decoded = {.buf = storage, .size = strlen(query), .len = 99};
    query_capture_t capture = {.capacity = 16, .fail_at = SIZE_MAX};
    hwire_ctx_t ctx         = {.uctx = &capture, .query_cb = capture_query};
    ctx.qrybuf              = decoded;
    size_t pos              = 0;

    ASSERT_OK(
        hwire_parse_query(&ctx, query, strlen(query), &pos, strlen(query)));
    ASSERT_EQ(pos, strlen(query));
    ASSERT_EQ(capture.count, 5);
    ASSERT_SLICE(capture.params[0].key, "a b");
    ASSERT_SLICE(capture.params[0].value, "x&y=z");
    ASSERT_SLICE(capture.params[1].key, "nul");
    ASSERT_EQ(capture.params[1].value.len, 1);
    ASSERT_EQ((unsigned char)capture.params[1].value.ptr[0], 0);
    ASSERT_SLICE(capture.params[2].value, "1;2");
    ASSERT_SLICE(capture.params[3].value, " ");
    ASSERT_SLICE(capture.params[4].key, "=");
    ASSERT_SLICE(capture.params[4].value, "&");
    ASSERT_SLICE(capture.params[0].value, "x&y=z");
    ASSERT_EQ(ctx.qrybuf.len, 23);
    for (size_t i = 0; i < capture.count; i++) {
        ASSERT(capture.params[i].key.ptr >= storage);
        ASSERT(capture.params[i].key.ptr + capture.params[i].key.len <=
               storage + ctx.qrybuf.len);
        ASSERT(capture.params[i].value.ptr >= storage);
        ASSERT(capture.params[i].value.ptr + capture.params[i].value.len <=
               storage + ctx.qrybuf.len);
    }
    ASSERT(strcmp(query, "a+b=x%26y%3Dz&nul=%00&semi=1;2&v=+&%3D=%26") == 0);
    TEST_END();
}

static void test_empty_and_offset(void)
{
    TEST_START("test_empty_and_offset");
    query_capture_t capture = {.capacity = 16, .fail_at = SIZE_MAX};
    hwire_ctx_t ctx         = {.uctx = &capture, .query_cb = capture_query};
    char storage[16];
    hwire_buf_t decoded = {.buf = storage, .size = 0, .len = 99};
    ctx.qrybuf          = decoded;
    size_t pos          = 0;

    ASSERT_OK(hwire_parse_query(&ctx, "", 0, &pos, 0));
    ASSERT_EQ(ctx.qrybuf.len, 0);
    ASSERT_EQ(capture.count, 0);
    ASSERT_OK(hwire_parse_query(&ctx, "&&", 2, &pos, 2));
    ASSERT_EQ(pos, 2);
    ASSERT_EQ(capture.count, 0);

    const char *input = "xxx?key=value";
    pos               = 4;
    ctx.qrybuf.size   = sizeof(storage);
    ASSERT_OK(hwire_parse_query(&ctx, input, strlen(input), &pos,
                                strlen(input) - 4));
    ASSERT_EQ(pos, strlen(input));
    ASSERT_EQ(capture.count, 1);
    ASSERT_SLICE(capture.params[0].key, "key");
    ASSERT_SLICE(capture.params[0].value, "value");

    const char *partial = "xxx?one=1&two=2";
    pos                 = 4;
    capture.count       = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, partial, strlen(partial), &pos, 6),
              HWIRE_ELEN);
    ASSERT_EQ(pos, 10);
    ASSERT_EQ(capture.count, 1);
    ASSERT_SLICE(capture.params[0].key, "one");
    ASSERT_SLICE(capture.params[0].value, "1");

    pos            = strlen(input) + 1;
    ctx.qrybuf.len = 99;
    ASSERT_EQ(hwire_parse_query(&ctx, input, strlen(input), &pos, 100),
              HWIRE_EILSEQ);
    ASSERT_EQ(pos, strlen(input) + 1);
    ASSERT_EQ(ctx.qrybuf.len, 99);
    TEST_END();
}

static void test_invalid_query(void)
{
    TEST_START("test_invalid_query");
    const char *invalid[] = {"x=%G0", "x=%0G",  "x=hello world",
                             "x=#",   "x=\x7f", "x=\xc3\xa9",
                             "x=<"};
    char storage[32];
    hwire_buf_t decoded     = {.buf = storage, .size = sizeof(storage)};
    query_capture_t capture = {.capacity = 16, .fail_at = SIZE_MAX};
    hwire_ctx_t ctx         = {.uctx = &capture, .query_cb = capture_query};
    ctx.qrybuf              = decoded;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        size_t pos = 0;
        ASSERT_EQ(hwire_parse_query(&ctx, invalid[i], strlen(invalid[i]), &pos,
                                    SIZE_MAX),
                  HWIRE_EURI);
        ASSERT_EQ(pos, 0);
        ASSERT_EQ(capture.count, 0);
    }
    const char with_nul[] = {'x', '=', '\0'};
    size_t pos            = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, with_nul, sizeof(with_nul), &pos, 3),
              HWIRE_EURI);
    ASSERT_EQ(pos, 0);
    TEST_END();
}

static void test_limits_and_partial_output(void)
{
    TEST_START("test_limits_and_partial_output");
    const char *query       = "one=1&two=2";
    query_capture_t capture = {.capacity = 16, .fail_at = SIZE_MAX};
    hwire_ctx_t ctx         = {.uctx = &capture, .query_cb = capture_query};
    size_t pos              = 0;
    char storage[32];
    hwire_buf_t decoded = {.buf = storage, .size = sizeof(storage)};
    ctx.qrybuf          = decoded;

    ASSERT_EQ(hwire_parse_query(&ctx, query, strlen(query), &pos, 5),
              HWIRE_ELEN);
    ASSERT_EQ(pos, 5);
    ASSERT_EQ(capture.count, 1);
    ASSERT_SLICE(capture.params[0].key, "one");
    ASSERT_SLICE(capture.params[0].value, "1");
    capture.count = 0;
    pos           = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, query, strlen(query), &pos, 6),
              HWIRE_ELEN);
    ASSERT_EQ(pos, 6);
    ASSERT_EQ(capture.count, 1);
    ASSERT_SLICE(capture.params[0].value, "1");

    capture.count = 0;
    capture.capacity = 1;
    pos = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, query, strlen(query), &pos, SIZE_MAX),
              HWIRE_ECALLBACK);
    ASSERT_EQ(capture.error, HWIRE_ENOBUFS);
    ASSERT_EQ(pos, 6);
    ASSERT_EQ(capture.count, 1);

    capture.count   = 0;
    capture.capacity = 16;
    capture.error = 0;
    capture.fail_at = 1;
    pos             = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, query, strlen(query), &pos, SIZE_MAX),
              HWIRE_ECALLBACK);
    ASSERT_EQ(pos, 6);
    ASSERT_EQ(capture.count, 1);

    capture.fail_at = SIZE_MAX;
    capture.count   = 0;
    pos             = 0;
    ctx.qrybuf.size = 3;
    ctx.qrybuf.len  = 99;
    ASSERT_EQ(hwire_parse_query(&ctx, query, strlen(query), &pos, SIZE_MAX),
              HWIRE_ENOBUFS);
    ASSERT_EQ(pos, 0);
    ASSERT_EQ(capture.count, 0);
    ASSERT_EQ(ctx.qrybuf.len, 0);

    pos = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, "x=%00", 5, &pos, 3), HWIRE_ELEN);
    ASSERT_EQ(pos, 0);
    TEST_END();
}

static void test_simd_boundaries(void)
{
    TEST_START("test_simd_boundaries");
    query_capture_t capture = {.capacity = 16, .fail_at = SIZE_MAX};
    hwire_ctx_t ctx         = {.uctx = &capture, .query_cb = capture_query};
    char query[97];
    char storage[97];
    hwire_buf_t decoded = {.buf = storage, .size = sizeof(storage)};
    ctx.qrybuf          = decoded;
    for (size_t i = 0; i < 64; i++) {
        memset(query, 'a', 96);
        query[96]     = '\0';
        query[i]      = '&';
        size_t pos    = 0;
        capture.count = 0;
        ASSERT_OK(hwire_parse_query(&ctx, query, 96, &pos, 96));
        ASSERT_EQ(pos, 96);
        ASSERT_EQ(capture.count, i == 0 ? 1 : 2);

        query[i]      = '=';
        pos           = 0;
        capture.count = 0;
        ASSERT_OK(hwire_parse_query(&ctx, query, 96, &pos, 96));
        ASSERT_EQ(capture.count, 1);
        ASSERT_EQ(capture.params[0].key.len, i);

        query[i]      = '#';
        pos           = 0;
        capture.count = 0;
        ASSERT_EQ(hwire_parse_query(&ctx, query, 96, &pos, 96), HWIRE_EURI);
        ASSERT_EQ(pos, 0);
    }
    TEST_END();
}

static void test_query_byte_classes(void)
{
    TEST_START("test_query_byte_classes");
    const char *allowed =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
        "-._~!$&'()*+,;=:@/?%";
    query_capture_t capture = {.capacity = 16, .fail_at = SIZE_MAX};
    hwire_ctx_t ctx         = {.uctx = &capture, .query_cb = capture_query};
    char query[65];
    char storage[65];
    hwire_buf_t decoded = {.buf = storage, .size = sizeof(storage)};
    ctx.qrybuf          = decoded;

    for (unsigned int byte = 0; byte < 256; byte++) {
        memset(query, 'a', sizeof(query));
        query[32]     = (char)byte;
        int valid     = byte != 0 && strchr(allowed, (int)byte) != NULL;
        size_t pos    = 0;
        capture.count = 0;
        int rv        = hwire_parse_query(&ctx, query, sizeof(query), &pos,
                                          sizeof(query));
        ASSERT_EQ(rv, valid ? HWIRE_OK : HWIRE_EURI);
        ASSERT_EQ(pos, valid ? sizeof(query) : 0);
        ASSERT_EQ(capture.count, valid ? (byte == '&' ? 2 : 1) : 0);
    }
    TEST_END();
}

static void test_long_query(void)
{
    TEST_START("test_long_query");
    char query[8192];
    char storage[sizeof(query)];
    memset(query, 'a', sizeof(query));
    query_capture_t capture = {.capacity = 16, .fail_at = SIZE_MAX};
    hwire_ctx_t ctx         = {.uctx = &capture, .query_cb = capture_query};
    hwire_buf_t decoded     = {.buf = storage, .size = sizeof(storage)};
    ctx.qrybuf              = decoded;
    size_t pos              = 0;

    ASSERT_OK(
        hwire_parse_query(&ctx, query, sizeof(query), &pos, sizeof(query)));
    ASSERT_EQ(capture.count, 1);
    ASSERT_EQ(ctx.qrybuf.len, sizeof(query));
    ASSERT(capture.params[0].key.ptr == storage);
    ASSERT(memcmp(storage, query, sizeof(query)) == 0);
    TEST_END();
}

static void test_decoded_failures(void)
{
    TEST_START("test_decoded_failures");
    query_capture_t capture = {.capacity = 16, .fail_at = SIZE_MAX};
    hwire_ctx_t ctx         = {.uctx = &capture, .query_cb = capture_query};
    char storage[32];
    hwire_buf_t decoded      = {.buf = storage, .size = sizeof(storage)};
    ctx.qrybuf               = decoded;
    const char *incomplete[] = {"x=%", "x=%0"};
    const char *invalid[]    = {"x=%G0", "x=%0G"};

    for (size_t i = 0; i < sizeof(incomplete) / sizeof(incomplete[0]); i++) {
        size_t pos = 0;
        size_t len = strlen(incomplete[i]);
        ASSERT_EQ(hwire_parse_query(&ctx, incomplete[i], len, &pos, len + 1),
                  HWIRE_EAGAIN);
        ASSERT_EQ(pos, 0);
        ASSERT_EQ(capture.count, 0);
        ASSERT_EQ(ctx.qrybuf.len, 0);
        ASSERT_EQ(hwire_parse_query(&ctx, incomplete[i], len, &pos, len),
                  HWIRE_ELEN);
        ASSERT_EQ(pos, 0);
        ASSERT_EQ(capture.count, 0);
        ASSERT_EQ(ctx.qrybuf.len, 0);
    }

    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        size_t pos = 0;
        ASSERT_EQ(hwire_parse_query(&ctx, invalid[i], strlen(invalid[i]), &pos,
                                    SIZE_MAX),
                  HWIRE_EURI);
        ASSERT_EQ(pos, 0);
        ASSERT_EQ(ctx.qrybuf.len, 0);
    }

    size_t pos = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, "x=%00", 5, &pos, 3), HWIRE_ELEN);
    ASSERT_EQ(pos, 0);
    ASSERT_EQ(ctx.qrybuf.len, 0);

    const char *query = "one=1&two=2";
    pos               = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, query, strlen(query), &pos, 6),
              HWIRE_ELEN);
    ASSERT_EQ(pos, 6);
    ASSERT_EQ(capture.count, 1);
    ASSERT_EQ(ctx.qrybuf.len, 4);

    capture.count = 0;
    capture.capacity = 1;
    pos = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, query, strlen(query), &pos, SIZE_MAX),
              HWIRE_ECALLBACK);
    ASSERT_EQ(capture.error, HWIRE_ENOBUFS);
    ASSERT_EQ(pos, 6);
    ASSERT_EQ(capture.count, 1);
    ASSERT_EQ(ctx.qrybuf.len, 4);

    capture.count   = 0;
    capture.capacity = 16;
    capture.error = 0;
    capture.fail_at = 1;
    pos             = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, query, strlen(query), &pos, SIZE_MAX),
              HWIRE_ECALLBACK);
    ASSERT_EQ(pos, 6);
    ASSERT_EQ(capture.count, 1);
    ASSERT_EQ(ctx.qrybuf.len, 4);

    capture.fail_at = SIZE_MAX;
    capture.count   = 0;
    pos             = 0;
    ASSERT_OK(hwire_parse_query(&ctx, "&&x=y=z", 7, &pos, 7));
    ASSERT_EQ(capture.count, 1);
    ASSERT_SLICE(capture.params[0].value, "y=z");

    ctx.qrybuf.size = 0;
    capture.count   = 0;
    pos             = 0;
    ASSERT_OK(hwire_parse_query(&ctx, "=", 1, &pos, 1));
    ASSERT_EQ(pos, 1);
    ASSERT_EQ(capture.count, 1);
    ASSERT_EQ(capture.params[0].key.len, 0);
    ASSERT(capture.params[0].key.ptr == storage);
    ASSERT_EQ(capture.params[0].value.len, 0);
    ASSERT(capture.params[0].value.ptr == storage);
    ASSERT_EQ(ctx.qrybuf.len, 0);
    pos = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, "a", 1, &pos, 1), HWIRE_ENOBUFS);
    ASSERT_EQ(pos, 0);
    pos = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, "%41", 3, &pos, 3), HWIRE_ENOBUFS);
    ASSERT_EQ(pos, 0);

    ctx.qrybuf.size = 1;
    pos             = 0;
    ASSERT_EQ(hwire_parse_query(&ctx, "a+", 2, &pos, 2), HWIRE_ENOBUFS);
    ASSERT_EQ(pos, 0);
    ASSERT_EQ(ctx.qrybuf.len, 0);
    TEST_END();
}

int main(void)
{
    test_pairs_and_structure();
    test_decoded_pairs();
    test_empty_and_offset();
    test_invalid_query();
    test_limits_and_partial_output();
    test_simd_boundaries();
    test_query_byte_classes();
    test_long_query();
    test_decoded_failures();
    print_test_summary();
    return g_tests_failed;
}
