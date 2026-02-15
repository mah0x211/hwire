#include "test_helpers.h"

void test_is_tchar(void) {
    TEST_START("test_is_tchar");

    // Valid tchars
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
    ASSERT(hwire_is_tchar('|'));
    ASSERT(hwire_is_tchar('~'));

    // Invalid tchars
    ASSERT(!hwire_is_tchar(0));   // Null
    ASSERT(!hwire_is_tchar(' ')); // SP
    ASSERT(!hwire_is_tchar('\t'));// HT
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
    ASSERT(!hwire_is_tchar(127)); // DEL

    TEST_END();
}

void test_parse_tchar(void) {
    TEST_START("test_parse_tchar");

    const char *str = "token::value";
    size_t len = strlen(str);
    size_t pos = 0;
    size_t n = 0;

    // "token"
    n = hwire_parse_tchar(str, len, &pos);
    ASSERT_EQ(n, 5);
    ASSERT_EQ(pos, 5); // Points to ':'

    // ":" (not tchar)
    n = hwire_parse_tchar(str, len, &pos);
    ASSERT_EQ(n, 0); // Stops immediately
    ASSERT_EQ(pos, 5); // Pos remains same

    // Skip "::" manually for testing continuation
    pos += 2;

    // "value"
    n = hwire_parse_tchar(str, len, &pos);
    ASSERT_EQ(n, 5);
    ASSERT_EQ(pos, 12); // End of string

    TEST_END();
}

int main(void) {
    test_is_tchar();
    test_parse_tchar();
    print_test_summary();
    return g_tests_failed;
}
