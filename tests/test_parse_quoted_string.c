#define _GNU_SOURCE

#include "test_helpers.h"

#if defined(__APPLE__) || defined(__linux__)
# include <sys/mman.h>
# include <unistd.h>
#endif

#if defined(MAP_ANONYMOUS)
# define HWIRE_MAP_ANONYMOUS MAP_ANONYMOUS
#elif defined(MAP_ANON)
# define HWIRE_MAP_ANONYMOUS MAP_ANON
#endif

/*
 * Covers: RFC 9110 §5.6.4  quoted-string = DQUOTE *( qdtext / quoted-pair )
 * DQUOTE qdtext       = HTAB / SP / %x21 / %x23-5B / %x5D-7E / obs-text
 *                           quoted-pair  = "\" ( HTAB / SP / VCHAR / obs-text )
 *                           obs-text     = %x80-FF
 * MUST: pos MUST equal total consumed bytes (including both DQUOTE delimiters)
 * after HWIRE_OK.
 */
void test_parse_quoted_string_valid(void)
{
    TEST_START("test_parse_quoted_string_valid");

    size_t pos;
    int rv;
    const char *str;

    /* Basic quoted string */
    str = "\"quoted string\"";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(str)); /* includes both DQUOTE */

    /* RFC 9110 §5.6.4: quoted-pair allows escaping DQUOTE inside
       quoted-string */
    str = "\"quoted\\\"string\"";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(str));

    TEST_END();
}

/*
 * Covers: error cases for RFC 9110 §5.6.4 quoted-string parsing.
 * MUST reject: input not starting with DQUOTE → HWIRE_EILSEQ
 * MUST return HWIRE_EAGAIN: no closing DQUOTE yet (incomplete input)
 * MUST return HWIRE_EAGAIN: backslash at end of input (quoted-pair incomplete)
 * MUST return HWIRE_EAGAIN: pos >= len (no input remaining)
 * MUST reject: invalid quoted-pair target (CTL other than HTAB) → HWIRE_EILSEQ
 * MUST return HWIRE_ELEN: content exceeds maxlen
 */
void test_parse_quoted_string_invalid(void)
{
    TEST_START("test_parse_quoted_string_invalid");

    size_t pos;
    int rv;
    const char *str;

    /* MUST reject: missing opening DQUOTE → HWIRE_EILSEQ */
    str = "no quotes";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* MUST return HWIRE_EAGAIN: closing DQUOTE not yet received (incomplete) */
    str = "\"partial";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* RFC 9110 §5.6.4: quoted-pair = "\" ( HTAB / SP / VCHAR / obs-text );
     * 0x01 is not HTAB, SP, VCHAR, or obs-text → HWIRE_EILSEQ */
    str = "\"bad escape \\\x01\"";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* MUST return HWIRE_ELEN: total wire length (10) exceeds maxlen (5) */
    str = "\"too long\""; /* total wire = 10 bytes */
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 5);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* MUST return HWIRE_ELEN: total wire length (7) exceeds maxlen (4) */
    str = "\"hello\""; /* total wire = 7 bytes */
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 4);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* MUST return HWIRE_EAGAIN: pos >= len (no input remaining at start) */
    str = "abc";
    pos = 3;
    rv  = hwire_parse_quoted_string(str, 3, &pos, 100);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* MUST return HWIRE_EAGAIN: backslash at end of input (quoted-pair
       incomplete) */
    str = "\"escape \\";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.6.4  RFC-compliant content acceptance.
 * MUST accept: quoted-pair with HTAB, SP, DQUOTE, and VCHAR targets.
 * MUST accept: obs-text (0x80-0xFF) as qdtext inside quoted-string.
 * MUST accept: empty quoted-string ("").
 */
