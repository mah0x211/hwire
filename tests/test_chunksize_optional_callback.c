#include "test_helpers.h"

/*
 * Covers hwire_parse_chunksize with the optional chunksize_ext_cb unset.
 * The chunk-size callback remains required, while extensions must still be
 * parsed and validated without an extension-count limit.
 */
void test_chunksize_optional_callback_single_extension(void)
{
    TEST_START("test_chunksize_optional_callback_single_extension");

    hwire_ctx_t ctx = {.chunksize_cb = mock_chunksize_cb};
    const char *buf = "1;foo=bar\r\n";
    size_t pos      = 0;
    int rv = hwire_parse_chunksize(&ctx, buf, strlen(buf), &pos, 100);

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
    int rv = hwire_parse_chunksize(&ctx, buf, strlen(buf), &pos, 100);

    ASSERT_EQ(rv, HWIRE_OK);
    ASSERT_EQ(pos, strlen(buf));

    TEST_END();
}

void test_chunksize_optional_callback_unlimited(void)
{
    TEST_START("test_chunksize_optional_callback_unlimited");
    const char *cases[] = {"1;foo\r\n", "1;foo;bar\r\n"};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        hwire_ctx_t ctx = {.chunksize_cb = mock_chunksize_cb};
        size_t pos = 0;
        ASSERT_OK(hwire_parse_chunksize(&ctx, cases[i], strlen(cases[i]),
                                        &pos, 100));
        ASSERT_EQ(pos, strlen(cases[i]));
    }
    TEST_END();
}

void test_chunksize_optional_callback_invalid_extension(void)
{
    TEST_START("test_chunksize_optional_callback_invalid_extension");

    hwire_ctx_t ctx = {.chunksize_cb = mock_chunksize_cb};
    const char *buf = "1;=bar\r\n";
    size_t pos      = 0;
    int rv = hwire_parse_chunksize(&ctx, buf, strlen(buf), &pos, 100);

    ASSERT_EQ(rv, HWIRE_EEXTNAME);

    TEST_END();
}

int main(void)
{
    test_chunksize_optional_callback_single_extension();
    test_chunksize_optional_callback_multiple_extensions();
    test_chunksize_optional_callback_unlimited();
    test_chunksize_optional_callback_invalid_extension();
    print_test_summary();
    return g_tests_failed;
}
