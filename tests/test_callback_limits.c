#include "test_helpers.h"

enum { REQUEST, RESPONSE, HEADERS, QUERY, PARAMETERS, CHUNKSIZE, API_COUNT };

static const char *const inputs[API_COUNT] = {
    "GET / HTTP/1.1\r\nA:1\r\nB:2\r\nC:3\r\n\r\n",
    "HTTP/1.1 200 OK\r\nA:1\r\nB:2\r\nC:3\r\n\r\n",
    "A:1\r\nB:2\r\nC:3\r\n\r\n",
    "A=1&B=2&C=3",
    ";A=1;B=2;C=3",
    "1;A=1;B=2;C=3\r\n",
};

static const char *const empty_inputs[API_COUNT] = {
    "GET / HTTP/1.1\r\n\r\n", "HTTP/1.1 200 OK\r\n\r\n",
    "\r\n", "&&", "; ; ", "1\r\n",
};

static int parse(int api, hwire_ctx_t *ctx, const char *input, size_t len,
                 size_t *pos, size_t maxlen)
{
    switch (api) {
    case REQUEST:
        return hwire_parse_request(ctx, input, len, pos, maxlen);
    case RESPONSE:
        return hwire_parse_response(ctx, input, len, pos, maxlen);
    case HEADERS:
        return hwire_parse_headers(ctx, input, len, pos, maxlen);
    case QUERY:
        return hwire_parse_query(ctx, input, len, pos, maxlen);
    case PARAMETERS:
        return hwire_parse_parameters(ctx, input, len, pos, maxlen, 0);
    default:
        return hwire_parse_chunksize(ctx, input, len, pos, maxlen);
    }
}

/* Application storage enforces zero, partial and exactly-full capacities. */
static void test_capacity_rejection(void)
{
    TEST_START("test_capacity_rejection");
    for (int api = 0; api < API_COUNT; api++) {
        for (size_t capacity = 0; capacity <= 3; capacity++) {
            char decoded[16];
            test_capacity_t storage = {.capacity = capacity};
            hwire_ctx_t ctx = {
                .uctx = &storage,
                .qrybuf = {.buf = decoded, .size = sizeof(decoded)},
                .request_cb = mock_request_cb,
                .response_cb = mock_response_cb,
                .header_cb = capacity_pair_cb,
                .query_cb = capacity_pair_cb,
                .param_cb = capacity_pair_cb,
                .chunksize_cb = mock_chunksize_cb,
                .chunksize_ext_cb = capacity_pair_cb,
            };
            size_t pos = 0;
            size_t len = strlen(inputs[api]);
            int rv = parse(api, &ctx, inputs[api], len, &pos, len);
            ASSERT_EQ(rv, capacity == 3 ? HWIRE_OK : HWIRE_ECALLBACK);
            ASSERT_EQ(storage.error, capacity == 3 ? HWIRE_OK : HWIRE_ENOBUFS);
            ASSERT_EQ(storage.count, capacity);
            ASSERT_EQ(storage.calls, capacity == 3 ? 3 : capacity + 1);
            for (size_t i = 0; i < storage.count; i++) {
                ASSERT_EQ(storage.pairs[i].key.len, 1);
                ASSERT_EQ(storage.pairs[i].key.ptr[0], 'A' + (int)i);
                ASSERT_EQ(storage.pairs[i].value.len, 1);
                ASSERT_EQ(storage.pairs[i].value.ptr[0], '1' + (int)i);
            }
            if (rv == HWIRE_OK) {
                ASSERT_EQ(pos, len);
            }
            /* Rejected entries never overwrite the next caller-owned slot. */
            ASSERT(storage.pairs[storage.count].key.ptr == NULL);
        }
    }
    TEST_END();
}

static void test_zero_capacity_without_items(void)
{
    TEST_START("test_zero_capacity_without_items");
    for (int api = 0; api < API_COUNT; api++) {
        char decoded;
        test_capacity_t storage = {0};
        hwire_ctx_t ctx = {
            .uctx = &storage,
            .qrybuf = {.buf = &decoded},
            .request_cb = mock_request_cb,
            .response_cb = mock_response_cb,
            .header_cb = capacity_pair_cb,
            .query_cb = capacity_pair_cb,
            .param_cb = capacity_pair_cb,
            .chunksize_cb = mock_chunksize_cb,
            .chunksize_ext_cb = capacity_pair_cb,
        };
        size_t pos = 0;
        size_t len = strlen(empty_inputs[api]);
        ASSERT_OK(parse(api, &ctx, empty_inputs[api], len, &pos, len));
        ASSERT_EQ(pos, len);
        ASSERT_EQ(storage.count, 0);
        ASSERT_EQ(storage.calls, 0);
        ASSERT_EQ(storage.error, 0);
    }
    TEST_END();
}

