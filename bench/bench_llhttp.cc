#include "inputs.h"
#include "llhttp.h"
#include <catch2/catch_all.hpp>
#include <stdio.h>
#include <string.h>

static int on_message_begin(llhttp_t *p)
{
    (void)p;
    return 0;
}

static int on_url(llhttp_t *p, const char *at, size_t len)
{
    (void)p;
    (void)at;
    (void)len;
    return 0;
}

static int on_header_field(llhttp_t *p, const char *at, size_t len)
{
    (void)p;
    (void)at;
    (void)len;
    return 0;
}

static int on_header_value(llhttp_t *p, const char *at, size_t len)
{
    (void)p;
    (void)at;
    (void)len;
    return 0;
}

static int on_headers_complete(llhttp_t *p)
{
    (void)p;
    return 0;
}

static int on_message_complete(llhttp_t *p)
{
    (void)p;
    return 0;
}

static void bench_llhttp(const unsigned char *data, size_t len)
{
    llhttp_t parser;
    llhttp_settings_t settings;

    llhttp_settings_init(&settings);
    settings.on_message_begin    = on_message_begin;
    settings.on_url              = on_url;
    settings.on_header_field     = on_header_field;
    settings.on_header_value     = on_header_value;
    settings.on_headers_complete = on_headers_complete;
    settings.on_message_complete = on_message_complete;

    llhttp_init(&parser, HTTP_REQUEST, &settings);
    llhttp_execute(&parser, (const char *)data, len);
}

TEST_CASE("Header Count", "[llhttp][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "8 Headers, %zu B", sizeof(REQ_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_HDR_8, sizeof(REQ_HDR_8) - 1);
    };
    snprintf(n, sizeof(n), "15 Headers, %zu B", sizeof(REQ_HDR_15) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_HDR_15, sizeof(REQ_HDR_15) - 1);
    };
    snprintf(n, sizeof(n), "20 Headers, %zu B", sizeof(REQ_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_HDR_20, sizeof(REQ_HDR_20) - 1);
    };
    snprintf(n, sizeof(n), "28 Headers, %zu B", sizeof(REQ_HDR_28) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_HDR_28, sizeof(REQ_HDR_28) - 1);
    };
}

TEST_CASE("Header Value Length", "[llhttp][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "Short Values, %zu B", sizeof(REQ_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_VAL_SHORT, sizeof(REQ_VAL_SHORT) - 1);
    };
    snprintf(n, sizeof(n), "Medium Values, %zu B", sizeof(REQ_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_VAL_MEDIUM, sizeof(REQ_VAL_MEDIUM) - 1);
    };
    snprintf(n, sizeof(n), "Long Values, %zu B", sizeof(REQ_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_VAL_LONG, sizeof(REQ_VAL_LONG) - 1);
    };
    snprintf(n, sizeof(n), "Extra Long Values, %zu B",
             sizeof(REQ_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_VAL_XLONG, sizeof(REQ_VAL_XLONG) - 1);
    };
}

TEST_CASE("Case Sensitivity", "[llhttp][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "All Lowercase, %zu B", sizeof(REQ_CASE_LOWER) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_CASE_LOWER, sizeof(REQ_CASE_LOWER) - 1);
    };
    snprintf(n, sizeof(n), "Mixed Case, %zu B", sizeof(REQ_CASE_MIXED) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_CASE_MIXED, sizeof(REQ_CASE_MIXED) - 1);
    };
}

TEST_CASE("Real-World Requests", "[llhttp][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "Browser, %zu B", sizeof(REQ_REAL_BROWSER) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_REAL_BROWSER, sizeof(REQ_REAL_BROWSER) - 1);
    };
    snprintf(n, sizeof(n), "REST API, %zu B", sizeof(REQ_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_REAL_API, sizeof(REQ_REAL_API) - 1);
    };
    snprintf(n, sizeof(n), "Mobile App, %zu B", sizeof(REQ_REAL_MOBILE) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_REAL_MOBILE, sizeof(REQ_REAL_MOBILE) - 1);
    };
}

TEST_CASE("Baseline", "[llhttp][req]")
{
    char n[128];
    snprintf(n, sizeof(n), "No Headers, %zu B", sizeof(REQ_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_MINIMAL, sizeof(REQ_MINIMAL) - 1);
    };
    snprintf(n, sizeof(n), "Host Only, %zu B", sizeof(REQ_MINIMAL_HOST) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1);
    };
}
