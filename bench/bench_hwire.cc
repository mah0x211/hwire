#include "hwire.h"
#include "inputs.h"
#include <catch2/catch_all.hpp>
#include <stdio.h>
#include <string.h>

#define MAX_KEY_LEN 256

static int dummy_header_cb(hwire_ctx_t *ctx, hwire_header_t *header)
{
    (void)ctx;
    (void)header;
    return 0;
}

static int dummy_request_cb(hwire_ctx_t *ctx, hwire_request_t *req)
{
    (void)ctx;
    (void)req;
    return 0;
}

static void bench_hwire(const unsigned char *data, size_t len)
{
    size_t pos     = 0;
    hwire_ctx_t cb = {0};
    cb.key_lc.size = 0;
    cb.header_cb   = dummy_header_cb;
    cb.request_cb  = dummy_request_cb;
    hwire_parse_request(&cb, (const char *)data, len, &pos, UINT16_MAX,
                        UINT8_MAX);
}

static void bench_hwire_lc(const unsigned char *data, size_t len)
{
    size_t pos = 0;
    char key_buf[MAX_KEY_LEN];
    hwire_ctx_t cb = {0};
    cb.key_lc.buf  = key_buf;
    cb.key_lc.size = sizeof(key_buf);
    cb.header_cb   = dummy_header_cb;
    cb.request_cb  = dummy_request_cb;
    hwire_parse_request(&cb, (const char *)data, len, &pos, UINT16_MAX,
                        UINT8_MAX);
}

TEST_CASE("Header Count, 8 Headers", "[req][header-count][8-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_HDR_8, sizeof(REQ_HDR_8) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_HDR_8, sizeof(REQ_HDR_8) - 1);
    };
}

TEST_CASE("Header Count, 15 Headers", "[req][header-count][15-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_HDR_15) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_HDR_15, sizeof(REQ_HDR_15) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_HDR_15) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_HDR_15, sizeof(REQ_HDR_15) - 1);
    };
}

TEST_CASE("Header Count, 20 Headers", "[req][header-count][20-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_HDR_20, sizeof(REQ_HDR_20) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_HDR_20, sizeof(REQ_HDR_20) - 1);
    };
}

TEST_CASE("Header Count, 28 Headers", "[req][header-count][28-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_HDR_28) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_HDR_28, sizeof(REQ_HDR_28) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_HDR_28) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_HDR_28, sizeof(REQ_HDR_28) - 1);
    };
}

TEST_CASE("Header Value Length, Short Values",
          "[req][header-value-length][short-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_VAL_SHORT, sizeof(REQ_VAL_SHORT) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_VAL_SHORT, sizeof(REQ_VAL_SHORT) - 1);
    };
}

TEST_CASE("Header Value Length, Medium Values",
          "[req][header-value-length][medium-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_VAL_MEDIUM, sizeof(REQ_VAL_MEDIUM) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_VAL_MEDIUM, sizeof(REQ_VAL_MEDIUM) - 1);
    };
}

TEST_CASE("Header Value Length, Long Values",
          "[req][header-value-length][long-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_VAL_LONG, sizeof(REQ_VAL_LONG) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_VAL_LONG, sizeof(REQ_VAL_LONG) - 1);
    };
}

TEST_CASE("Header Value Length, Extra Long Values",
          "[req][header-value-length][extra-long-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_VAL_XLONG, sizeof(REQ_VAL_XLONG) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_VAL_XLONG, sizeof(REQ_VAL_XLONG) - 1);
    };
}

TEST_CASE("Case Sensitivity, All Lowercase",
          "[req][case-sensitivity][all-lowercase]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_CASE_LOWER) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_CASE_LOWER, sizeof(REQ_CASE_LOWER) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_CASE_LOWER) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_CASE_LOWER, sizeof(REQ_CASE_LOWER) - 1);
    };
}

TEST_CASE("Case Sensitivity, Mixed Case", "[req][case-sensitivity][mixed-case]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_CASE_MIXED) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_CASE_MIXED, sizeof(REQ_CASE_MIXED) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_CASE_MIXED) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_CASE_MIXED, sizeof(REQ_CASE_MIXED) - 1);
    };
}

TEST_CASE("Real-World Requests, Browser", "[req][real-world][browser]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_REAL_BROWSER) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_REAL_BROWSER, sizeof(REQ_REAL_BROWSER) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_REAL_BROWSER) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_REAL_BROWSER, sizeof(REQ_REAL_BROWSER) - 1);
    };
}

TEST_CASE("Real-World Requests, REST API", "[req][real-world][rest-api]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_REAL_API, sizeof(REQ_REAL_API) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_REAL_API, sizeof(REQ_REAL_API) - 1);
    };
}

TEST_CASE("Real-World Requests, Mobile App", "[req][real-world][mobile-app]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_REAL_MOBILE) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_REAL_MOBILE, sizeof(REQ_REAL_MOBILE) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_REAL_MOBILE) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_REAL_MOBILE, sizeof(REQ_REAL_MOBILE) - 1);
    };
}

TEST_CASE("Baseline, No Headers", "[req][baseline][no-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_MINIMAL, sizeof(REQ_MINIMAL) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_MINIMAL, sizeof(REQ_MINIMAL) - 1);
    };
}

TEST_CASE("Baseline, Host Only", "[req][baseline][host-only]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_MINIMAL_HOST) - 1);
    BENCHMARK(n)
    {
        return bench_hwire(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(REQ_MINIMAL_HOST) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_lc(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1);
    };
}
