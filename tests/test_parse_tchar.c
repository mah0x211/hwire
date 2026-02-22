#include "test_helpers.h"

/*
 * Covers: RFC 9110 §5.6.2
 *   tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" / "-" / "." /
 *           "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA
 *   token = 1*tchar
 * MUST: header field-name and other HTTP tokens consist only of tchar
 * characters. Note: '|' (0x7C) and '~' (0x7E) are valid tchar but are edge
 * cases for range-based or nibble-trick SIMD implementations.
 */
void test_is_tchar(void)
{
    TEST_START("test_is_tchar");

    // MUST accept: ALPHA (a-z, A-Z), DIGIT (0-9), and all symbol tchars
    ASSERT(hwire_is_tchar('a'));
    ASSERT(hwire_is_tchar('z'));
    ASSERT(hwire_is_tchar('A'));
    ASSERT(hwire_is_tchar('Z'));
    ASSERT(hwire_is_tchar('0'));
    ASSERT(hwire_is_tchar('9'));
    ASSERT(hwire_is_tchar('!'));
    ASSERT(hwire_is_tchar('#'));
    ASSERT(hwire_is_tchar('$'));
    ASSERT(hwire_is_tchar('%'));
    ASSERT(hwire_is_tchar('&'));
    ASSERT(hwire_is_tchar('\''));
    ASSERT(hwire_is_tchar('*'));
    ASSERT(hwire_is_tchar('+'));
    ASSERT(hwire_is_tchar('-'));
    ASSERT(hwire_is_tchar('.'));
    ASSERT(hwire_is_tchar('^'));
    ASSERT(hwire_is_tchar('_'));
    ASSERT(hwire_is_tchar('`'));
    // MUST accept: '|' (0x7C) and '~' (0x7E) — highest valid tchar values
    ASSERT(hwire_is_tchar('|'));
    ASSERT(hwire_is_tchar('~'));

    // MUST reject: characters not in tchar
    // RFC 9110 §5.6.2 separators: "(" ")" "," "/" ":" ";" "<" "=" ">" "?" "@"
    // "[" "\" "]" "{" "}"
    ASSERT(!hwire_is_tchar(0));   // NUL  (0x00): CTL, not tchar
    ASSERT(!hwire_is_tchar(' ')); // SP   (0x20): delimiter, not tchar
    ASSERT(!hwire_is_tchar(
        '\t')); // HTAB (0x09): only valid as OWS in field-value, not tchar
    ASSERT(!hwire_is_tchar('('));
    ASSERT(!hwire_is_tchar(')'));
    ASSERT(!hwire_is_tchar(','));
    ASSERT(!hwire_is_tchar('/'));
    ASSERT(!hwire_is_tchar(':'));
    ASSERT(!hwire_is_tchar(';'));
    ASSERT(!hwire_is_tchar('<'));
    ASSERT(!hwire_is_tchar('='));
    ASSERT(!hwire_is_tchar('>'));
    ASSERT(!hwire_is_tchar('?'));
    ASSERT(!hwire_is_tchar('@'));
    ASSERT(!hwire_is_tchar('['));
    ASSERT(!hwire_is_tchar('\\'));
    ASSERT(!hwire_is_tchar(']'));
    ASSERT(!hwire_is_tchar('{'));
    ASSERT(!hwire_is_tchar('}'));
    ASSERT(!hwire_is_tchar(127)); // DEL  (0x7F): CTL, not tchar

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.6.2  token = 1*tchar
 * hwire_parse_tchar() scans a token from the input and returns its length.
 * Scanning stops at the first non-tchar character; pos advances past the token.
 */
void test_parse_tchar(void)
{
    TEST_START("test_parse_tchar");

    const char *str = "token::value";
    size_t len      = strlen(str);
    size_t pos      = 0;
    size_t n        = 0;

    // "token"
    n = hwire_parse_tchar(str, len, &pos);
    ASSERT_EQ(n, 5);
    ASSERT_EQ(pos, 5); // Points to ':'

    // ":" (not tchar)
    n = hwire_parse_tchar(str, len, &pos);
    ASSERT_EQ(n, 0);   // Stops immediately
    ASSERT_EQ(pos, 5); // Pos remains same

    // Skip "::" manually for testing continuation
    pos += 2;

    // "value"
    n = hwire_parse_tchar(str, len, &pos);
    ASSERT_EQ(n, 5);
    ASSERT_EQ(pos, 12); // End of string

    TEST_END();
}

int main(void)
{
    test_is_tchar();
    test_parse_tchar();
    print_test_summary();
    return g_tests_failed;
}
