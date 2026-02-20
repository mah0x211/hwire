#include "hwire.h"
#include "inputs.h"
#include <catch2/catch_all.hpp>
#include <stdio.h>
#include <string.h>

#define MAX_KEY_LEN 256

static int dummy_header_cb(hwire_callbacks_t *cb, hwire_header_t *header)
{
    (void)cb;
    (void)header;
    return 0;
}

static int dummy_request_cb(hwire_callbacks_t *cb, hwire_request_t *req)
{
    (void)cb;
    (void)req;
    return 0;
}

static void bench_hwire(const unsigned char *data, size_t len)
{
    size_t pos           = 0;
    hwire_callbacks_t cb = {0};
    cb.key_lc.size       = 0;
    cb.header_cb         = dummy_header_cb;
    cb.request_cb        = dummy_request_cb;
    hwire_parse_request((char *)data, len, &pos, HWIRE_MAX_MSGLEN,
                        HWIRE_MAX_NHDRS, &cb);
}

static void bench_hwire_lc(const unsigned char *data, size_t len)
{
    size_t pos = 0;
    char key_buf[MAX_KEY_LEN];
    hwire_callbacks_t cb = {0};
    cb.key_lc.buf        = key_buf;
    cb.key_lc.size       = sizeof(key_buf);
    cb.header_cb         = dummy_header_cb;
    cb.request_cb        = dummy_request_cb;
    hwire_parse_request((char *)data, len, &pos, HWIRE_MAX_MSGLEN,
                        HWIRE_MAX_NHDRS, &cb);
}

TEST_CASE("Header Count", "[hwire][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "8 Headers, %zu B", sizeof(REQ_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_HDR_8, sizeof(REQ_HDR_8) - 1);
    };
    snprintf(n, sizeof(n), "8 Headers, %zu B, LC", sizeof(REQ_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_HDR_8, sizeof(REQ_HDR_8) - 1);
    };
    snprintf(n, sizeof(n), "15 Headers, %zu B", sizeof(REQ_HDR_15) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_HDR_15, sizeof(REQ_HDR_15) - 1);
    };
    snprintf(n, sizeof(n), "15 Headers, %zu B, LC", sizeof(REQ_HDR_15) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_HDR_15, sizeof(REQ_HDR_15) - 1);
    };
    snprintf(n, sizeof(n), "20 Headers, %zu B", sizeof(REQ_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_HDR_20, sizeof(REQ_HDR_20) - 1);
    };
    snprintf(n, sizeof(n), "20 Headers, %zu B, LC", sizeof(REQ_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_HDR_20, sizeof(REQ_HDR_20) - 1);
    };
    snprintf(n, sizeof(n), "28 Headers, %zu B", sizeof(REQ_HDR_28) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_HDR_28, sizeof(REQ_HDR_28) - 1);
    };
    snprintf(n, sizeof(n), "28 Headers, %zu B, LC", sizeof(REQ_HDR_28) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_HDR_28, sizeof(REQ_HDR_28) - 1);
    };
}

TEST_CASE("Header Value Length", "[hwire][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "Short Values, %zu B", sizeof(REQ_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_VAL_SHORT, sizeof(REQ_VAL_SHORT) - 1);
    };
    snprintf(n, sizeof(n), "Short Values, %zu B, LC",
             sizeof(REQ_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_VAL_SHORT, sizeof(REQ_VAL_SHORT) - 1);
    };
    snprintf(n, sizeof(n), "Medium Values, %zu B", sizeof(REQ_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_VAL_MEDIUM, sizeof(REQ_VAL_MEDIUM) - 1);
    };
    snprintf(n, sizeof(n), "Medium Values, %zu B, LC",
             sizeof(REQ_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_VAL_MEDIUM, sizeof(REQ_VAL_MEDIUM) - 1);
    };
    snprintf(n, sizeof(n), "Long Values, %zu B", sizeof(REQ_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_VAL_LONG, sizeof(REQ_VAL_LONG) - 1);
    };
    snprintf(n, sizeof(n), "Long Values, %zu B, LC", sizeof(REQ_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_VAL_LONG, sizeof(REQ_VAL_LONG) - 1);
    };
    snprintf(n, sizeof(n), "Extra Long Values, %zu B",
             sizeof(REQ_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_VAL_XLONG, sizeof(REQ_VAL_XLONG) - 1);
    };
    snprintf(n, sizeof(n), "Extra Long Values, %zu B, LC",
             sizeof(REQ_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_VAL_XLONG, sizeof(REQ_VAL_XLONG) - 1);
    };
}

TEST_CASE("Case Sensitivity", "[hwire][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "All Lowercase, %zu B", sizeof(REQ_CASE_LOWER) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_CASE_LOWER, sizeof(REQ_CASE_LOWER) - 1);
    };
    snprintf(n, sizeof(n), "All Lowercase, %zu B, LC",
             sizeof(REQ_CASE_LOWER) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_CASE_LOWER, sizeof(REQ_CASE_LOWER) - 1);
    };
    snprintf(n, sizeof(n), "Mixed Case, %zu B", sizeof(REQ_CASE_MIXED) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_CASE_MIXED, sizeof(REQ_CASE_MIXED) - 1);
    };
    snprintf(n, sizeof(n), "Mixed Case, %zu B, LC", sizeof(REQ_CASE_MIXED) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_CASE_MIXED, sizeof(REQ_CASE_MIXED) - 1);
    };
}

TEST_CASE("Real-World Requests", "[hwire][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "Browser, %zu B", sizeof(REQ_REAL_BROWSER) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_REAL_BROWSER, sizeof(REQ_REAL_BROWSER) - 1);
    };
    snprintf(n, sizeof(n), "Browser, %zu B, LC", sizeof(REQ_REAL_BROWSER) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_REAL_BROWSER, sizeof(REQ_REAL_BROWSER) - 1);
    };
    snprintf(n, sizeof(n), "REST API, %zu B", sizeof(REQ_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_REAL_API, sizeof(REQ_REAL_API) - 1);
    };
    snprintf(n, sizeof(n), "REST API, %zu B, LC", sizeof(REQ_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_REAL_API, sizeof(REQ_REAL_API) - 1);
    };
    snprintf(n, sizeof(n), "Mobile App, %zu B", sizeof(REQ_REAL_MOBILE) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_REAL_MOBILE, sizeof(REQ_REAL_MOBILE) - 1);
    };
    snprintf(n, sizeof(n), "Mobile App, %zu B, LC",
             sizeof(REQ_REAL_MOBILE) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_REAL_MOBILE, sizeof(REQ_REAL_MOBILE) - 1);
    };
}

TEST_CASE("Baseline", "[hwire][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "No Headers, %zu B", sizeof(REQ_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_MINIMAL, sizeof(REQ_MINIMAL) - 1);
    };
    snprintf(n, sizeof(n), "No Headers, %zu B, LC", sizeof(REQ_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_MINIMAL, sizeof(REQ_MINIMAL) - 1);
    };
    snprintf(n, sizeof(n), "Host Only, %zu B", sizeof(REQ_MINIMAL_HOST) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1);
    };
    snprintf(n, sizeof(n), "Host Only, %zu B, LC",
             sizeof(REQ_MINIMAL_HOST) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1);
    };
}