void test_parse_quoted_string_rfc_compliance(void)
{
    TEST_START("test_parse_quoted_string_rfc_compliance");

    size_t pos;
    int rv;
    const char *str;

    /* RFC 9110 §5.6.4: quoted-pair targets HTAB (\t), SP ( ), DQUOTE (\"),
       and ALPHA (A) */
    str = "\"quoted pair: \\t \\  \\\" \\A\"";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(str));

    /* RFC 9110 §5.6.4: obs-text = %x80-FF; multi-byte UTF-8 bytes are
       obs-text (MUST accept) */
    str = "\"UTF-8 text: "
          "\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf\"";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(str));

    /* RFC 9110 §5.6.4: empty quoted-string (DQUOTE DQUOTE) MUST be accepted */
    str = "\"\"";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 2);

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.6.4  rejection of invalid characters.
 * MUST reject: invalid quoted-pair target (CTL other than HTAB) → HWIRE_EILSEQ
 * MUST reject: CTL character (0x01-0x1F except HTAB) as qdtext → HWIRE_EILSEQ
 */
void test_parse_quoted_string_rfc_invalid(void)
{
    TEST_START("test_parse_quoted_string_rfc_invalid");

    size_t pos;
    int rv;
    const char *str;

    /* RFC 9110 §5.6.4: quoted-pair = "\" ( HTAB / SP / VCHAR / obs-text );
     * 0x01 is CTL (not HTAB, SP, VCHAR, or obs-text) → HWIRE_EILSEQ */
    str = "\"bad \\\x01\"";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* RFC 9110 §5.6.4: qdtext = HTAB / SP / %x21 / %x23-5B / %x5D-7E /
     * obs-text; 0x1F is CTL (not in qdtext) → HWIRE_EILSEQ */
    str = "\"cntrl \x1F\"";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 100);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    TEST_END();
}

/*
 * Covers: exact pos advancement for hwire_parse_quoted_string.
 * MUST: pos MUST equal the total byte count of the quoted-string including
 * both DQUOTE delimiters. MUST: a quoted-pair inside the string MUST count
 * as 2 bytes (backslash + target char) toward pos.
 * MUST: non-zero initial pos MUST be correctly offset.
 * MUST: maxlen is an absolute limit measured from str[0].
 *       A closing DQUOTE at maxlen - 1 MUST succeed.
 *       A quoted-string that would end after maxlen MUST return HWIRE_ELEN.
 */
void test_parse_quoted_string_content_verification(void)
{
    TEST_START("test_parse_quoted_string_content_verification");

    size_t pos;
    int rv;

    /* "hello" → 1 + 5 + 1 = 7 bytes → pos=7 */
    const char *str1 = "\"hello\"";
    pos              = 0;
    rv               = hwire_parse_quoted_string(str1, strlen(str1), &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 7);

    /* "he\"llo" with quoted-pair: " + h + e + \ + " + l + l + o + " = 9 bytes
     */
    const char *str2 = "\"he\\\"llo\"";
    pos              = 0;
    rv               = hwire_parse_quoted_string(str2, strlen(str2), &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 9);

    /* maxlen boundary: maxlen == total wire length (4) MUST return HWIRE_OK */
    const char *str3 = "\"ab\""; /* total wire = 4 bytes */
    pos              = 0;
    rv               = hwire_parse_quoted_string(str3, strlen(str3), &pos, 4);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 4);

    /* maxlen one short (3) for a 4-byte wire string MUST return HWIRE_ELEN */
    pos = 0;
    rv  = hwire_parse_quoted_string(str3, strlen(str3), &pos, 3);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* non-zero initial pos: x"ab" starting at pos=1 */
    const char *str4 = "x\"ab\"";
    pos              = 1;
    rv               = hwire_parse_quoted_string(str4, strlen(str4), &pos, 100);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 5); /* consumed 4 bytes from pos=1 */

    /* maxlen is absolute: the prefix at index 0 also consumes the budget */
    pos = 1;
    rv  = hwire_parse_quoted_string(str4, strlen(str4), &pos, 5);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 5);

    pos = 1;
    rv  = hwire_parse_quoted_string(str4, strlen(str4), &pos, 4);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, 4);

    /* Input ending before the absolute budget remains retryable. */
    pos = 1;
    rv  = hwire_parse_quoted_string(str4, 4, &pos, 5);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, 4);

    /* quoted-pair "\a" = 4 total wire bytes (`"`, `\`, `a`, `"`);
     * maxlen=4 MUST return HWIRE_OK */
    const char *str5 = "\"\\a\"";
    pos              = 0;
    rv               = hwire_parse_quoted_string(str5, strlen(str5), &pos, 4);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, 4);

    /* maxlen=3 for same 4-byte string MUST return HWIRE_ELEN */
    pos = 0;
    rv  = hwire_parse_quoted_string(str5, strlen(str5), &pos, 3);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* An incomplete string at the exact absolute budget is not retryable. */
    const char *str6 = "\"abc";
    pos              = 0;
    rv               = hwire_parse_quoted_string(str6, strlen(str6), &pos, 4);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, 4);

    pos = 0;
    rv  = hwire_parse_quoted_string(str6, strlen(str6), &pos, 5);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, 4);

    TEST_END();
}

