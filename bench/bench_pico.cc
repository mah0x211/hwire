#include "inputs.h"
#include "picohttpparser.h"
#include <catch2/catch_all.hpp>
#include <stdio.h>
#include <string.h>

static void bench_pico(const unsigned char *data, size_t len)
{
    const char *method, *path;
    size_t method_len, path_len;
    int minor_version;
    struct phr_header headers[100];
    size_t num_headers = 100;
    phr_parse_request((const char *)data, len, &method, &method_len, &path,
                      &path_len, &minor_version, headers, &num_headers, 0);
}

TEST_CASE("Header Count, 8 Headers", "[req][header-count][8-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_HDR_8) - 1);
    BENCHMARK(n) { return bench_pico(REQ_HDR_8, sizeof(REQ_HDR_8) - 1); };
}

TEST_CASE("Header Count, 15 Headers", "[req][header-count][15-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_HDR_15) - 1);
    BENCHMARK(n) { return bench_pico(REQ_HDR_15, sizeof(REQ_HDR_15) - 1); };
}

TEST_CASE("Header Count, 20 Headers", "[req][header-count][20-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_HDR_20) - 1);
    BENCHMARK(n) { return bench_pico(REQ_HDR_20, sizeof(REQ_HDR_20) - 1); };
}

TEST_CASE("Header Count, 28 Headers", "[req][header-count][28-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_HDR_28) - 1);
    BENCHMARK(n) { return bench_pico(REQ_HDR_28, sizeof(REQ_HDR_28) - 1); };
}

TEST_CASE("Header Value Length, Short Values", "[req][header-value-length][short-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_VAL_SHORT) - 1);
    BENCHMARK(n) { return bench_pico(REQ_VAL_SHORT, sizeof(REQ_VAL_SHORT) - 1); };
}

TEST_CASE("Header Value Length, Medium Values", "[req][header-value-length][medium-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_VAL_MEDIUM) - 1);
    BENCHMARK(n) { return bench_pico(REQ_VAL_MEDIUM, sizeof(REQ_VAL_MEDIUM) - 1); };
}

TEST_CASE("Header Value Length, Long Values", "[req][header-value-length][long-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_VAL_LONG) - 1);
    BENCHMARK(n) { return bench_pico(REQ_VAL_LONG, sizeof(REQ_VAL_LONG) - 1); };
}

TEST_CASE("Header Value Length, Extra Long Values", "[req][header-value-length][extra-long-values]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_VAL_XLONG) - 1);
    BENCHMARK(n) { return bench_pico(REQ_VAL_XLONG, sizeof(REQ_VAL_XLONG) - 1); };
}

TEST_CASE("Case Sensitivity, All Lowercase", "[req][case-sensitivity][all-lowercase]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_CASE_LOWER) - 1);
    BENCHMARK(n) { return bench_pico(REQ_CASE_LOWER, sizeof(REQ_CASE_LOWER) - 1); };
}

TEST_CASE("Case Sensitivity, Mixed Case", "[req][case-sensitivity][mixed-case]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_CASE_MIXED) - 1);
    BENCHMARK(n) { return bench_pico(REQ_CASE_MIXED, sizeof(REQ_CASE_MIXED) - 1); };
}

TEST_CASE("Real-World Requests, Browser", "[req][real-world][browser]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_REAL_BROWSER) - 1);
    BENCHMARK(n) { return bench_pico(REQ_REAL_BROWSER, sizeof(REQ_REAL_BROWSER) - 1); };
}

TEST_CASE("Real-World Requests, REST API", "[req][real-world][rest-api]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_REAL_API) - 1);
    BENCHMARK(n) { return bench_pico(REQ_REAL_API, sizeof(REQ_REAL_API) - 1); };
}

TEST_CASE("Real-World Requests, Mobile App", "[req][real-world][mobile-app]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_REAL_MOBILE) - 1);
    BENCHMARK(n) { return bench_pico(REQ_REAL_MOBILE, sizeof(REQ_REAL_MOBILE) - 1); };
}

TEST_CASE("Baseline, No Headers", "[req][baseline][no-headers]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_MINIMAL) - 1);
    BENCHMARK(n) { return bench_pico(REQ_MINIMAL, sizeof(REQ_MINIMAL) - 1); };
}

TEST_CASE("Baseline, Host Only", "[req][baseline][host-only]")
{
    char n[32];
    snprintf(n, sizeof(n), "%zu B", sizeof(REQ_MINIMAL_HOST) - 1);
    BENCHMARK(n) { return bench_pico(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1); };
}

