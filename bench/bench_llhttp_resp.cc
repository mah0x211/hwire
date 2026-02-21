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

TEST_CASE("Header Count, 4 Headers", "[resp][header-count][4-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_HDR_4) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_HDR_4, sizeof(RSP_HDR_4) - 1); };
}

TEST_CASE("Header Count, 8 Headers", "[resp][header-count][8-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_HDR_8) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_HDR_8, sizeof(RSP_HDR_8) - 1); };
}

TEST_CASE("Header Count, 12 Headers", "[resp][header-count][12-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_HDR_12) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_HDR_12, sizeof(RSP_HDR_12) - 1); };
}

TEST_CASE("Header Count, 20 Headers", "[resp][header-count][20-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_HDR_20) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_HDR_20, sizeof(RSP_HDR_20) - 1); };
}

TEST_CASE("Header Value Length, Short Values", "[resp][header-value-length][short-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_VAL_SHORT) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_VAL_SHORT, sizeof(RSP_VAL_SHORT) - 1); };
}

TEST_CASE("Header Value Length, Medium Values", "[resp][header-value-length][medium-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_VAL_MEDIUM) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_VAL_MEDIUM, sizeof(RSP_VAL_MEDIUM) - 1); };
}

TEST_CASE("Header Value Length, Long Values", "[resp][header-value-length][long-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_VAL_LONG) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_VAL_LONG, sizeof(RSP_VAL_LONG) - 1); };
}

TEST_CASE("Header Value Length, Extra Long Values", "[resp][header-value-length][extra-long-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_VAL_XLONG) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_VAL_XLONG, sizeof(RSP_VAL_XLONG) - 1); };
}

TEST_CASE("Case Sensitivity, All Lowercase", "[resp][case-sensitivity][all-lowercase]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_CASE_LOWER) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_CASE_LOWER, sizeof(RSP_CASE_LOWER) - 1); };
}

TEST_CASE("Case Sensitivity, Mixed Case", "[resp][case-sensitivity][mixed-case]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_CASE_MIXED) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_CASE_MIXED, sizeof(RSP_CASE_MIXED) - 1); };
}

TEST_CASE("Real-World Responses, HTML Page", "[resp][real-world][html-page]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_REAL_HTML) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_REAL_HTML, sizeof(RSP_REAL_HTML) - 1); };
}

TEST_CASE("Real-World Responses, REST API", "[resp][real-world][rest-api]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_REAL_API) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_REAL_API, sizeof(RSP_REAL_API) - 1); };
}

TEST_CASE("Real-World Responses, Static File", "[resp][real-world][static-file]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_REAL_STATIC) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_REAL_STATIC, sizeof(RSP_REAL_STATIC) - 1); };
}

TEST_CASE("Baseline, No Extra Headers", "[resp][baseline][no-extra-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_MINIMAL) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_MINIMAL, sizeof(RSP_MINIMAL) - 1); };
}

TEST_CASE("Baseline, Date Header Only", "[resp][baseline][date-header-only]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(RSP_MINIMAL_DATE) - 1);
    BENCHMARK(n) { return bench_llhttp_resp(RSP_MINIMAL_DATE, sizeof(RSP_MINIMAL_DATE) - 1); };
}

