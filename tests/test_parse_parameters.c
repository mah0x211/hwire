#include "test_helpers.h"

/*
 * Covers: RFC 9110 §5.6.6  parameters = *( OWS ";" OWS [ parameter ] )
 *                           parameter       = parameter-name "="
 * parameter-value parameter-name  = token = 1*tchar parameter-value = ( token /
 * quoted-string ) The skip_semicolon flag (1) allows bare "name=value" without
 * a leading ";". MUST accept: parameters with token or quoted-string values.
 * MUST: pos MUST equal the total consumed bytes after HWIRE_OK.
 */
void test_parse_parameters_valid(void)
{
    TEST_START("test_parse_parameters_valid");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc   = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .param_cb = mock_param_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Single parameter with token value */
    buf = "; key=value ";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* Multiple parameters: second with quoted-string value */
    buf = "; k1=v1; k2=\"quoted\" ";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* skip_semicolon=1: bare "key=value" without leading ";" MUST be accepted
     */
    buf = "key=value ";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 1, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    TEST_END();
}

/*
 * Covers: error cases for RFC 9110 §5.6.6 parameter parsing.
 * MUST reject: missing "=" after parameter-name → HWIRE_EILSEQ
 * MUST reject: empty parameter-value (token must be 1*tchar) → HWIRE_EILSEQ
 * MUST return HWIRE_ENOBUFS if max parameter count is exceeded.
 * MUST return HWIRE_ECALLBACK if param_cb returns non-zero.
 * MUST return HWIRE_ELEN if OWS exceeds maxlen.
 * MUST return HWIRE_EKEYLEN if key length exceeds key_lc.size.
 */
void test_parse_parameters_invalid(void)
{
    TEST_START("test_parse_parameters_invalid");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc   = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .param_cb = mock_param_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* RFC 9110 §5.6.6: parameter requires "="; '?' in place of "=" →
       HWIRE_EILSEQ */
    buf = "; key?";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    { /* No key_lc buffer: hwire MUST still parse successfully (lowercase
         conversion skipped) */
        hwire_callbacks_t cb_no_lc = cb;
        cb_no_lc.key_lc.size       = 0;
        buf                        = "; KEY=Val ";
        pos                        = 0;
        rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0,
                                    &cb_no_lc);
        ASSERT_OK(rv);
    }

    { /* MUST return HWIRE_ECALLBACK if param_cb returns non-zero (quoted-string
         value) */
        hwire_callbacks_t cb_fail = cb;
        cb_fail.param_cb          = mock_param_cb_fail;
        buf                       = "; k=\"v\"";
        pos                       = 0;
        rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0,
                                    &cb_fail);
        ASSERT_EQ(rv, HWIRE_ECALLBACK);
    }

    /* RFC 9110 §5.6.6: parameter-value = token / quoted-string; token =
     * 1*tchar. Empty token value (';' immediately after '=') → HWIRE_EILSEQ */
    buf = "; k=;";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* OWS before first ";" exceeds maxlen → HWIRE_ELEN */
    buf = "   ;";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 1, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* OWS after ";" exceeds maxlen → HWIRE_ELEN */
    buf = ";   k=v";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 2, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* Valid parameter (sanity check) */
    buf = "; k=v";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_OK(rv);

    /* MUST return HWIRE_ENOBUFS if max parameter count is exceeded */
    buf = "; k1=v1; k2=v2";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 1, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ENOBUFS);

    { /* MUST return HWIRE_ECALLBACK if param_cb returns non-zero (token value)
       */
        hwire_callbacks_t cb_fail = cb;
        cb_fail.param_cb          = mock_param_cb_fail;
        buf                       = "; k1=v1 ";
        pos                       = 0;
        rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0,
                                    &cb_fail);
        ASSERT_EQ(rv, HWIRE_ECALLBACK);
    }

    { /* hwire: parameter-name key_lc.size exceeded → HWIRE_EKEYLEN */
        char small_key[3];
        hwire_callbacks_t cb_small = cb;
        cb_small.key_lc.buf        = small_key;
        cb_small.key_lc.size       = sizeof(small_key);
        buf                        = "; longkey=val";
        pos                        = 0;
        rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0,
                                    &cb_small);
        ASSERT_EQ(rv, HWIRE_EKEYLEN);
    }

    TEST_END();
}

/*
 * Covers: edge cases in RFC 9110 §5.6.6 parameter parsing.
 * MUST return HWIRE_EAGAIN if input ends at ";" with no parameter following.
 * MUST return HWIRE_EAGAIN if input ends immediately after "=" (no value yet).
 * MUST return HWIRE_ELEN if parameter-name pushes position past maxlen.
 * MUST return HWIRE_OK and pos==6 when buffer ends in trailing OWS
 *   (CHECK_NEXT_PARAM bounds check: ustr[cur] MUST NOT be read when cur >= len;
 *   without the fix a phantom ';' at buf[len] causes extra parameters to be
 *   parsed beyond the declared length).
 * MUST return HWIRE_OK and pos==3 when CHECK_PARAM sees cur==len after OWS
 *   following a ';' (ustr[cur] MUST NOT be read when cur == len; without the
 *   fix the phantom ';' at buf[len] causes strtchar to run with SIZE_MAX
 *   length).
 */
