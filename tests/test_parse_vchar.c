#include "test_helpers.h"

void test_is_vchar(void) {
    TEST_START("test_is_vchar");

    // VCHAR: 0x21 - 0x7E
    for (int c = 0x21; c <= 0x7E; c++) {
        if (!hwire_is_vchar((unsigned char)c)) {
            fprintf(stderr, "Failed vchar check for: 0x%02X\n", c);
            g_tests_failed++;
        }
    }

    // obs-text: 0x80 - 0xFF
    for (int c = 0x80; c <= 0xFF; c++) {
        if (!hwire_is_vchar((unsigned char)c)) {
             fprintf(stderr, "Failed obs-text check for: 0x%02X\n", c);
             g_tests_failed++;
        }
    }

    // Invalid vchars
    ASSERT(!hwire_is_vchar(0x00)); // Null
    ASSERT(!hwire_is_vchar(0x20)); // SP
    ASSERT(!hwire_is_vchar(0x7F)); // DEL
    ASSERT(!hwire_is_vchar('\t')); // HT

    TEST_END();
}

void test_parse_vchar(void) {
    TEST_START("test_parse_vchar");

    // "Value\tKey"
    // VCHAR include everything except SP, HT, DEL, CTLs
    const char *str = "Value\tKey";
    size_t len = strlen(str);
    size_t pos = 0;
    size_t n = 0;

    // "Value"
    n = hwire_parse_vchar(str, len, &pos);
    ASSERT_EQ(n, 5);
    ASSERT_EQ(pos, 5); // Points to '\t'

    // "\t" (not vchar)
    n = hwire_parse_vchar(str, len, &pos);
    ASSERT_EQ(n, 0);
    ASSERT_EQ(pos, 5);

    TEST_END();
}

int main(void) {
    test_is_vchar();
    test_parse_vchar();
    print_test_summary();
    return g_tests_failed;
}
