#include "inputs_resp.h"
#include "llhttp.h"
#include <catch2/catch_all.hpp>
#include <stdio.h>
#include <string.h>

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

static int on_status(llhttp_t *p, const char *at, size_t len)
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

static void bench_llhttp_resp(const unsigned char *data, size_t len)
{
    llhttp_t parser;
    llhttp_settings_t settings;

    llhttp_settings_init(&settings);
    settings.on_status           = on_status;
    settings.on_header_field     = on_header_field;
    settings.on_header_value     = on_header_value;
    settings.on_headers_complete = on_headers_complete;
    settings.on_message_complete = on_message_complete;

    llhttp_init(&parser, HTTP_RESPONSE, &settings);
    llhttp_execute(&parser, (const char *)data, len);
}

TEST_CASE("Header Count", "[llhttp][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "4 Headers, %zu B", sizeof(RSP_HDR_4) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_HDR_4, sizeof(RSP_HDR_4) - 1);
    };
    snprintf(n, sizeof(n), "8 Headers, %zu B", sizeof(RSP_HDR_8) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_HDR_8, sizeof(RSP_HDR_8) - 1);
    };
    snprintf(n, sizeof(n), "12 Headers, %zu B", sizeof(RSP_HDR_12) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_HDR_12, sizeof(RSP_HDR_12) - 1);
    };
    snprintf(n, sizeof(n), "20 Headers, %zu B", sizeof(RSP_HDR_20) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_HDR_20, sizeof(RSP_HDR_20) - 1);
    };
}

TEST_CASE("Header Value Length", "[llhttp][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "Short Values, %zu B", sizeof(RSP_VAL_SHORT) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_VAL_SHORT, sizeof(RSP_VAL_SHORT) - 1);
    };
    snprintf(n, sizeof(n), "Medium Values, %zu B", sizeof(RSP_VAL_MEDIUM) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_VAL_MEDIUM, sizeof(RSP_VAL_MEDIUM) - 1);
    };
    snprintf(n, sizeof(n), "Long Values, %zu B", sizeof(RSP_VAL_LONG) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_VAL_LONG, sizeof(RSP_VAL_LONG) - 1);
    };
    snprintf(n, sizeof(n), "Extra Long Values, %zu B",
             sizeof(RSP_VAL_XLONG) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_VAL_XLONG, sizeof(RSP_VAL_XLONG) - 1);
    };
}

TEST_CASE("Real-World Responses", "[llhttp][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "HTML Page, %zu B", sizeof(RSP_REAL_HTML) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_REAL_HTML, sizeof(RSP_REAL_HTML) - 1);
    };
    snprintf(n, sizeof(n), "REST API, %zu B", sizeof(RSP_REAL_API) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_REAL_API, sizeof(RSP_REAL_API) - 1);
    };
    snprintf(n, sizeof(n), "Static File, %zu B", sizeof(RSP_REAL_STATIC) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_REAL_STATIC, sizeof(RSP_REAL_STATIC) - 1);
    };
}

TEST_CASE("Baseline", "[llhttp][resp]")
{
    char n[128];
    snprintf(n, sizeof(n), "No Extra Headers, %zu B", sizeof(RSP_MINIMAL) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_MINIMAL, sizeof(RSP_MINIMAL) - 1);
    };
    snprintf(n, sizeof(n), "Date Header Only, %zu B",
             sizeof(RSP_MINIMAL_DATE) - 1);
    BENCHMARK(n)
    {
        return bench_llhttp_resp(RSP_MINIMAL_DATE,
                                 sizeof(RSP_MINIMAL_DATE) - 1);
    };
}
