#include <catch2/catch_all.hpp>
#include <iostream>
#include <string.h>
#include "hwire.h"
#include "inputs.h"

// Maximum key length for lowercase buffer
#define MAX_KEY_LEN 256

static int dummy_header_cb(hwire_callbacks_t *cb, hwire_header_t *header) {
    (void)cb;
    (void)header;
    return 0;
}

static int dummy_request_cb(hwire_callbacks_t *cb, hwire_request_t *req) {
    (void)cb;
    (void)req;
    return 0;
}

static void bench_hwire(const unsigned char *data, size_t len) {
    size_t pos = 0;

    hwire_callbacks_t cb = {0};
    cb.key_lc.size = 0;  // Disable lowercase conversion
    cb.header_cb = dummy_header_cb;
    cb.request_cb = dummy_request_cb;

    hwire_parse_request((char *)data, len, &pos, HWIRE_MAX_MSGLEN, HWIRE_MAX_NHDRS, &cb);
}

static void bench_hwire_lc(const unsigned char *data, size_t len) {
    size_t pos = 0;
    char key_buf[MAX_KEY_LEN];

    hwire_callbacks_t cb = {0};
    cb.key_lc.buf = key_buf;
    cb.key_lc.size = sizeof(key_buf);  // Enable lowercase conversion
    cb.header_cb = dummy_header_cb;
    cb.request_cb = dummy_request_cb;

    hwire_parse_request((char *)data, len, &pos, HWIRE_MAX_MSGLEN, HWIRE_MAX_NHDRS, &cb);
}

TEST_CASE("hwire benchmarks", "[hwire]") {
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
    BENCHMARK("hdr_8") { return bench_hwire(REQ_HDR_8, sizeof(REQ_HDR_8) - 1); };
    BENCHMARK("hdr_15") { return bench_hwire(REQ_HDR_15, sizeof(REQ_HDR_15) - 1); };
    BENCHMARK("hdr_20") { return bench_hwire(REQ_HDR_20, sizeof(REQ_HDR_20) - 1); };
    BENCHMARK("hdr_28") { return bench_hwire(REQ_HDR_28, sizeof(REQ_HDR_28) - 1); };

    // Category 2: Header Value Length
    BENCHMARK("val_short") { return bench_hwire(REQ_VAL_SHORT, sizeof(REQ_VAL_SHORT) - 1); };
    BENCHMARK("val_medium") { return bench_hwire(REQ_VAL_MEDIUM, sizeof(REQ_VAL_MEDIUM) - 1); };
    BENCHMARK("val_long") { return bench_hwire(REQ_VAL_LONG, sizeof(REQ_VAL_LONG) - 1); };
    BENCHMARK("val_xlong") { return bench_hwire(REQ_VAL_XLONG, sizeof(REQ_VAL_XLONG) - 1); };

    // Category 3: Case Sensitivity
    BENCHMARK("case_lower") { return bench_hwire(REQ_CASE_LOWER, sizeof(REQ_CASE_LOWER) - 1); };
    BENCHMARK("case_mixed") { return bench_hwire(REQ_CASE_MIXED, sizeof(REQ_CASE_MIXED) - 1); };

    // Category 4: Real-World
    BENCHMARK("real_browser") { return bench_hwire(REQ_REAL_BROWSER, sizeof(REQ_REAL_BROWSER) - 1); };
    BENCHMARK("real_api") { return bench_hwire(REQ_REAL_API, sizeof(REQ_REAL_API) - 1); };
    BENCHMARK("real_mobile") { return bench_hwire(REQ_REAL_MOBILE, sizeof(REQ_REAL_MOBILE) - 1); };

    // Category 5: Baseline
    BENCHMARK("minimal") { return bench_hwire(REQ_MINIMAL, sizeof(REQ_MINIMAL) - 1); };
    BENCHMARK("minimal_host") { return bench_hwire(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1); };
}

