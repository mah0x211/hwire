#include "test_helpers.h"

void test_parse_quoted_string_valid(void)
{
    TEST_START("test_parse_quoted_string_valid");

    const char *str = "\"quoted string\"";
    size_t len      = strlen(str);
    size_t pos      = 0;
    int rv;

    rv = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 15); // Full length including quotes

    // With escaped characters
    str = "\"quoted\\\"string\""; // "quoted\"string"
    len = strlen(str);
    pos = 0;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 16);

    TEST_END();
}

void test_parse_quoted_string_invalid(void)
{
    TEST_START("test_parse_quoted_string_invalid");

    const char *str;
    size_t len;
    size_t pos;
    int rv;

    // Missing opening quote
    str = "no quotes";
    len = strlen(str);
    pos = 0;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    // Missing closing quote (EAGAIN)
    str = "\"partial";
    len = strlen(str);
    pos = 0;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    // Invalid escape (EILSEQ) - e.g., \ followed by invalid char
    // But hwire allows various chars after backslash... let's try \x00
    // Actually hwire check: is_vchar(c) || c == HT || c == SP
    // \x01 is CTL, not VCHAR.
    str = "\"bad escape \\"
          "\x01"
          "\"";
    len = strlen(str);
    pos = 0;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    // Exceed maxlen
    str = "\"too long\"";
    len = strlen(str);
    pos = 0;
    // maxlen applies to content (approx) - strictly checking impl:
    // tail = cur + maxlen. If len > tail, returns HWIRE_ELEN.
    // So if maxlen=5, tail=5. len=10. Returns ELEN.
    rv  = hwire_parse_quoted_string(str, len, &pos, 5);
    ASSERT_EQ(rv, HWIRE_ELEN);

    // pos >= len -> EAGAIN
    str = "abc";
    len = 3;
    pos = 3;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    // Backslash at EOF -> EAGAIN
    str = "\"escape \\";
    len = strlen(str);
    pos = 0;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

void test_parse_quoted_string_rfc_compliance(void)
{
    TEST_START("test_parse_quoted_string_rfc_compliance");

    const char *str;
    size_t len;
    size_t pos = 0;
    int rv;

    /* 1. quoted-pair with VCHAR (RFC 7230 3.2.6) */
    /* specific chars: HTAB, SP, VCHAR, obs-text */
    str = "\"quoted pair: \\t \\  \\\" \\A\""; // \t, \ , \", \A
    len = strlen(str);
    pos = 0;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, len);

    /* 2. obs-text (UTF-8 / High bit set) */
    /* RFC 7230 3.2.6: qdtext = ... / obs-text */
    /* obs-text = %x80-FF */
    str = "\"UTF-8 text: こんにちは\""; // bytes > 0x7F
    len = strlen(str);
    pos = 0;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, len);

    /* 3. Empty quoted string */
    str = "\"\"";
    len = strlen(str);
    pos = 0;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 2);

    TEST_END();
}

void test_parse_quoted_string_rfc_invalid(void)
{
    TEST_START("test_parse_quoted_string_rfc_invalid");

    const char *str;
    size_t len;
    size_t pos = 0;
    int rv;

    /* 1. Invalid escaped char (CTL: \x01) */
    /* quoted-pair = "\" ( HTAB / SP / VCHAR / obs-text ) */
    /* \x01 is not allowed */
    str = "\"bad \\"
          "\x01"
          "\"";
    len = strlen(str);
    pos = 0;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* 2. Invalid char in qdtext (CTL: \x1F) */
    /* qdtext = HTAB / SP / ... */
    str = "\"cntrl \x1F\"";
    len = strlen(str);
    pos = 0;
    rv  = hwire_parse_quoted_string(str, len, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    TEST_END();
}

int main(void)
{
    test_parse_quoted_string_valid();
    test_parse_quoted_string_invalid();
    test_parse_quoted_string_rfc_compliance();
    test_parse_quoted_string_rfc_invalid();
    print_test_summary();
    return g_tests_failed;
}