void test_parse_quoted_string_numeric_boundaries(void)
{
    TEST_START("test_parse_quoted_string_numeric_boundaries");

    const char *str = "x\"ab\"";
    size_t pos      = 1;
    int rv = hwire_parse_quoted_string(str, strlen(str), &pos, SIZE_MAX);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(str));

    pos = 1;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 0);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, 1);

    pos = 1;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 1);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, 1);

    pos = SIZE_MAX;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, SIZE_MAX);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, SIZE_MAX);

    pos = strlen(str);
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 0);
    ASSERT_EQ(rv, HWIRE_EAGAIN);
    ASSERT_EQ(pos, strlen(str));

    TEST_END();
}

void test_parse_quoted_string_quoted_pair_budget(void)
{
    TEST_START("test_parse_quoted_string_quoted_pair_budget");

    const char *str = "\"\\a\"";
    size_t pos      = 0;
    int rv          = hwire_parse_quoted_string(str, strlen(str), &pos, 2);
    ASSERT_EQ(rv, HWIRE_ELEN);
    ASSERT_EQ(pos, 1);

#if defined(HWIRE_MAP_ANONYMOUS)
    long page_size = sysconf(_SC_PAGESIZE);
    ASSERT(page_size > 0);
    ASSERT((size_t)page_size <= SIZE_MAX / 2);

    size_t map_len        = (size_t)page_size * 2;
    unsigned char *region = mmap(NULL, map_len, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | HWIRE_MAP_ANONYMOUS, -1, 0);
    ASSERT(region != MAP_FAILED);
    ASSERT_EQ(
        mprotect(region + (size_t)page_size, (size_t)page_size, PROT_NONE), 0);

    unsigned char *buf = region + (size_t)page_size - 2;
    buf[0]             = '"';
    buf[1]             = '\\';
    pos                = 0;
    ASSERT_EQ(hwire_parse_quoted_string((const char *)buf, 3, &pos, 2),
              HWIRE_ELEN);
    ASSERT_EQ(pos, 1);

    /* str[maxlen] is protected: an initial position at the absolute budget
     * must return without examining that byte. */
    pos = 2;
    ASSERT_EQ(hwire_parse_quoted_string((const char *)buf, 3, &pos, 2),
              HWIRE_ELEN);
    ASSERT_EQ(pos, 2);

    ASSERT_EQ(mprotect(region + (size_t)page_size, (size_t)page_size,
                       PROT_READ | PROT_WRITE),
              0);
    ASSERT_EQ(munmap(region, map_len), 0);
#else
    fprintf(stdout, "[SKIP] anonymous mmap is unavailable\n");
#endif

    TEST_END();
}

int main(void)
{
    test_parse_quoted_string_valid();
    test_parse_quoted_string_invalid();
    test_parse_quoted_string_rfc_compliance();
    test_parse_quoted_string_rfc_invalid();
    test_parse_quoted_string_content_verification();
    test_parse_quoted_string_numeric_boundaries();
    test_parse_quoted_string_quoted_pair_budget();
    print_test_summary();
    return g_tests_failed;
}
