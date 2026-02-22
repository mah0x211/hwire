#include "hwire.h"
#include "inputs_resp.h"
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

static int dummy_response_cb(hwire_ctx_t *ctx, hwire_response_t *rsp)
{
    (void)ctx;
    (void)rsp;
    return 0;
}

static void bench_hwire_resp(const unsigned char *data, size_t len)
{
    size_t pos     = 0;
    hwire_ctx_t cb = {0};
    cb.key_lc.size = 0;
    cb.header_cb   = dummy_header_cb;
    cb.response_cb = dummy_response_cb;
    hwire_parse_response(&cb, (const char *)data, len, &pos, UINT16_MAX,
                         UINT8_MAX);
}

static void bench_hwire_resp_lc(const unsigned char *data, size_t len)
{
    size_t pos = 0;
    char key_buf[MAX_KEY_LEN];
    hwire_ctx_t cb = {0};
    cb.key_lc.buf  = key_buf;
    cb.key_lc.size = sizeof(key_buf);
    cb.header_cb   = dummy_header_cb;
    cb.response_cb = dummy_response_cb;
    hwire_parse_response(&cb, (const char *)data, len, &pos, UINT16_MAX,
                         UINT8_MAX);
}

TEST_CASE("Header Count, 4 Headers", "[resp][header-count][4-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_HDR_4) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_HDR_4, sizeof(RSP_HDR_4) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_HDR_4) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_HDR_4, sizeof(RSP_HDR_4) - 1);
    };
}

TEST_CASE("Header Count, 8 Headers", "[resp][header-count][8-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_HDR_8, sizeof(RSP_HDR_8) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_HDR_8, sizeof(RSP_HDR_8) - 1);
    };
}

TEST_CASE("Header Count, 12 Headers", "[resp][header-count][12-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_HDR_12) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_HDR_12, sizeof(RSP_HDR_12) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_HDR_12) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_HDR_12, sizeof(RSP_HDR_12) - 1);
    };
}

TEST_CASE("Header Count, 20 Headers", "[resp][header-count][20-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_HDR_20, sizeof(RSP_HDR_20) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_HDR_20, sizeof(RSP_HDR_20) - 1);
    };
}

TEST_CASE("Header Value Length, Short Values",
          "[resp][header-value-length][short-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_VAL_SHORT, sizeof(RSP_VAL_SHORT) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_VAL_SHORT, sizeof(RSP_VAL_SHORT) - 1);
    };
}

TEST_CASE("Header Value Length, Medium Values",
          "[resp][header-value-length][medium-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_VAL_MEDIUM, sizeof(RSP_VAL_MEDIUM) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_VAL_MEDIUM, sizeof(RSP_VAL_MEDIUM) - 1);
    };
}

TEST_CASE("Header Value Length, Long Values",
          "[resp][header-value-length][long-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_VAL_LONG, sizeof(RSP_VAL_LONG) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_VAL_LONG, sizeof(RSP_VAL_LONG) - 1);
    };
}

TEST_CASE("Header Value Length, Extra Long Values",
          "[resp][header-value-length][extra-long-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_VAL_XLONG, sizeof(RSP_VAL_XLONG) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_VAL_XLONG, sizeof(RSP_VAL_XLONG) - 1);
    };
}

TEST_CASE("Case Sensitivity, All Lowercase",
          "[resp][case-sensitivity][all-lowercase]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_CASE_LOWER) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_CASE_LOWER, sizeof(RSP_CASE_LOWER) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_CASE_LOWER) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_CASE_LOWER, sizeof(RSP_CASE_LOWER) - 1);
    };
}

TEST_CASE("Case Sensitivity, Mixed Case",
          "[resp][case-sensitivity][mixed-case]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_CASE_MIXED) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_CASE_MIXED, sizeof(RSP_CASE_MIXED) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_CASE_MIXED) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_CASE_MIXED, sizeof(RSP_CASE_MIXED) - 1);
    };
}

TEST_CASE("Real-World Responses, HTML Page", "[resp][real-world][html-page]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_REAL_HTML) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_REAL_HTML, sizeof(RSP_REAL_HTML) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_REAL_HTML) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_REAL_HTML, sizeof(RSP_REAL_HTML) - 1);
    };
}

TEST_CASE("Real-World Responses, REST API", "[resp][real-world][rest-api]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_REAL_API, sizeof(RSP_REAL_API) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_REAL_API, sizeof(RSP_REAL_API) - 1);
    };
}

TEST_CASE("Real-World Responses, Static File",
          "[resp][real-world][static-file]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_REAL_STATIC) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_REAL_STATIC, sizeof(RSP_REAL_STATIC) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_REAL_STATIC) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_REAL_STATIC,
                                   sizeof(RSP_REAL_STATIC) - 1);
    };
}

TEST_CASE("Baseline, No Extra Headers", "[resp][baseline][no-extra-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_MINIMAL, sizeof(RSP_MINIMAL) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_MINIMAL, sizeof(RSP_MINIMAL) - 1);
    };
}

TEST_CASE("Baseline, Date Header Only", "[resp][baseline][date-header-only]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_MINIMAL_DATE) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_MINIMAL_DATE, sizeof(RSP_MINIMAL_DATE) - 1);
    };
    snprintf(n, sizeof(n), "%zu B, LC", sizeof(RSP_MINIMAL_DATE) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_MINIMAL_DATE,
                                   sizeof(RSP_MINIMAL_DATE) - 1);
    };
}
