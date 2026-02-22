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

/*
 * Covers: RFC 9110 §5.5  field-vchar = VCHAR / obs-text
 *                        obs-text    = %x80-FF
 * MUST: hwire_parse_vchar() MUST consume obs-text bytes (0x80-0xFF) as
 * field-vchar.  MUST: tokens containing obs-text at SIMD chunk boundaries
 * (positions 15, 16, 17, 31, 32) MUST be fully consumed.
 */
void test_parse_vchar_obstext_and_boundary(void)
{
    TEST_START("test_parse_vchar_obstext_and_boundary");

    /* Pure obs-text bytes */
    const char *obs = "\x80\x95\xff"; /* 3 obs-text bytes */
    size_t pos      = 0;
    size_t n        = hwire_parse_vchar(obs, 3, &pos);
    ASSERT_EQ(n, 3);
    ASSERT_EQ(pos, 3);

    /* obs-text mixed with VCHAR */
    const char *mixed = "abc\x80xyz\xff"; /* 8 bytes, all field-vchar */
    pos               = 0;
    n                 = hwire_parse_vchar(mixed, 8, &pos);
    ASSERT_EQ(n, 8);
    ASSERT_EQ(pos, 8);

    /* SIMD boundary: 33-byte string with obs-text at boundary positions 15 and
     * 16 */
    char buf[34];
    memset(buf, 'a', 33);
    buf[15] = '\x80'; /* obs-text at end of first SIMD chunk */
    buf[16] = '\xff'; /* obs-text at start of second SIMD chunk */

    pos = 0;
    n   = hwire_parse_vchar(buf, 33, &pos);
    ASSERT_EQ(n, 33);
    ASSERT_EQ(pos, 33);

    /* MUST stop at SP (0x20) even after obs-text bytes */
    buf[20] = ' ';
    pos     = 0;
    n       = hwire_parse_vchar(buf, 33, &pos);
    ASSERT_EQ(n, 20); /* stops at SP */
    ASSERT_EQ(pos, 20);

    TEST_END();
}

int main(void)
{
    test_is_vchar();
    test_parse_vchar();
    test_parse_vchar_obstext_and_boundary();
    print_test_summary();
    return g_tests_failed;
}
