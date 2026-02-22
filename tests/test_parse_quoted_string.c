#include "test_helpers.h"

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

    /* MUST return HWIRE_ELEN: content length exceeds maxlen parameter */
    str = "\"too long\"";
    pos = 0;
    rv  = hwire_parse_quoted_string(str, strlen(str), &pos, 5);
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

int main(void)
{
    test_parse_quoted_string_valid();
    test_parse_quoted_string_invalid();
    test_parse_quoted_string_rfc_compliance();
    test_parse_quoted_string_rfc_invalid();
    print_test_summary();
    return g_tests_failed;
}
