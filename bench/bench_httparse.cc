#include "inputs.h"
#include <catch2/catch_all.hpp>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Rust FFI declarations
extern "C" int httparse_bench_parse(const uint8_t *data, size_t len);

static void bench_httparse(const unsigned char *data, size_t len)
{
    httparse_bench_parse(data, len);
}

TEST_CASE("Header Count", "[httparse][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "8 Headers, %zu B", sizeof(REQ_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_HDR_8, sizeof(REQ_HDR_8) - 1);
    };
    snprintf(n, sizeof(n), "15 Headers, %zu B", sizeof(REQ_HDR_15) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_HDR_15, sizeof(REQ_HDR_15) - 1);
    };
    snprintf(n, sizeof(n), "20 Headers, %zu B", sizeof(REQ_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_HDR_20, sizeof(REQ_HDR_20) - 1);
    };
    snprintf(n, sizeof(n), "28 Headers, %zu B", sizeof(REQ_HDR_28) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_HDR_28, sizeof(REQ_HDR_28) - 1);
    };
}

TEST_CASE("Header Value Length", "[httparse][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "Short Values, %zu B", sizeof(REQ_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_VAL_SHORT, sizeof(REQ_VAL_SHORT) - 1);
    };
    snprintf(n, sizeof(n), "Medium Values, %zu B", sizeof(REQ_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_VAL_MEDIUM, sizeof(REQ_VAL_MEDIUM) - 1);
    };
    snprintf(n, sizeof(n), "Long Values, %zu B", sizeof(REQ_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_VAL_LONG, sizeof(REQ_VAL_LONG) - 1);
    };
    snprintf(n, sizeof(n), "Extra Long Values, %zu B",
             sizeof(REQ_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_VAL_XLONG, sizeof(REQ_VAL_XLONG) - 1);
    };
}

TEST_CASE("Case Sensitivity", "[httparse][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "All Lowercase, %zu B", sizeof(REQ_CASE_LOWER) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_CASE_LOWER, sizeof(REQ_CASE_LOWER) - 1);
    };
    snprintf(n, sizeof(n), "Mixed Case, %zu B", sizeof(REQ_CASE_MIXED) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_CASE_MIXED, sizeof(REQ_CASE_MIXED) - 1);
    };
}

TEST_CASE("Real-World Requests", "[httparse][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "Browser, %zu B", sizeof(REQ_REAL_BROWSER) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_REAL_BROWSER, sizeof(REQ_REAL_BROWSER) - 1);
    };
    snprintf(n, sizeof(n), "REST API, %zu B", sizeof(REQ_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_REAL_API, sizeof(REQ_REAL_API) - 1);
    };
    snprintf(n, sizeof(n), "Mobile App, %zu B", sizeof(REQ_REAL_MOBILE) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_REAL_MOBILE, sizeof(REQ_REAL_MOBILE) - 1);
    };
}

TEST_CASE("Baseline", "[httparse][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "No Headers, %zu B", sizeof(REQ_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_MINIMAL, sizeof(REQ_MINIMAL) - 1);
    };
    snprintf(n, sizeof(n), "Host Only, %zu B", sizeof(REQ_MINIMAL_HOST) - 1);
    BENCHMARK(n)
    {
        return bench_httparse(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1);
    };
}

BENCHMARK("minimal_host")
{
    return bench_httparse(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1);
};
}
