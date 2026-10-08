#include "test_helpers.h"

typedef struct {
    const char *key;
    size_t keylen;
    int calls;
    int failed;
} key_expect_t;

static int capture_original_key(hwire_ctx_t *ctx, hwire_kv_pair_t *pair)
{
    key_expect_t *expected = ctx->uctx;
    expected->calls++;
    if (pair->key.ptr != expected->key || pair->key.len != expected->keylen) {
        expected->failed = 1;
    }
    return 0;
}

/* Every key-bearing token API preserves the input, including long names. */
static void test_original_keys(void)
{
    TEST_START("test_original_keys");
    static const size_t lengths[] = {1, 15, 16, 17, 63, 64, 65, 255, 256};
    static const char *const prefixes[] = {
        "", "GET / HTTP/1.1\r\n", "HTTP/1.1 200 OK\r\n", "; ", "1;"
    };
    static const char *const suffixes[] = {
        ": Value\r\n\r\n", ": Value\r\n\r\n", ": Value\r\n\r\n", "=Value", "=Value\r\n"
    };
    for (size_t kind = 0; kind < sizeof(prefixes) / sizeof(prefixes[0]); kind++) {
        for (size_t n = 0; n < sizeof(lengths) / sizeof(lengths[0]); n++) {
            char input[512];
            char original[sizeof(input)];
            size_t start = strlen(prefixes[kind]);
            size_t keylen = lengths[n];
            size_t len = start + keylen + strlen(suffixes[kind]);
            memcpy(input, prefixes[kind], start);
            for (size_t i = 0; i < keylen; i++) {
                input[start + i] = (i % 2) ? 'a' : 'Z';
            }
            memcpy(input + start + keylen, suffixes[kind], strlen(suffixes[kind]));
            memcpy(original, input, len);
            key_expect_t expected = {.key = input + start, .keylen = keylen};
            hwire_ctx_t ctx = {
                .uctx = &expected,
                .header_cb = capture_original_key,
                .param_cb = capture_original_key,
                .chunksize_ext_cb = capture_original_key,
                .request_cb = mock_request_cb,
                .response_cb = mock_response_cb,
                .chunksize_cb = mock_chunksize_cb
            };
            size_t pos = 0;
            int code;
            switch (kind) {
            case 0:
                code = hwire_parse_headers(&ctx, input, len, &pos, len);
                break;
            case 1:
                code = hwire_parse_request(&ctx, input, len, &pos, len);
                break;
            case 2:
                code = hwire_parse_response(&ctx, input, len, &pos, len);
                break;
            case 3:
                code = hwire_parse_parameters(&ctx, input, len, &pos, len, 0);
                break;
            default:
                code = hwire_parse_chunksize(&ctx, input, len, &pos, len);
                break;
            }
            ASSERT_OK(code);
            ASSERT_EQ(pos, len);
            ASSERT_EQ(expected.calls, 1);
            ASSERT_EQ(expected.failed, 0);
            ASSERT_EQ(memcmp(input, original, len), 0);
        }
    }
    TEST_END();
}

int main(void)
{
    test_original_keys();
    print_test_summary();
    return g_tests_failed;
}
