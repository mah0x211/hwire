#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include "hwire.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Common buffer sizes */
#define TEST_BUF_SIZE 256
#define TEST_KEY_SIZE 64

/* Test counters */
extern int g_tests_run;
extern int g_tests_passed;
extern int g_tests_failed;

/* Assertion macros */
#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAILED: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_EQ(actual, expected)                                            \
    do {                                                                       \
        long long _a = (long long)(actual);                                    \
        long long _e = (long long)(expected);                                  \
        if (_a != _e) {                                                        \
            fprintf(stderr, "FAILED: %s:%d: expected %lld, got %lld\n",        \
                    __FILE__, __LINE__, _e, _a);                               \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_OK(rv) ASSERT_EQ(rv, HWIRE_OK)

#define TEST_START(name)                                                       \
    do {                                                                       \
        fprintf(stdout, "[RUN] %s\n", name);                                   \
        g_tests_run++;                                                         \
    } while (0)

#define TEST_END()                                                             \
    do {                                                                       \
        if (g_tests_failed == 0) {                                             \
            g_tests_passed++;                                                  \
            fprintf(stdout, "[PASS]\n");                                       \
        } else {                                                               \
            fprintf(stdout, "[FAIL]\n");                                       \
        }                                                                      \
    } while (0)

/* Common mock callback functions (success) */
int mock_header_cb(hwire_ctx_t *ctx, hwire_header_t *header);
int mock_request_cb(hwire_ctx_t *ctx, hwire_request_t *req);
int mock_response_cb(hwire_ctx_t *ctx, hwire_response_t *rsp);
int mock_param_cb(hwire_ctx_t *ctx, hwire_param_t *param);
int mock_chunksize_cb(hwire_ctx_t *ctx, uint32_t size);
int mock_chunksize_ext_cb(hwire_ctx_t *ctx, hwire_chunksize_ext_t *ext);

/* Common mock callback functions (failure) */
int mock_header_cb_fail(hwire_ctx_t *ctx, hwire_header_t *header);
int mock_request_cb_fail(hwire_ctx_t *ctx, hwire_request_t *req);
int mock_response_cb_fail(hwire_ctx_t *ctx, hwire_response_t *rsp);
int mock_param_cb_fail(hwire_ctx_t *ctx, hwire_param_t *param);
int mock_chunksize_cb_fail(hwire_ctx_t *ctx, uint32_t size);
int mock_chunksize_ext_cb_fail(hwire_ctx_t *ctx, hwire_chunksize_ext_t *ext);

/* Verify that a hwire_str_t slice is within [buf, buf+buf_len). */
static inline int str_in_buf(hwire_str_t s, const char *buf, size_t buf_len)
{
    if (s.len == 0)
        return 1; /* empty slice: ptr irrelevant */
    return s.ptr >= buf && s.ptr + s.len <= buf + buf_len;
}

/* Helper to print summary */
void print_test_summary(void);

#endif /* TEST_HELPERS_H */
