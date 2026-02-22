#include "test_helpers.h"

/*
 * Covers: RFC 5234 §B.1   VCHAR    = %x21-7E (visible printing characters)
 *         RFC 9110 §5.5   field-vchar = VCHAR / obs-text
 *                         obs-text    = %x80-FF
 * MUST: hwire_is_vchar() returns true for VCHAR (0x21-0x7E) and obs-text
 * (0x80-0xFF). MUST: returns false for NUL (0x00), SP (0x20), DEL (0x7F), HTAB
 * (0x09), and all CTLs.
 */
void test_is_vchar(void)
{
    TEST_START("test_is_vchar");

    // MUST accept: VCHAR = %x21-7E (RFC 5234 §B.1)
    for (int c = 0x21; c <= 0x7E; c++) {
        if (!hwire_is_vchar((unsigned char)c)) {
            fprintf(stderr, "Failed vchar check for: 0x%02X\n", c);
            g_tests_failed++;
        }
    }

    // MUST accept: obs-text = %x80-FF (RFC 9110 §5.5 field-vchar)
    for (int c = 0x80; c <= 0xFF; c++) {
        if (!hwire_is_vchar((unsigned char)c)) {
            fprintf(stderr, "Failed obs-text check for: 0x%02X\n", c);
            g_tests_failed++;
        }
    }

    // MUST reject: not field-vchar (RFC 9110 §5.5)
    ASSERT(!hwire_is_vchar(0x00)); // NUL  (0x00): CTL
    ASSERT(!hwire_is_vchar(0x20)); // SP   (0x20): not field-vchar
    ASSERT(!hwire_is_vchar(0x7F)); // DEL  (0x7F): CTL
    ASSERT(!hwire_is_vchar(
        '\t')); // HTAB (0x09): valid only as OWS, not field-vchar

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5  field-vchar = VCHAR / obs-text
 * hwire_parse_vchar() scans field-vchar characters and stops at the first
 * non-field-vchar character (SP, HTAB, CTL, DEL, NUL); pos advances past
 * consumed chars. Note: HTAB terminates scanning — it is valid as OWS in
 * field-value, not as field-vchar.
 */
void test_parse_vchar(void)
{
    TEST_START("test_parse_vchar");

    // "Value\tKey": HTAB (0x09) terminates field-vchar scanning (HTAB is OWS,
    // not field-vchar)
    const char *str = "Value\tKey";
    size_t len      = strlen(str);
    size_t pos      = 0;
    size_t n        = 0;

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

int main(void)
{
    test_is_vchar();
    test_parse_vchar();
    print_test_summary();
    return g_tests_failed;
}
