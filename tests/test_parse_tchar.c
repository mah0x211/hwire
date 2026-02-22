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

/*
 * Covers: RFC 9110 §5.6.2  tchar ABNF exhaustive verification
 * MUST: hwire_is_tchar() MUST return true for exactly the 76 tchar bytes
 * defined by RFC 9110 §5.6.2 and false for all other 180 bytes (0x00-0xFF).
 *   Valid tchar: ALPHA (a-z, A-Z), DIGIT (0-9),
 *     '!' '#' '$' '%' '&' '\'' '*' '+' '-' '.' '^' '_' '`' '|' '~'
 */
void test_is_tchar_all256(void)
{
    TEST_START("test_is_tchar_all256");

    for (int c = 0; c <= 0xFF; c++) {
        int is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        int is_digit = (c >= '0' && c <= '9');
        int is_symbol =
            (c == '!' || c == '#' || c == '$' || c == '%' || c == '&' ||
             c == '\'' || c == '*' || c == '+' || c == '-' || c == '.' ||
             c == '^' || c == '_' || c == '`' || c == '|' || c == '~');
        int expected = is_alpha || is_digit || is_symbol;
        int actual   = hwire_is_tchar((unsigned char)c);
        if ((actual != 0) != expected) {
            fprintf(stderr,
                    "FAILED: %s:%d: hwire_is_tchar(0x%02X): expected %d, "
                    "got %d\n",
                    __FILE__, __LINE__, (unsigned char)c, expected,
                    actual != 0);
            g_tests_failed++;
        }
    }

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.6.2  hwire_parse_tchar() SIMD boundary behaviour
 * MUST: tokens of exactly 15/16/17/31/32/33 tchar characters MUST be fully
 * consumed.  '|' (0x7C) and '~' (0x7E) — the two highest-valued tchar bytes —
 * MUST be accepted at every position including SIMD chunk boundaries.
 * A non-tchar byte MUST terminate scanning at the correct position.
 */
void test_parse_tchar_simd_boundary(void)
{
    TEST_START("test_parse_tchar_simd_boundary");

    /* All-tchar string; the +1 byte is '@' (non-tchar) to act as a stop */
    char str[35];
    memset(str, 'a', 34);
    str[33] = '@'; /* non-tchar sentinel */

    /* Boundary lengths: 15, 16, 17, 31, 32, 33 */
    static const size_t lens[] = {15, 16, 17, 31, 32, 33};
    for (size_t i = 0; i < 6; i++) {
        size_t pos = 0;
        size_t n   = hwire_parse_tchar(str, lens[i], &pos);
        if (n != lens[i] || pos != lens[i]) {
            fprintf(stderr,
                    "FAILED: %s:%d: len=%zu: n=%zu pos=%zu (expected both "
                    "%zu)\n",
                    __FILE__, __LINE__, lens[i], n, pos, lens[i]);
            g_tests_failed++;
            return;
        }
    }

    /* '|' and '~' at SIMD boundary positions (bytes 15 and 16 of a 33-byte
     * token) */
    char bstr[34];
    memset(bstr, 'a', 33);
    bstr[15] = '|'; /* last byte of first 16-byte SIMD chunk */
    bstr[16] = '~'; /* first byte of second chunk */

    size_t pos = 0;
    size_t n   = hwire_parse_tchar(bstr, 33, &pos);
    ASSERT_EQ(n, 33);
    ASSERT_EQ(pos, 33);

    /* Non-tchar at byte 15 MUST stop scanning at exactly position 15 */
    bstr[15] = '@';
    pos      = 0;
    n        = hwire_parse_tchar(bstr, 33, &pos);
    ASSERT_EQ(n, 15);
    ASSERT_EQ(pos, 15);

    TEST_END();
}

int main(void)
{
    test_is_tchar();
    test_parse_tchar();
    test_is_tchar_all256();
    test_parse_tchar_simd_boundary();
    print_test_summary();
    return g_tests_failed;
}
