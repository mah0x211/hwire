#include "test_helpers.h"

/*
 * Covers hwire_parse_chunksize with the optional chunksize_ext_cb unset.
 * The chunk-size callback remains required, while extensions must still be
 * parsed, validated, and counted against maxexts.
 */
void test_chunksize_optional_callback_single_extension(void)
{
    TEST_START("test_chunksize_optional_callback_single_extension");

    hwire_ctx_t ctx = {.chunksize_cb = mock_chunksize_cb};
    const char *buf = "1;foo=bar\r\n";
    size_t pos      = 0;
    int rv = hwire_parse_chunksize(&ctx, buf, strlen(buf), &pos, 100, 1);

    ASSERT_EQ(rv, HWIRE_OK);
    ASSERT_EQ(pos, strlen(buf));

    TEST_END();
}

void test_chunksize_optional_callback_multiple_extensions(void)
{
    TEST_START("test_chunksize_optional_callback_multiple_extensions");

    hwire_ctx_t ctx = {.chunksize_cb = mock_chunksize_cb};
    const char *buf = "2;foo=bar;baz=qux\r\n";
    size_t pos      = 0;
    int rv = hwire_parse_chunksize(&ctx, buf, strlen(buf), &pos, 100, 2);

    ASSERT_EQ(rv, HWIRE_OK);
    ASSERT_EQ(pos, strlen(buf));

    TEST_END();
}

void test_chunksize_optional_callback_maxexts(void)
{
    TEST_START("test_chunksize_optional_callback_maxexts");

    static const struct {
        const char *buf;
        uint8_t maxexts;
        int expected;
    } cases[] = {
        {"1;foo\r\n",     0, HWIRE_ENOBUFS},
        {"1;foo\r\n",     1, HWIRE_OK     },
        {"1;foo;bar\r\n", 1, HWIRE_ENOBUFS},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        hwire_ctx_t ctx = {.chunksize_cb = mock_chunksize_cb};
        size_t pos      = 0;
        int rv = hwire_parse_chunksize(&ctx, cases[i].buf, strlen(cases[i].buf),
                                       &pos, 100, cases[i].maxexts);

        ASSERT_EQ(rv, cases[i].expected);
    }

    TEST_END();
}

void test_chunksize_optional_callback_invalid_extension(void)
{
    TEST_START("test_chunksize_optional_callback_invalid_extension");

    hwire_ctx_t ctx = {.chunksize_cb = mock_chunksize_cb};
    const char *buf = "1;=bar\r\n";
    size_t pos      = 0;
    int rv = hwire_parse_chunksize(&ctx, buf, strlen(buf), &pos, 100, 1);

    ASSERT_EQ(rv, HWIRE_EEXTNAME);

    TEST_END();
}

int main(void)
{
    test_chunksize_optional_callback_single_extension();
    test_chunksize_optional_callback_multiple_extensions();
    test_chunksize_optional_callback_maxexts();
    test_chunksize_optional_callback_invalid_extension();
    print_test_summary();
    return g_tests_failed;
}
