#include "test_helpers.h"

void test_parse_parameters_valid(void) {
    TEST_START("test_parse_parameters_valid");

    char buf[TEST_BUF_SIZE];
    size_t len = strlen(buf);
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .param_cb = mock_param_cb
    };

    strcpy(buf, "; key=value ");
    len = strlen(buf);
    pos = 0;
    int rv = hwire_parse_parameters(buf, len, &pos, 100, 10, 0, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, len);

    /* multiple parameters */
    strcpy(buf, "; k1=v1; k2=\"quoted\" ");
    len = strlen(buf);
    pos = 0;
    rv = hwire_parse_parameters(buf, len, &pos, 100, 10, 0, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, len);

    /* skip leading semicolon */
    strcpy(buf, "key=value ");
    len = strlen(buf);
    pos = 0;
    rv = hwire_parse_parameters(buf, len, &pos, 100, 10, 1, &cb);
    ASSERT_OK(rv);
    ASSERT_EQ(pos, len);

    TEST_END();
}

void test_parse_parameters_invalid(void) {
    TEST_START("test_parse_parameters_invalid");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .param_cb = mock_param_cb
    };

    /* Missing = (invalid char instead of =) */
    strcpy(buf, "; key?");
    int rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* No key_lc buffer */
    hwire_callbacks_t cb_no_lc = cb;
    cb_no_lc.key_lc.size = 0;
    strcpy(buf, "; KEY=Val ");
    pos = 0;
    rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb_no_lc);
    ASSERT_OK(rv);

    /* Callback fail during quoted string parse */
    cb.param_cb = mock_param_cb_fail;
    strcpy(buf, "; k=\"v\"");
    pos = 0;
    rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    /* Empty token value -> EILSEQ */
    cb.param_cb = mock_param_cb;
    strcpy(buf, "; k=;");
    pos = 0;
    rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_EILSEQ);

    /* Initial skip_ws failure */
    strcpy(buf, "   ;");
    pos = 0;
    rv = hwire_parse_parameters(buf, strlen(buf), &pos, 1, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* skip_ws failure after semicolon */
    strcpy(buf, ";   k=v");
    pos = 0;
    rv = hwire_parse_parameters(buf, strlen(buf), &pos, 2, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    /* Valid parameter */
    strcpy(buf, "; k=v");
    pos = 0;
    rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_OK(rv);

    /* Max params exceeded */
    strcpy(buf, "; k1=v1; k2=v2");
    pos = 0;
    rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 1, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ENOBUFS);

    /* Callback error */
    cb.param_cb = mock_param_cb_fail;
    strcpy(buf, "; k1=v1 ");
    pos = 0;
    rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ECALLBACK);

    TEST_END();
}

void test_parse_parameters_edge_cases(void) {
    TEST_START("test_parse_parameters_edge_cases");

    char buf[TEST_BUF_SIZE];
    size_t pos = 0;
    char key_storage[TEST_KEY_SIZE];
    hwire_callbacks_t cb = {
        .key_lc = { .buf = key_storage, .size = sizeof(key_storage), .len = 0 },
        .param_cb = mock_param_cb
    };

    /* EAGAIN after semicolon */
    strcpy(buf, ";");
    pos = 0;
    int rv = hwire_parse_parameters(buf, strlen(buf), &pos, 100, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_EAGAIN);

    /* ELEN after semicolon */
    strcpy(buf, "; key=value");
    pos = 0;
    rv = hwire_parse_parameters(buf, strlen(buf), &pos, 1, 10, 0, &cb);
    ASSERT_EQ(rv, HWIRE_ELEN);

    TEST_END();
}

int main(void) {
    test_parse_parameters_valid();
    test_parse_parameters_invalid();
    test_parse_parameters_edge_cases();
    print_test_summary();
    return g_tests_failed;
}