/* Syntax validation precedes callback policy, even with no storage space. */
static void test_invalid_before_callback(void)
{
    TEST_START("test_invalid_before_callback");
    static const char *const invalid[API_COUNT] = {
        "GET / HTTP/1.1\r\n\001A:1\r\n\r\n",
        "HTTP/1.1 200 OK\r\n\001A:1\r\n\r\n",
        "\001A:1\r\n\r\n", "A=%GG", ";A?", "1;=1\r\n",
    };
    static const int errors[API_COUNT] = {
        HWIRE_EHDRNAME, HWIRE_EHDRNAME, HWIRE_EHDRNAME,
        HWIRE_EURI, HWIRE_EILSEQ, HWIRE_EEXTNAME,
    };
    for (int api = 0; api < API_COUNT; api++) {
        char decoded[16];
        test_capacity_t storage = {0};
        hwire_ctx_t ctx = {
            .uctx = &storage,
            .qrybuf = {.buf = decoded, .size = sizeof(decoded)},
            .request_cb = mock_request_cb,
            .response_cb = mock_response_cb,
            .header_cb = capacity_pair_cb,
            .query_cb = capacity_pair_cb,
            .param_cb = capacity_pair_cb,
            .chunksize_cb = mock_chunksize_cb,
            .chunksize_ext_cb = capacity_pair_cb,
        };
        size_t pos = 0;
        size_t len = strlen(invalid[api]);
        ASSERT_EQ(parse(api, &ctx, invalid[api], len, &pos, len), errors[api]);
        ASSERT_EQ(storage.count, 0);
        ASSERT_EQ(storage.calls, 0);
        ASSERT_EQ(storage.error, 0);
    }
    TEST_END();
}

static int count_pair(hwire_ctx_t *ctx, hwire_kv_pair_t *pair)
{
    (void)pair;
    size_t *count = ctx->uctx;
    (*count)++;
    return 0;
}

/* Generate a bounded fixture just beyond the former integer count ceiling. */
static size_t make_many(char *input, int api, size_t count)
{
    const char *prefix = api == REQUEST ? "GET / HTTP/1.1\r\n" :
                         api == RESPONSE ? "HTTP/1.1 200 OK\r\n" :
                         api == CHUNKSIZE ? "1" : "";
    const char *item = api == QUERY ? "=" :
                       api == PARAMETERS || api == CHUNKSIZE ? ";A=1" :
                                                                  "A:1\r\n";
    const char *ending = api == QUERY || api == PARAMETERS ? "" : "\r\n";
    char *out = input;
    size_t len = strlen(prefix);
    memcpy(out, prefix, len);
    out += len;
    len = strlen(item);
    for (size_t i = 0; i < count; i++) {
        if (api == QUERY && i != 0) {
            *out++ = '&';
        }
        memcpy(out, item, len);
        out += len;
    }
    len = strlen(ending);
    memcpy(out, ending, len);
    out += len;
    *out = '\0';
    return (size_t)(out - input);
}

static void test_former_count_boundaries(void)
{
    TEST_START("test_former_count_boundaries");
    static char input[2 * 65537 + 64];
    for (int api = 0; api < API_COUNT; api++) {
        size_t expected = api == QUERY ? 65537 : 257;
        size_t len = make_many(input, api, expected);
        char decoded;
        size_t count = 0;
        hwire_ctx_t ctx = {
            .uctx = &count,
            .qrybuf = {.buf = &decoded},
            .request_cb = mock_request_cb,
            .response_cb = mock_response_cb,
            .header_cb = count_pair,
            .query_cb = count_pair,
            .param_cb = count_pair,
            .chunksize_cb = mock_chunksize_cb,
            .chunksize_ext_cb = count_pair,
        };
        size_t pos = 0;
        ASSERT_OK(parse(api, &ctx, input, len, &pos, len));
        ASSERT_EQ(count, expected);
        ASSERT_EQ(pos, len);

        /* Byte budgets continue to apply after removing item-count limits. */
        count = 0;
        pos = 0;
        int rv = parse(api, &ctx, input, len, &pos, len - 1);
        ASSERT_EQ(rv, api == REQUEST || api == RESPONSE || api == HEADERS ?
                          HWIRE_EHDRLEN : HWIRE_ELEN);
        if (api == CHUNKSIZE) {
            ctx.chunksize_ext_cb = NULL;
            pos = 0;
            ASSERT_OK(parse(api, &ctx, input, len, &pos, len));
            ASSERT_EQ(pos, len);
            memcpy(input + len - 2, ";=\r\n", 5);
            pos = 0;
            ASSERT_EQ(parse(api, &ctx, input, len + 2, &pos, len + 2),
                      HWIRE_EEXTNAME);
        }
    }
    TEST_END();
}

/* EAGAIN replays callbacks; application storage is reset before retry. */
static void test_retry_resets_storage(void)
{
    TEST_START("test_retry_resets_storage");
    const char *full = "A:1\r\nB:2\r\n\r\n";
    test_capacity_t storage = {.capacity = 2};
    hwire_ctx_t ctx = {.uctx = &storage, .header_cb = capacity_pair_cb};
    size_t pos = 0;
    ASSERT_EQ(hwire_parse_headers(&ctx, full, 7, &pos, SIZE_MAX), HWIRE_EAGAIN);
    ASSERT_EQ(pos, 0);
    ASSERT_EQ(storage.count, 1);
    storage = (test_capacity_t){.capacity = 2};
    ASSERT_OK(hwire_parse_headers(&ctx, full, strlen(full), &pos, SIZE_MAX));
    ASSERT_EQ(storage.count, 2);
    ASSERT_EQ(storage.calls, 2);
    ASSERT_EQ(storage.error, 0);
    ASSERT_EQ(pos, strlen(full));
    TEST_END();
}

int main(void)
{
    test_capacity_rejection();
    test_zero_capacity_without_items();
    test_invalid_before_callback();
    test_former_count_boundaries();
    test_retry_resets_storage();
    print_test_summary();
    return g_tests_failed;
}
