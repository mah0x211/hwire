#include "hwire.h"
#include "inputs_resp.h"
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

static int dummy_response_cb(hwire_callbacks_t *cb, hwire_response_t *rsp)
{
    (void)cb;
    (void)rsp;
    return 0;
}

static void bench_hwire_resp(const unsigned char *data, size_t len)
{
    size_t pos           = 0;
    hwire_callbacks_t cb = {0};
    cb.key_lc.size       = 0;
    cb.header_cb         = dummy_header_cb;
    cb.response_cb       = dummy_response_cb;
    hwire_parse_response((char *)data, len, &pos, HWIRE_MAX_MSGLEN,
                         HWIRE_MAX_NHDRS, &cb);
}

static void bench_hwire_resp_lc(const unsigned char *data, size_t len)
{
    size_t pos = 0;
    char key_buf[MAX_KEY_LEN];
    hwire_callbacks_t cb = {0};
    cb.key_lc.buf        = key_buf;
    cb.key_lc.size       = sizeof(key_buf);
    cb.header_cb         = dummy_header_cb;
    cb.response_cb       = dummy_response_cb;
    hwire_parse_response((char *)data, len, &pos, HWIRE_MAX_MSGLEN,
                         HWIRE_MAX_NHDRS, &cb);
}

TEST_CASE("Header Count", "[hwire][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "4 Headers, %zu B", sizeof(RSP_HDR_4) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_HDR_4, sizeof(RSP_HDR_4) - 1);
    };
    snprintf(n, sizeof(n), "4 Headers, %zu B, LC", sizeof(RSP_HDR_4) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_HDR_4, sizeof(RSP_HDR_4) - 1);
    };
    snprintf(n, sizeof(n), "8 Headers, %zu B", sizeof(RSP_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_HDR_8, sizeof(RSP_HDR_8) - 1);
    };
    snprintf(n, sizeof(n), "8 Headers, %zu B, LC", sizeof(RSP_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_HDR_8, sizeof(RSP_HDR_8) - 1);
    };
    snprintf(n, sizeof(n), "12 Headers, %zu B", sizeof(RSP_HDR_12) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_HDR_12, sizeof(RSP_HDR_12) - 1);
    };
    snprintf(n, sizeof(n), "12 Headers, %zu B, LC", sizeof(RSP_HDR_12) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_HDR_12, sizeof(RSP_HDR_12) - 1);
    };
    snprintf(n, sizeof(n), "20 Headers, %zu B", sizeof(RSP_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_HDR_20, sizeof(RSP_HDR_20) - 1);
    };
    snprintf(n, sizeof(n), "20 Headers, %zu B, LC", sizeof(RSP_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_HDR_20, sizeof(RSP_HDR_20) - 1);
    };
}

TEST_CASE("Header Value Length", "[hwire][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "Short Values, %zu B", sizeof(RSP_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_VAL_SHORT, sizeof(RSP_VAL_SHORT) - 1);
    };
    snprintf(n, sizeof(n), "Short Values, %zu B, LC",
             sizeof(RSP_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_VAL_SHORT, sizeof(RSP_VAL_SHORT) - 1);
    };
    snprintf(n, sizeof(n), "Medium Values, %zu B", sizeof(RSP_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_VAL_MEDIUM, sizeof(RSP_VAL_MEDIUM) - 1);
    };
    snprintf(n, sizeof(n), "Medium Values, %zu B, LC",
             sizeof(RSP_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_VAL_MEDIUM, sizeof(RSP_VAL_MEDIUM) - 1);
    };
    snprintf(n, sizeof(n), "Long Values, %zu B", sizeof(RSP_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_VAL_LONG, sizeof(RSP_VAL_LONG) - 1);
    };
    snprintf(n, sizeof(n), "Long Values, %zu B, LC", sizeof(RSP_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_VAL_LONG, sizeof(RSP_VAL_LONG) - 1);
    };
    snprintf(n, sizeof(n), "Extra Long Values, %zu B",
             sizeof(RSP_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_VAL_XLONG, sizeof(RSP_VAL_XLONG) - 1);
    };
    snprintf(n, sizeof(n), "Extra Long Values, %zu B, LC",
             sizeof(RSP_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_VAL_XLONG, sizeof(RSP_VAL_XLONG) - 1);
    };
}

TEST_CASE("Real-World Responses", "[hwire][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "HTML Page, %zu B", sizeof(RSP_REAL_HTML) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_REAL_HTML, sizeof(RSP_REAL_HTML) - 1);
    };
    snprintf(n, sizeof(n), "HTML Page, %zu B, LC", sizeof(RSP_REAL_HTML) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_REAL_HTML, sizeof(RSP_REAL_HTML) - 1);
    };
    snprintf(n, sizeof(n), "REST API, %zu B", sizeof(RSP_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_REAL_API, sizeof(RSP_REAL_API) - 1);
    };
    snprintf(n, sizeof(n), "REST API, %zu B, LC", sizeof(RSP_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_REAL_API, sizeof(RSP_REAL_API) - 1);
    };
    snprintf(n, sizeof(n), "Static File, %zu B", sizeof(RSP_REAL_STATIC) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_REAL_STATIC, sizeof(RSP_REAL_STATIC) - 1);
    };
    snprintf(n, sizeof(n), "Static File, %zu B, LC",
             sizeof(RSP_REAL_STATIC) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_REAL_STATIC,
                                   sizeof(RSP_REAL_STATIC) - 1);
    };
}

TEST_CASE("Baseline", "[hwire][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "No Extra Headers, %zu B", sizeof(RSP_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_MINIMAL, sizeof(RSP_MINIMAL) - 1);
    };
    snprintf(n, sizeof(n), "No Extra Headers, %zu B, LC",
             sizeof(RSP_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_MINIMAL, sizeof(RSP_MINIMAL) - 1);
    };
    snprintf(n, sizeof(n), "Date Header Only, %zu B",
             sizeof(RSP_MINIMAL_DATE) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp(RSP_MINIMAL_DATE, sizeof(RSP_MINIMAL_DATE) - 1);
    };
    snprintf(n, sizeof(n), "Date Header Only, %zu B, LC",
             sizeof(RSP_MINIMAL_DATE) - 1);
    BENCHMARK(n)
    {
        return bench_hwire_resp_lc(RSP_MINIMAL_DATE,
                                   sizeof(RSP_MINIMAL_DATE) - 1);
    };
}
