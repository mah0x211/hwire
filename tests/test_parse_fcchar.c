#include "test_helpers.h"

/*
 * Covers: RFC 9110 §5.5
 *   field-content = field-vchar [ 1*( SP / HTAB / field-vchar ) field-vchar ]
 *   field-vchar   = VCHAR / obs-text
 *   VCHAR         = %x21-7E
 *   obs-text      = %x80-FF
 *   SP            = %x20
 *   HTAB          = %x09
 * MUST: hwire_is_fcchar() returns true for VCHAR (0x21-0x7E), obs-text
 * (0x80-0xFF), SP (0x20), and HTAB (0x09).
 * MUST: returns false for NUL (0x00), DEL (0x7F), CR (0x0D), LF (0x0A),
 * and all other CTLs (0x01-0x08, 0x0B-0x0C, 0x0E-0x1F).
 */
void test_is_fcchar(void)
{
    TEST_START("test_is_fcchar");

    /* MUST accept: SP and HTAB (OWS within field-value) */
    ASSERT(hwire_is_fcchar(0x20)); /* SP   */
    ASSERT(hwire_is_fcchar(0x09)); /* HTAB */

    /* MUST accept: VCHAR = %x21-7E */
    ASSERT(hwire_is_fcchar('!'));
    ASSERT(hwire_is_fcchar('a'));
    ASSERT(hwire_is_fcchar('z'));
    ASSERT(hwire_is_fcchar('A'));
    ASSERT(hwire_is_fcchar('Z'));
    ASSERT(hwire_is_fcchar('0'));
    ASSERT(hwire_is_fcchar('9'));
    ASSERT(hwire_is_fcchar('~')); /* highest VCHAR: 0x7E */

    /* MUST accept: obs-text = %x80-FF */
    ASSERT(hwire_is_fcchar(0x80));
    ASSERT(hwire_is_fcchar(0xFF));

    /* MUST reject: NUL (0x00) */
    ASSERT(!hwire_is_fcchar(0x00));
    /* MUST reject: DEL (0x7F) */
    ASSERT(!hwire_is_fcchar(0x7F));
    /* MUST reject: CR (0x0D) and LF (0x0A) — line terminators */
    ASSERT(!hwire_is_fcchar(0x0D));
    ASSERT(!hwire_is_fcchar(0x0A));
    /* MUST reject: other CTLs */
    ASSERT(!hwire_is_fcchar(0x01));
    ASSERT(!hwire_is_fcchar(0x1F));

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5  exhaustive verification over all 256 byte values.
 * MUST: hwire_is_fcchar() returns true for exactly SP (0x20), HTAB (0x09),
 * VCHAR (0x21-0x7E), and obs-text (0x80-0xFF) — 224 bytes in total.
 */
void test_is_fcchar_all256(void)
{
    TEST_START("test_is_fcchar_all256");

    for (int c = 0; c <= 0xFF; c++) {
        int is_sp      = (c == 0x20);
        int is_ht      = (c == 0x09);
        int is_vchar   = (c >= 0x21 && c <= 0x7E);
        int is_obstext = (c >= 0x80 && c <= 0xFF);
        int expected   = is_sp || is_ht || is_vchar || is_obstext;
        int actual     = hwire_is_fcchar((unsigned char)c);
        if ((actual != 0) != expected) {
            fprintf(stderr,
                    "FAILED: %s:%d: hwire_is_fcchar(0x%02X): expected %d, "
                    "got %d\n",
                    __FILE__, __LINE__, (unsigned char)c, expected,
                    actual != 0);
            g_tests_failed++;
        }
    }

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5  basic scanning behaviour.
 * hwire_parse_fcchar() scans fcchar characters and stops at the first
 * non-fcchar character; pos advances past consumed chars.
 * CR (0x0D) and LF (0x0A) terminate scanning.
 */
void test_parse_fcchar_basic(void)
{
    TEST_START("test_parse_fcchar_basic");

    /* Stops at CR */
    const char *s1 = "value\r\n";
    size_t len1    = strlen(s1);
    size_t pos     = 0;
    size_t n       = hwire_parse_fcchar(s1, len1, &pos);
    ASSERT_EQ(n, 5);
    ASSERT_EQ(pos, 5); /* points to '\r' */

    /* Stops at LF (bare LF) */
    const char *s2 = "value\n";
    size_t len2    = strlen(s2);
    pos            = 0;
    n              = hwire_parse_fcchar(s2, len2, &pos);
    ASSERT_EQ(n, 5);
    ASSERT_EQ(pos, 5); /* points to '\n' */

    /* Non-fcchar at start returns 0, pos unchanged */
    const char *s3 = "\r\n";
    pos            = 0;
    n              = hwire_parse_fcchar(s3, strlen(s3), &pos);
    ASSERT_EQ(n, 0);
    ASSERT_EQ(pos, 0);

    /* Whole string consumed when no terminator present */
    const char *s4 = "abc";
    pos            = 0;
    n              = hwire_parse_fcchar(s4, strlen(s4), &pos);
    ASSERT_EQ(n, 3);
    ASSERT_EQ(pos, 3);

    TEST_END();
}

void test_parse_fcchar_offset_boundaries(void)
{
    TEST_START("test_parse_fcchar_offset_boundaries");

    const char *str = "abc";
    const size_t len = 3;
    const size_t offsets[] = {len, len + 1, SIZE_MAX};

    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        size_t pos = offsets[i];
        size_t n   = hwire_parse_fcchar(str, len, &pos);
        ASSERT_EQ(n, 0);
        ASSERT_EQ(pos, offsets[i]);
    }

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5  SP and HTAB are valid inside field-value.
 * hwire_parse_fcchar() MUST NOT stop at SP (0x20) or HTAB (0x09); it stops
 * only at CR, LF, NUL, DEL, or other CTLs.
 */
void test_parse_fcchar_whitespace(void)
{
    TEST_START("test_parse_fcchar_whitespace");

    /* SP in the middle is consumed */
    const char *s1 = "text/html; charset=utf-8\r\n";
    size_t len1    = strlen(s1);
    size_t pos     = 0;
    size_t n       = hwire_parse_fcchar(s1, len1, &pos);
    ASSERT_EQ(n, 24); /* everything up to '\r' */
    ASSERT_EQ(pos, 24);

    /* HTAB in the middle is consumed */
    const char *s2 = "value\twith\ttabs\r\n";
    size_t len2    = strlen(s2);
    pos            = 0;
    n              = hwire_parse_fcchar(s2, len2, &pos);
    ASSERT_EQ(n, 15); /* "value\twith\ttabs" */
    ASSERT_EQ(pos, 15);

    /* Leading SP is consumed (unlike hwire_parse_vchar which stops at SP) */
    const char *s3 = " leading\r\n";
    pos            = 0;
    n              = hwire_parse_fcchar(s3, strlen(s3), &pos);
    ASSERT_EQ(n, 8); /* " leading" */
    ASSERT_EQ(pos, 8);

    /* hwire_parse_vchar stops at leading SP; hwire_parse_fcchar does not */
    pos = 0;
    n   = hwire_parse_vchar(s3, strlen(s3), &pos);
    ASSERT_EQ(n, 0);
    ASSERT_EQ(pos, 0);

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.5  SIMD chunk boundary behaviour.
 * MUST: runs of exactly 15/16/17/31/32/33 fcchar characters MUST be fully
 * consumed.  obs-text bytes and SP/HTAB at SIMD chunk boundaries (positions
 * 15 and 16) MUST be accepted.
 * A non-fcchar byte MUST terminate scanning at the correct position.
 */
void test_parse_fcchar_simd_boundary(void)
{
    TEST_START("test_parse_fcchar_simd_boundary");

    /* All-fcchar string; sentinel '\r' (non-fcchar) at position 33 */
    char str[35];
    memset(str, 'a', 34);
    str[33] = '\r';

    static const size_t lens[] = {15, 16, 17, 31, 32, 33};
    for (size_t i = 0; i < 6; i++) {
        size_t pos = 0;
        size_t n   = hwire_parse_fcchar(str, lens[i], &pos);
        if (n != lens[i] || pos != lens[i]) {
            fprintf(stderr,
                    "FAILED: %s:%d: len=%zu: n=%zu pos=%zu (expected both "
                    "%zu)\n",
                    __FILE__, __LINE__, lens[i], n, pos, lens[i]);
            g_tests_failed++;
            return;
        }
    }

    /* SP and obs-text at SIMD boundary positions (bytes 15 and 16) */
    char bstr[34];
    memset(bstr, 'a', 33);
    bstr[15] = ' ';    /* SP at end of first 16-byte SIMD chunk */
    bstr[16] = '\x80'; /* obs-text at start of second chunk */

    size_t pos = 0;
    size_t n   = hwire_parse_fcchar(bstr, 33, &pos);
    ASSERT_EQ(n, 33);
    ASSERT_EQ(pos, 33);

    /* Non-fcchar (LF) at byte 15 MUST stop scanning at exactly position 15 */
    bstr[15] = '\n';
    pos      = 0;
    n        = hwire_parse_fcchar(bstr, 33, &pos);
    ASSERT_EQ(n, 15);
    ASSERT_EQ(pos, 15);

    TEST_END();
}

int main(void)
{
    test_is_fcchar();
    test_is_fcchar_all256();
    test_parse_fcchar_basic();
    test_parse_fcchar_offset_boundaries();
    test_parse_fcchar_whitespace();
    test_parse_fcchar_simd_boundary();
    print_test_summary();
    return g_tests_failed;
}
