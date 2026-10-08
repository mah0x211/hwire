#include "test_helpers.h"

static void test_empty_parameters(void)
{
    TEST_START("test_empty_parameters");

    static const struct {
        const char *input;
        size_t pairs;
    } cases[] = {
        {"",                       0},
        {";",                      0},
        {"; ",                     0},
        {"\t; \t",                 0},
        {";;",                     0},
        {"; \t; ;",                0},
        {";\r\n",                  0},
        {";\n",                    0},
        {"; \t\r\n",               0},
        {";a=b;",                  1},
        {";a=b;\r\n",              1},
        {";a=b; ;\r\n",            1},
        {";;a=b; ;c=d;;\r\n",      2},
        {";a=\"b\";\r\n",          1},
        {";a=b; \t\n",             1},
    };
    char buf[128];

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int n = snprintf(buf, sizeof(buf), "xx%s", cases[i].input);
        ASSERT(n >= 2);
        ASSERT((size_t)n < sizeof(buf));
        size_t len = (size_t)n;
        size_t end = 2 + strcspn(cases[i].input, "\r\n");

        for (int skip = 0; skip <= 1; skip++) {
            /* End of input is complete, even at the exact length budget. */
            for (size_t extra = 0; extra <= 1; extra++) {
                test_capacity_t storage = {.capacity = 2};
                hwire_ctx_t ctx = {
                    .uctx = &storage,
                    .param_cb = capacity_pair_cb,
                };
                size_t pos = 2;
                ASSERT_OK(hwire_parse_parameters(&ctx, buf, len, &pos,
                                                  len - 2 + extra, skip));
                ASSERT_EQ(pos, end);
                ASSERT_EQ(storage.count, cases[i].pairs);
                ASSERT_EQ(storage.calls, cases[i].pairs);
                for (size_t j = 0; j < storage.count; j++) {
                    ASSERT_EQ(storage.pairs[j].key.len, 1);
                    ASSERT_EQ(storage.pairs[j].value.len, 1);
                    ASSERT_EQ(storage.pairs[j].key.ptr[0], j == 0 ? 'a' : 'c');
                    ASSERT_EQ(storage.pairs[j].value.ptr[0], j == 0 ? 'b' : 'd');
                }
            }
        }
    }

    TEST_END();
}

static void test_empty_parameter_boundaries(void)
{
    TEST_START("test_empty_parameter_boundaries");

    const char *buf = "xx; \t; a=b; \t\r\n";
    size_t len = strlen(buf);
    size_t end = (size_t)(strchr(buf, '\r') - buf);

    for (int skip = 0; skip <= 1; skip++) {
        for (size_t budget = 0; budget <= end - 2; budget++) {
            test_capacity_t storage = {.capacity = 2};
            hwire_ctx_t ctx = {
                .uctx = &storage,
                .param_cb = capacity_pair_cb,
            };
            size_t pos = 2;
            ASSERT_EQ(hwire_parse_parameters(&ctx, buf, len, &pos, budget, skip),
                      HWIRE_ELEN);
            ASSERT(pos <= 2 + budget);
        }

        test_capacity_t storage = {.capacity = 2};
        hwire_ctx_t ctx = {
            .uctx = &storage,
            .param_cb = capacity_pair_cb,
        };
        size_t pos = 2;
        ASSERT_OK(hwire_parse_parameters(&ctx, buf, len, &pos, end - 1, skip));
        ASSERT_EQ(pos, end);
        ASSERT_EQ(storage.calls, 1);
    }

    TEST_END();
}

static void test_nonempty_parameter_errors(void)
{
    TEST_START("test_nonempty_parameter_errors");

    static const struct {
        const char *input;
        int expected;
    } cases[] = {
        {";@x",       HWIRE_EILSEQ},
        {";=b",       HWIRE_EILSEQ},
        {";a?",       HWIRE_EILSEQ},
        {";a=;",      HWIRE_EILSEQ},
        {";a=\r\n",   HWIRE_EILSEQ},
        {";;a",       HWIRE_EAGAIN},
        {";;a=",      HWIRE_EAGAIN},
        {";;a=\"b",   HWIRE_EAGAIN},
        {";;a=\"b\\", HWIRE_EAGAIN},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        for (int skip = 0; skip <= 1; skip++) {
            test_capacity_t storage = {.capacity = 2};
            hwire_ctx_t ctx = {
                .uctx = &storage,
                .param_cb = capacity_pair_cb,
            };
            size_t pos = 0;
            size_t len = strlen(cases[i].input);
            ASSERT_EQ(hwire_parse_parameters(&ctx, cases[i].input, len, &pos,
                                               len + 1, skip),
                      cases[i].expected);
            ASSERT_EQ(storage.calls, 0);
            if (cases[i].expected == HWIRE_EAGAIN) {
                pos = 0;
                ASSERT_EQ(hwire_parse_parameters(&ctx, cases[i].input, len,
                                                   &pos, len, skip),
                          HWIRE_ELEN);
                ASSERT_EQ(storage.calls, 0);
            }
        }
    }

    TEST_END();
}

int main(void)
{
    test_empty_parameters();
    test_empty_parameter_boundaries();
    test_nonempty_parameter_errors();
    print_test_summary();
    return g_tests_failed;
}
