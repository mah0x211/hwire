#include "test_helpers.h"

void test_parse_quoted_string_valid(void) {
    TEST_START("test_parse_quoted_string_valid");

    const char *str = "\"quoted string\"";
    size_t len = strlen(str);
    size_t pos = 0;
    int rv;

    rv = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 15); // Full length including quotes

    // With escaped characters
    str = "\"quoted\\\"string\""; // "quoted\"string"
    len = strlen(str);
    pos = 0;
    rv = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 16);

    TEST_END();
}

void test_parse_quoted_string_invalid(void) {
    TEST_START("test_parse_quoted_string_invalid");

    const char *str;
    size_t len;
    size_t pos;
    int rv;

    // Missing opening quote
    str = "no quotes";
    len = strlen(str);
    pos = 0;
    rv = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    // Missing closing quote (EAGAIN)
    str = "\"partial";
    len = strlen(str);
    pos = 0;
    rv = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    // Invalid escape (EILSEQ) - e.g., \ followed by invalid char
    // But hwire allows various chars after backslash... let's try \x00
    // Actually hwire check: is_vchar(c) || c == HT || c == SP
    // \x01 is CTL, not VCHAR.
    str = "\"bad escape \\" "\x01" "\"";
    len = strlen(str);
    pos = 0;
    rv = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    // Exceed maxlen
    str = "\"too long\"";
    len = strlen(str);
    pos = 0;
    // maxlen applies to content (approx) - strictly checking impl:
    // tail = cur + maxlen. If len > tail, returns HWIRE_ELEN.
    // So if maxlen=5, tail=5. len=10. Returns ELEN.
    rv = hwire_parse_quoted_string(str, len, &pos, 5);
    ASSERT_EQ(rv, HWIRE_ELEN);

    // pos >= len -> EAGAIN
    str = "abc";
    len = 3;
    pos = 3;
    rv = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    // Backslash at EOF -> EAGAIN
    str = "\"escape \\";
    len = strlen(str);
    pos = 0;
    rv = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

int main(void) {
    test_parse_quoted_string_valid();
    test_parse_quoted_string_invalid();
    print_test_summary();
    return g_tests_failed;
}
