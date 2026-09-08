#include "test_helpers.h"

typedef struct {
    int calls;
    size_t key_len;
    size_t value_len;
} extension_result_t;

static int capture_extension(hwire_ctx_t *ctx, hwire_chunksize_ext_t *ext)
{
    extension_result_t *result = (extension_result_t *)ctx->uctx;

    result->calls++;
    result->key_len   = ext->key.len;
    result->value_len = ext->value.len;
    return 0;
}

/* RFC 9112 section 7.1.1 permits an empty quoted-string extension value. */
void test_chunksize_empty_quoted_extension_value(void)
{
    TEST_START("test_chunksize_empty_quoted_extension_value");

    extension_result_t result = {0, 0, 0};
    hwire_ctx_t ctx           = {.uctx             = &result,
                                 .chunksize_cb     = mock_chunksize_cb,
                                 .chunksize_ext_cb = capture_extension};
    const char *buf           = "1;foo=\"\"\r\n";
    size_t pos                = 0;

    ASSERT_OK(hwire_parse_chunksize(&ctx, buf, strlen(buf), &pos, 100, 1));
    ASSERT_EQ(pos, strlen(buf));
    ASSERT_EQ(result.calls, 1);
    ASSERT_EQ(result.key_len, 3);
    ASSERT_EQ(result.value_len, 0);

    TEST_END();
}

/*
 * RFC 9112 section 7.1.1 defines chunk-ext-val as token / quoted-string.
 * Once '=' is present, a line or extension delimiter after optional BWS means
 * that the required value is empty. The invalid extension must not be
 * delivered.
 */
void test_chunksize_rejects_empty_token_extension_values(void)
{
    TEST_START("test_chunksize_rejects_empty_token_extension_values");

    static const char *cases[] = {
        "1;foo=\r\n",   "1;foo=\n",       "1;foo= \r\n",
        "1;foo=\t\r\n", "1;foo=;bar\r\n",
    };

    for (size_t use_callback = 0; use_callback < 2; use_callback++) {
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            extension_result_t result = {0, 0, 0};
            hwire_ctx_t ctx = {.uctx         = &result,
                               .chunksize_cb = mock_chunksize_cb,
                               .chunksize_ext_cb =
                                   use_callback ? capture_extension : NULL};
            size_t pos      = 0;

            ASSERT_EQ(hwire_parse_chunksize(&ctx, cases[i], strlen(cases[i]),
                                            &pos, 100, 10),
                      HWIRE_EEXTVAL);
            ASSERT_EQ(pos, 0);
            ASSERT_EQ(result.calls, 0);
        }
    }

    TEST_END();
}

/* Input ending after '=' or its BWS remains incomplete until maxlen is used. */
void test_chunksize_empty_token_extension_value_fragments(void)
{
    TEST_START("test_chunksize_empty_token_extension_value_fragments");

    static const char *cases[] = {
        "1;foo=",
        "1;foo= ",
        "1;foo=\t",
    };

    for (size_t use_callback = 0; use_callback < 2; use_callback++) {
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            extension_result_t result = {0, 0, 0};
            hwire_ctx_t ctx = {.uctx         = &result,
                               .chunksize_cb = mock_chunksize_cb,
                               .chunksize_ext_cb =
                                   use_callback ? capture_extension : NULL};
            size_t len      = strlen(cases[i]);
            size_t pos      = 0;

            ASSERT_EQ(
                hwire_parse_chunksize(&ctx, cases[i], len, &pos, len + 1, 10),
                HWIRE_EAGAIN);
            ASSERT_EQ(pos, 0);
            ASSERT_EQ(hwire_parse_chunksize(&ctx, cases[i], len, &pos, len, 10),
                      HWIRE_ELEN);
            ASSERT_EQ(pos, 0);
            ASSERT_EQ(result.calls, 0);
        }
    }

    TEST_END();
}

int main(void)
{
    test_chunksize_empty_quoted_extension_value();
    test_chunksize_rejects_empty_token_extension_values();
    test_chunksize_empty_token_extension_value_fragments();
    print_test_summary();
    return g_tests_failed;
}