void test_parse_parameters_edge_cases(void)
{
    TEST_START("test_parse_parameters_edge_cases");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc   = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .param_cb = mock_param_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* Input ends at ";" with no parameter following → HWIRE_EAGAIN */
    buf = ";";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* Parameter value exceeds maxlen → HWIRE_ELEN */
    buf = "; key=value";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 1, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* Input ends immediately after "=" — key parsed but value not yet present
       → HWIRE_EAGAIN */
    buf = "; k=";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* parameter-name runs up to maxpos boundary (key "ab" pushes cur to
       maxpos=4) → HWIRE_ELEN */
    buf = "; ab=val";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 4, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* OOB fix: CHECK_NEXT_PARAM must not read ustr[cur] when cur >= len.
       Use a 16-byte string but declare only 6 bytes available so buf[6]=';'
       (the phantom semicolon) is outside the declared buffer.  Without the
       fix the parser reads buf[6]=';', then processes "phantom=x" past len=6
       causing pos != 6.  With the fix cur >= len is detected first and
       HWIRE_OK is returned with pos==6. */
    {
        char oob1[] = "; k=v ;phantom=x";
        pos         = 0;
        rv          = hwire_parse_parameters(oob1, 6, &pos, 100, 10, 0, &cb);
        ASSERT_EQ(rv, HWIRE_OK);
        ASSERT_EQ(pos, 6);
    }

    /* OOB fix: CHECK_PARAM must not read ustr[cur] when cur == len.
       Use ";; ;x=y" but declare only 3 bytes (";; ") so buf[3]=';' is outside
       the declared buffer.  Without the fix the parser reads buf[3]=';', then
       tries to parse "x=y" past len=3 causing a size_t underflow in strtchar
       (effectively SIZE_MAX length).  With the fix cur==len is detected first
       and HWIRE_OK is returned with pos==3. */
    {
        char oob2[] = ";; ;x=y"; /* buf[3]=';', not NUL-terminated at len=3 */
        pos         = 0;
        rv          = hwire_parse_parameters(oob2, 3, &pos, 100, 10, 0, &cb);
        ASSERT_EQ(rv, HWIRE_OK);
        ASSERT_EQ(pos, 3);
    }

    TEST_END();
}

/*
 * Covers: RFC 9110 §5.6.6  parameters = *( OWS ";" OWS [ parameter ] )
 * The ABNF allows empty parameter slots (consecutive ";;").
 * MUST: empty parameter slots (between consecutive ";") MUST be silently
 * skipped. MUST: parameter-name = token = 1*tchar; empty name (';' "=" value)
 * MUST reject.
 */
void test_parse_parameters_rfc_compliance(void)
{
    TEST_START("test_parse_parameters_rfc_compliance");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc   = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .param_cb = mock_param_cb
    };
    size_t pos;
    int rv;
    const char *buf;

    /* RFC 9110 §5.6.6: empty parameter slot ";;" MUST be skipped */
    buf = ";; key=value";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 1024, 10, 0, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* RFC 9110 §5.6.6: trailing empty parameter slots MUST be skipped */
    buf = "; key=value;; ";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 1024, 10, 0, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* RFC 9110 §5.6.6: input consisting only of empty parameter slots MUST be
       accepted */
    buf = ";;;; ";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 1024, 10, 0, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, strlen(buf));

    /* RFC 9110 §5.6.6: parameter-name = token = 1*tchar; empty name →
       HWIRE_EILSEQ */
    buf = "; =value";
    pos = 0;
    rv  = hwire_parse_parameters(buf, strlen(buf), &pos, 1024, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    TEST_END();
}

static int verify_param_content_cb(hwire_callbacks_t *cb, hwire_param_t *param)
{
    (void)cb;
    if (strncmp(param->key.ptr, "key", param->key.len) == 0 &&
        param->key.len == 3) {
        if (strncmp(param->value.ptr, "value", param->value.len) == 0 &&
            param->value.len == 5) {
            return 0;
        }
    }
    return 1; // Validation failed
}

/*
 * Covers: exact content verification of parsed parameter-name and
 * parameter-value. MUST: key.ptr and key.len MUST point to the original input
 * bytes. MUST: value.ptr and value.len MUST point to the value token bytes.
 */
void test_parse_parameters_content_verification(void)
{
    TEST_START("test_parse_parameters_content_verification");

    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc   = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .param_cb = verify_param_content_cb
    };
    size_t pos      = 0;
    const char *buf = "; key=value";
    int rv = hwire_parse_parameters(buf, strlen(buf), &pos, 1024, 10, 0, &cb);
    ASSERT_OK(rv);

    TEST_END();
}

int main(void)
{
    test_parse_parameters_valid();
    test_parse_parameters_invalid();
    test_parse_parameters_edge_cases();
    test_parse_parameters_rfc_compliance();
    test_parse_parameters_content_verification();
    print_test_summary();
    return g_tests_failed;
}