TEST_CASE("hwire lowercase benchmarks", "[hwire][lowercase]") {
    // Output data sizes for lowercase benchmarks
    std::cout << "DATA_SIZE: lc_hdr_8=" << (sizeof(REQ_HDR_8) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_hdr_15=" << (sizeof(REQ_HDR_15) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_hdr_20=" << (sizeof(REQ_HDR_20) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_hdr_28=" << (sizeof(REQ_HDR_28) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_val_short=" << (sizeof(REQ_VAL_SHORT) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_val_medium=" << (sizeof(REQ_VAL_MEDIUM) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_val_long=" << (sizeof(REQ_VAL_LONG) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_val_xlong=" << (sizeof(REQ_VAL_XLONG) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_case_lower=" << (sizeof(REQ_CASE_LOWER) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_case_mixed=" << (sizeof(REQ_CASE_MIXED) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_real_browser=" << (sizeof(REQ_REAL_BROWSER) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_real_api=" << (sizeof(REQ_REAL_API) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_real_mobile=" << (sizeof(REQ_REAL_MOBILE) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_minimal=" << (sizeof(REQ_MINIMAL) - 1) << std::endl;
    std::cout << "DATA_SIZE: lc_minimal_host=" << (sizeof(REQ_MINIMAL_HOST) - 1) << std::endl;
    std::cout << std::endl;

    // Category 1: Header Count with lowercase conversion
    BENCHMARK("lc_hdr_8") { return bench_hwire_lc(REQ_HDR_8, sizeof(REQ_HDR_8) - 1); };
    BENCHMARK("lc_hdr_15") { return bench_hwire_lc(REQ_HDR_15, sizeof(REQ_HDR_15) - 1); };
    BENCHMARK("lc_hdr_20") { return bench_hwire_lc(REQ_HDR_20, sizeof(REQ_HDR_20) - 1); };
    BENCHMARK("lc_hdr_28") { return bench_hwire_lc(REQ_HDR_28, sizeof(REQ_HDR_28) - 1); };

    // Category 2: Header Value Length with lowercase conversion
    BENCHMARK("lc_val_short") { return bench_hwire_lc(REQ_VAL_SHORT, sizeof(REQ_VAL_SHORT) - 1); };
    BENCHMARK("lc_val_medium") { return bench_hwire_lc(REQ_VAL_MEDIUM, sizeof(REQ_VAL_MEDIUM) - 1); };
    BENCHMARK("lc_val_long") { return bench_hwire_lc(REQ_VAL_LONG, sizeof(REQ_VAL_LONG) - 1); };
    BENCHMARK("lc_val_xlong") { return bench_hwire_lc(REQ_VAL_XLONG, sizeof(REQ_VAL_XLONG) - 1); };

    // Category 3: Case Sensitivity with lowercase conversion
    BENCHMARK("lc_case_lower") { return bench_hwire_lc(REQ_CASE_LOWER, sizeof(REQ_CASE_LOWER) - 1); };
    BENCHMARK("lc_case_mixed") { return bench_hwire_lc(REQ_CASE_MIXED, sizeof(REQ_CASE_MIXED) - 1); };

    // Category 4: Real-World with lowercase conversion
    BENCHMARK("lc_real_browser") { return bench_hwire_lc(REQ_REAL_BROWSER, sizeof(REQ_REAL_BROWSER) - 1); };
    BENCHMARK("lc_real_api") { return bench_hwire_lc(REQ_REAL_API, sizeof(REQ_REAL_API) - 1); };
    BENCHMARK("lc_real_mobile") { return bench_hwire_lc(REQ_REAL_MOBILE, sizeof(REQ_REAL_MOBILE) - 1); };

    // Category 5: Baseline with lowercase conversion
    BENCHMARK("lc_minimal") { return bench_hwire_lc(REQ_MINIMAL, sizeof(REQ_MINIMAL) - 1); };
    BENCHMARK("lc_minimal_host") { return bench_hwire_lc(REQ_MINIMAL_HOST, sizeof(REQ_MINIMAL_HOST) - 1); };
}
