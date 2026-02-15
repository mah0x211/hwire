#include <catch2/catch_all.hpp>
#include <iostream>
#include <string.h>
#include "llhttp.h"
#include "inputs.h"

static int on_message_begin(llhttp_t *p) {
    (void)p;
    return 0;
}

static int on_url(llhttp_t *p, const char *at, size_t len) {
    (void)p;
    (void)at;
    (void)len;
    return 0;
}

static int on_header_field(llhttp_t *p, const char *at, size_t len) {
    (void)p;
    (void)at;
    (void)len;
    return 0;
}

static int on_header_value(llhttp_t *p, const char *at, size_t len) {
    (void)p;
    (void)at;
    (void)len;
    return 0;
}

static int on_headers_complete(llhttp_t *p) {
    (void)p;
    return 0;
}

static int on_message_complete(llhttp_t *p) {
    (void)p;
    return 0;
}

static void bench_llhttp(const unsigned char *data, size_t len) {
    llhttp_t parser;
    llhttp_settings_t settings;

    llhttp_settings_init(&settings);
    settings.on_message_begin = on_message_begin;
    settings.on_url = on_url;
    settings.on_header_field = on_header_field;
    settings.on_header_value = on_header_value;
    settings.on_headers_complete = on_headers_complete;
    settings.on_message_complete = on_message_complete;

    llhttp_init(&parser, HTTP_REQUEST, &settings);
    llhttp_execute(&parser, (const char *)data, len);
}

TEST_CASE("llhttp benchmarks", "[llhttp]") {
    // Output data sizes (parsed by generate_report.js)
    std::cout << "DATA_SIZE: hdr_8=" << (sizeof(REQ_HDR_8) - 1) << std::endl;
    std::cout << "DATA_SIZE: hdr_15=" << (sizeof(REQ_HDR_15) - 1) << std::endl;
    std::cout << "DATA_SIZE: hdr_20=" << (sizeof(REQ_HDR_20) - 1) << std::endl;
    std::cout << "DATA_SIZE: hdr_28=" << (sizeof(REQ_HDR_28) - 1) << std::endl;
    std::cout << "DATA_SIZE: val_short=" << (sizeof(REQ_VAL_SHORT) - 1) << std::endl;
    std::cout << "DATA_SIZE: val_medium=" << (sizeof(REQ_VAL_MEDIUM) - 1) << std::endl;
    std::cout << "DATA_SIZE: val_long=" << (sizeof(REQ_VAL_LONG) - 1) << std::endl;
    std::cout << "DATA_SIZE: val_xlong=" << (sizeof(REQ_VAL_XLONG) - 1) << std::endl;
    std::cout << "DATA_SIZE: case_lower=" << (sizeof(REQ_CASE_LOWER) - 1) << std::endl;
    std::cout << "DATA_SIZE: case_mixed=" << (sizeof(REQ_CASE_MIXED) - 1) << std::endl;
    std::cout << "DATA_SIZE: real_browser=" << (sizeof(REQ_REAL_BROWSER) - 1) << std::endl;
    std::cout << "DATA_SIZE: real_api=" << (sizeof(REQ_REAL_API) - 1) << std::endl;
    std::cout << "DATA_SIZE: real_mobile=" << (sizeof(REQ_REAL_MOBILE) - 1) << std::endl;
    std::cout << "DATA_SIZE: minimal=" << (sizeof(REQ_MINIMAL) - 1) << std::endl;
    std::cout << "DATA_SIZE: minimal_host=" << (sizeof(REQ_MINIMAL_HOST) - 1) << std::endl;
    std::cout << std::endl;

    // Category 1: Header Count (Modern Browser Structure)
    BENCHMARK("hdr_8") { return bench_llhttp(REQ_HDR_8, sizeof(REQ_HDR_8) - 1); };
    BENCHMARK("hdr_15") { return bench_llhttp(REQ_HDR_15, sizeof(REQ_HDR_15) - 1); };
    BENCHMARK("hdr_20") { return bench_llhttp(REQ_HDR_20, sizeof(REQ_HDR_20) - 1); };
    BENCHMARK("hdr_28") { return bench_llhttp(REQ_HDR_28, sizeof(REQ_HDR_28) - 1); };

    // Category 2: Header Value Length
    BENCHMARK("val_short") { return bench_llhttp(REQ_VAL_SHORT, sizeof(REQ_VAL_SHORT) - 1); };
    BENCHMARK("val_medium") { return bench_llhttp(REQ_VAL_MEDIUM, sizeof(REQ_VAL_MEDIUM) - 1); };
    BENCHMARK("val_long") { return bench_llhttp(REQ_VAL_LONG, sizeof(REQ_VAL_LONG) - 1); };
    BENCHMARK("val_xlong") { return bench_llhttp(REQ_VAL_XLONG, sizeof(REQ_VAL_XLONG) - 1); };

    // Category 3: Case Sensitivity
    BENCHMARK("case_lower") { return bench_llhttp(REQ_CASE_LOWER, sizeof(REQ_CASE_LOWER) - 1); };
    BENCHMARK("case_mixed") { return bench_llhttp(REQ_CASE_MIXED, sizeof(REQ_CASE_MIXED) - 1); };

    // Category 4: Real-World
    BENCHMARK("real_browser") { return bench_llhttp(REQ_REAL_BROWSER, sizeof(REQ_REAL_BROWSER) - 1); };
    BENCHMARK("real_api") { return bench_llhttp(REQ_REAL_API, sizeof(REQ_REAL_API) - 1); };
    BENCHMARK("real_mobile") { return bench_llhttp(REQ_REAL_MOBILE, sizeof(REQ_REAL_MOBILE) - 1); };

    // Category 5: Baseline
    BENCHMARK("minimal") { return bench_llhttp(REQ_MINIMAL, sizeof(REQ_MINIMAL) - 1); };
    BENCHMARK("minimal_host") { return bench_llhttp(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1); };
}
