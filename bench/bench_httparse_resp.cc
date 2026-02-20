#include "inputs_resp.h"
#include <catch2/catch_all.hpp>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Rust FFI declaration
extern "C" int httparse_bench_parse_response(const uint8_t *data, size_t len);

static void bench_httparse_resp(const unsigned char *data, size_t len)
{
    httparse_bench_parse_response(data, len);
}

TEST_CASE("Header Count", "[httparse][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "4 Headers, %zu B", sizeof(RSP_HDR_4) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_HDR_4, sizeof(RSP_HDR_4) - 1);
    };
    snprintf(n, sizeof(n), "8 Headers, %zu B", sizeof(RSP_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_HDR_8, sizeof(RSP_HDR_8) - 1);
    };
    snprintf(n, sizeof(n), "12 Headers, %zu B", sizeof(RSP_HDR_12) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_HDR_12, sizeof(RSP_HDR_12) - 1);
    };
    snprintf(n, sizeof(n), "20 Headers, %zu B", sizeof(RSP_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_HDR_20, sizeof(RSP_HDR_20) - 1);
    };
}

TEST_CASE("Header Value Length", "[httparse][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "Short Values, %zu B", sizeof(RSP_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_VAL_SHORT, sizeof(RSP_VAL_SHORT) - 1);
    };
    snprintf(n, sizeof(n), "Medium Values, %zu B", sizeof(RSP_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_VAL_MEDIUM, sizeof(RSP_VAL_MEDIUM) - 1);
    };
    snprintf(n, sizeof(n), "Long Values, %zu B", sizeof(RSP_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_VAL_LONG, sizeof(RSP_VAL_LONG) - 1);
    };
    snprintf(n, sizeof(n), "Extra Long Values, %zu B",
             sizeof(RSP_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_VAL_XLONG, sizeof(RSP_VAL_XLONG) - 1);
    };
}

TEST_CASE("Real-World Responses", "[httparse][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "HTML Page, %zu B", sizeof(RSP_REAL_HTML) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_REAL_HTML, sizeof(RSP_REAL_HTML) - 1);
    };
    snprintf(n, sizeof(n), "REST API, %zu B", sizeof(RSP_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_REAL_API, sizeof(RSP_REAL_API) - 1);
    };
    snprintf(n, sizeof(n), "Static File, %zu B", sizeof(RSP_REAL_STATIC) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_REAL_STATIC,
                                   sizeof(RSP_REAL_STATIC) - 1);
    };
}

TEST_CASE("Baseline", "[httparse][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "No Extra Headers, %zu B", sizeof(RSP_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_MINIMAL, sizeof(RSP_MINIMAL) - 1);
    };
    snprintf(n, sizeof(n), "Date Header Only, %zu B",
             sizeof(RSP_MINIMAL_DATE) - 1);
    BENCHMARK(n)
    {
        return bench_httparse_resp(RSP_MINIMAL_DATE,
                                   sizeof(RSP_MINIMAL_DATE) - 1);
    };
}
