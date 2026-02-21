CC = clang
CXX = clang++
CFLAGS = -std=c99 -O2 -DNDEBUG
CXXFLAGS = -std=c++17 -O2 -DNDEBUG -DCATCH_CONFIG_ENABLE_BENCHMARKING

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

# Catch2 path
ifeq ($(UNAME_S),Darwin)
    CATCH2_PREFIX := $(shell brew --prefix catch2 2>/dev/null)
    CATCH2_INC := $(CATCH2_PREFIX)/include
    CATCH2_LIB := $(CATCH2_PREFIX)/lib
else
    CATCH2_INC := /usr/include
    CATCH2_LIB := /usr/lib/x86_64-linux-gnu
endif

# External dependency paths
PICO_DIR     := deps/picohttpparser
LLHTTP_DIR   := deps/llhttp
HTTPARSE_DIR := deps/httparse_bench

INCLUDES = -I../src -I$(PICO_DIR) -I$(LLHTTP_DIR) -I$(CATCH2_INC)
LDFLAGS  = -L$(CATCH2_LIB) -lCatch2Main -lCatch2

CARGO := $(shell which cargo 2>/dev/null || echo $$HOME/.cargo/bin/cargo)

LLHTTP_SRC = $(LLHTTP_DIR)/build/c/llhttp.c \
             $(LLHTTP_DIR)/src/native/api.c \
             $(LLHTTP_DIR)/src/native/http.c

# External dependency versions
PICO_COMMIT := f8326098f63eefabfa2b6ec595d90e9ed5ed958a
LLHTTP_TAG  := v9.3.1

# =============================================================================
# Architecture-specific benchmark targets
# =============================================================================

ifeq ($(UNAME_M),x86_64)
    HWIRE_TARGETS       = bench_hwire_avx2 bench_hwire_sse42 bench_hwire_sse2 bench_hwire_nosimd
    HWIRE_RESP_TARGETS  = bench_hwire_resp_avx2 bench_hwire_resp_sse42 bench_hwire_resp_sse2 bench_hwire_resp_nosimd
    PICO_TARGETS        = bench_pico_sse42 bench_pico_nosimd
    PICO_RESP_TARGETS   = bench_pico_resp_sse42 bench_pico_resp_nosimd
    LLHTTP_TARGETS      = bench_llhttp_sse42 bench_llhttp_nosimd
    LLHTTP_RESP_TARGETS = bench_llhttp_resp_sse42 bench_llhttp_resp_nosimd
else
    HWIRE_TARGETS       = bench_hwire_neon bench_hwire_nosimd
    HWIRE_RESP_TARGETS  = bench_hwire_resp_neon bench_hwire_resp_nosimd
    PICO_TARGETS        = bench_pico_nosimd
    PICO_RESP_TARGETS   = bench_pico_resp_nosimd
    LLHTTP_TARGETS      = bench_llhttp_neon bench_llhttp_nosimd
    LLHTTP_RESP_TARGETS = bench_llhttp_resp_neon bench_llhttp_resp_nosimd
endif

HTTPARSE_TARGETS      = bench_httparse_simd
HTTPARSE_RESP_TARGETS = bench_httparse_resp_simd

ALL_REQ_TARGETS  = $(HWIRE_TARGETS) $(PICO_TARGETS) $(LLHTTP_TARGETS) $(HTTPARSE_TARGETS)
ALL_RESP_TARGETS = $(HWIRE_RESP_TARGETS) $(PICO_RESP_TARGETS) $(LLHTTP_RESP_TARGETS) $(HTTPARSE_RESP_TARGETS)

# =============================================================================
# Phony targets
# =============================================================================

.PHONY: all clean dist-clean \
        deps deps-pico deps-llhttp deps-httparse deps-hwire deps-npm \
        patch unpatch \
        run run-hwire run-pico run-llhttp run-httparse \
        run-resp run-hwire-resp run-pico-resp run-llhttp-resp run-httparse-resp \
        report report-resp \
        bench bench-hwire bench-pico bench-llhttp bench-httparse \
        bench-resp bench-hwire-resp bench-pico-resp bench-llhttp-resp bench-httparse-resp
# =============================================================================
# Top-level targets
# =============================================================================

all: deps patch $(ALL_REQ_TARGETS) $(ALL_RESP_TARGETS)

clean:
	$(MAKE) unpatch
	rm -f $(ALL_REQ_TARGETS) $(ALL_RESP_TARGETS) libhttparse_bench.a *.o
	rm -rf results/

dist-clean: clean
	rm -rf deps/

# =============================================================================
# Dependency setup
# =============================================================================

deps: deps-pico deps-llhttp deps-httparse deps-hwire deps-npm

deps-npm:
	@if [ ! -d "node_modules/fast-xml-parser" ]; then \
		echo "Installing fast-xml-parser..."; \
		npm install fast-xml-parser; \
	fi

deps-pico:
	@if [ ! -d "$(PICO_DIR)" ]; then \
		echo "Cloning picohttpparser ($(PICO_COMMIT))..."; \
		mkdir -p deps && git clone https://github.com/h2o/picohttpparser.git $(PICO_DIR) && \
		cd $(PICO_DIR) && git checkout $(PICO_COMMIT); \
	fi

deps-llhttp:
	@if [ ! -d "$(LLHTTP_DIR)" ]; then \
		echo "Cloning llhttp ($(LLHTTP_TAG))..."; \
		mkdir -p deps && git clone --depth 1 --branch $(LLHTTP_TAG) https://github.com/nodejs/llhttp.git $(LLHTTP_DIR); \
	fi
	@if [ ! -f "$(LLHTTP_DIR)/build/c/llhttp.c" ]; then \
		echo "Building llhttp..."; \
		cd $(LLHTTP_DIR) && npm install && make generate; \
	fi

deps-httparse:
	@if [ ! -f "$(HTTPARSE_DIR)/Cargo.toml" ]; then \
		echo "Setting up httparse_bench..."; \
		./scripts/setup_httparse.sh; \
	fi

deps-hwire:
	@echo "Copying hwire source..."
	@mkdir -p deps/hwire
	@cp ../src/hwire.c deps/hwire/hwire.c

patch:
	@echo "Applying NO_SIMD patches..."
	@./scripts/apply-no-simd.sh hwire
	@./scripts/apply-no-simd.sh pico
	@./scripts/apply-no-simd.sh llhttp

unpatch:
	@echo "Restoring original files..."
	@./scripts/restore-no-simd.sh hwire
	@./scripts/restore-no-simd.sh pico
	@./scripts/restore-no-simd.sh llhttp

# =============================================================================
# Build: request benchmarks
# =============================================================================

bench_hwire_avx2: bench_hwire.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) -mavx2 -mfma $(INCLUDES) -c -o hwire_avx2.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) -mavx2 -mfma $(INCLUDES) -o $@ bench_hwire.cc hwire_avx2.o $(LDFLAGS)

bench_hwire_sse42: bench_hwire.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -c -o hwire_sse42.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) -msse4.2 $(INCLUDES) -o $@ bench_hwire.cc hwire_sse42.o $(LDFLAGS)

bench_hwire_sse2: bench_hwire.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o hwire_sse2.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ bench_hwire.cc hwire_sse2.o $(LDFLAGS)

bench_hwire_neon: bench_hwire.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o hwire_neon.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ bench_hwire.cc hwire_neon.o $(LDFLAGS)

bench_hwire_nosimd: bench_hwire.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -c -o hwire_nosimd.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) -DNO_SIMD $(INCLUDES) -o $@ bench_hwire.cc hwire_nosimd.o $(LDFLAGS)

bench_pico_sse42: bench_pico.cc $(PICO_DIR)/picohttpparser.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -c -o pico_sse42.o $(PICO_DIR)/picohttpparser.c
	$(CXX) $(CXXFLAGS) -msse4.2 $(INCLUDES) -o $@ bench_pico.cc pico_sse42.o $(LDFLAGS)

bench_pico_nosimd: bench_pico.cc $(PICO_DIR)/picohttpparser.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -c -o pico_nosimd.o $(PICO_DIR)/picohttpparser.c
	$(CXX) $(CXXFLAGS) -DNO_SIMD $(INCLUDES) -o $@ bench_pico.cc pico_nosimd.o $(LDFLAGS)

bench_llhttp_sse42: bench_llhttp.cc $(LLHTTP_SRC)
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o llhttp_sse42.o $(LLHTTP_DIR)/build/c/llhttp.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o api_sse42.o $(LLHTTP_DIR)/src/native/api.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o http_sse42.o $(LLHTTP_DIR)/src/native/http.c
	$(CXX) $(CXXFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -o $@ bench_llhttp.cc llhttp_sse42.o api_sse42.o http_sse42.o $(LDFLAGS)

bench_llhttp_neon: bench_llhttp.cc $(LLHTTP_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o llhttp_neon.o $(LLHTTP_DIR)/build/c/llhttp.c
	$(CC) $(CFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o api_neon.o $(LLHTTP_DIR)/src/native/api.c
	$(CC) $(CFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o http_neon.o $(LLHTTP_DIR)/src/native/http.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -o $@ bench_llhttp.cc llhttp_neon.o api_neon.o http_neon.o $(LDFLAGS)

bench_llhttp_nosimd: bench_llhttp.cc $(LLHTTP_SRC)
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o llhttp_nosimd.o $(LLHTTP_DIR)/build/c/llhttp.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o api_nosimd.o $(LLHTTP_DIR)/src/native/api.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o http_nosimd.o $(LLHTTP_DIR)/src/native/http.c
	$(CXX) $(CXXFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -o $@ bench_llhttp.cc llhttp_nosimd.o api_nosimd.o http_nosimd.o $(LDFLAGS)

libhttparse_bench.a: $(HTTPARSE_DIR)/src/lib.rs
	@if [ ! -f "$(CARGO)" ]; then \
		echo "Error: cargo not found. Please install Rust: curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"; \
		exit 1; \
	fi
	cd $(HTTPARSE_DIR) && $(CARGO) build --release
	cp $(HTTPARSE_DIR)/target/release/libhttparse_bench.a ./

bench_httparse_simd: bench_httparse.cc libhttparse_bench.a
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ bench_httparse.cc libhttparse_bench.a -lpthread -ldl -lm $(LDFLAGS)

# =============================================================================
# Build: response benchmarks
# =============================================================================

bench_hwire_resp_avx2: bench_hwire_resp.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) -mavx2 -mfma $(INCLUDES) -c -o hwire_resp_avx2.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) -mavx2 -mfma $(INCLUDES) -o $@ bench_hwire_resp.cc hwire_resp_avx2.o $(LDFLAGS)

bench_hwire_resp_sse42: bench_hwire_resp.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -c -o hwire_resp_sse42.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) -msse4.2 $(INCLUDES) -o $@ bench_hwire_resp.cc hwire_resp_sse42.o $(LDFLAGS)

bench_hwire_resp_sse2: bench_hwire_resp.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o hwire_resp_sse2.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ bench_hwire_resp.cc hwire_resp_sse2.o $(LDFLAGS)

bench_hwire_resp_neon: bench_hwire_resp.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o hwire_resp_neon.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ bench_hwire_resp.cc hwire_resp_neon.o $(LDFLAGS)

bench_hwire_resp_nosimd: bench_hwire_resp.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -c -o hwire_resp_nosimd.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) -DNO_SIMD $(INCLUDES) -o $@ bench_hwire_resp.cc hwire_resp_nosimd.o $(LDFLAGS)

bench_pico_resp_sse42: bench_pico_resp.cc $(PICO_DIR)/picohttpparser.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -c -o pico_resp_sse42.o $(PICO_DIR)/picohttpparser.c
	$(CXX) $(CXXFLAGS) -msse4.2 $(INCLUDES) -o $@ bench_pico_resp.cc pico_resp_sse42.o $(LDFLAGS)

bench_pico_resp_nosimd: bench_pico_resp.cc $(PICO_DIR)/picohttpparser.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -c -o pico_resp_nosimd.o $(PICO_DIR)/picohttpparser.c
	$(CXX) $(CXXFLAGS) -DNO_SIMD $(INCLUDES) -o $@ bench_pico_resp.cc pico_resp_nosimd.o $(LDFLAGS)

bench_llhttp_resp_sse42: bench_llhttp_resp.cc $(LLHTTP_SRC)
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o llhttp_resp_sse42.o $(LLHTTP_DIR)/build/c/llhttp.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o api_resp_sse42.o $(LLHTTP_DIR)/src/native/api.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o http_resp_sse42.o $(LLHTTP_DIR)/src/native/http.c
	$(CXX) $(CXXFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -o $@ bench_llhttp_resp.cc llhttp_resp_sse42.o api_resp_sse42.o http_resp_sse42.o $(LDFLAGS)

bench_llhttp_resp_neon: bench_llhttp_resp.cc $(LLHTTP_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o llhttp_resp_neon.o $(LLHTTP_DIR)/build/c/llhttp.c
	$(CC) $(CFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o api_resp_neon.o $(LLHTTP_DIR)/src/native/api.c
	$(CC) $(CFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o http_resp_neon.o $(LLHTTP_DIR)/src/native/http.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -o $@ bench_llhttp_resp.cc llhttp_resp_neon.o api_resp_neon.o http_resp_neon.o $(LDFLAGS)

bench_llhttp_resp_nosimd: bench_llhttp_resp.cc $(LLHTTP_SRC)
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o llhttp_resp_nosimd.o $(LLHTTP_DIR)/build/c/llhttp.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o api_resp_nosimd.o $(LLHTTP_DIR)/src/native/api.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o http_resp_nosimd.o $(LLHTTP_DIR)/src/native/http.c
	$(CXX) $(CXXFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -o $@ bench_llhttp_resp.cc llhttp_resp_nosimd.o api_resp_nosimd.o http_resp_nosimd.o $(LDFLAGS)

bench_httparse_resp_simd: bench_httparse_resp.cc libhttparse_bench.a
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ bench_httparse_resp.cc libhttparse_bench.a -lpthread -ldl -lm $(LDFLAGS)


# =============================================================================
# Run targets: req benchmarks
# =============================================================================

.PHONY: run-hwire-req-header-count-8-headers
run-hwire-req-header-count-8-headers: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_header_count_8_headers.jsonl \
		"[header-count][8-headers]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-header-count-15-headers
run-hwire-req-header-count-15-headers: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_header_count_15_headers.jsonl \
		"[header-count][15-headers]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-header-count-20-headers
run-hwire-req-header-count-20-headers: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_header_count_20_headers.jsonl \
		"[header-count][20-headers]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-header-count-28-headers
run-hwire-req-header-count-28-headers: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_header_count_28_headers.jsonl \
		"[header-count][28-headers]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-header-value-length-short-values
run-hwire-req-header-value-length-short-values: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_header_value_length_short_values.jsonl \
		"[header-value-length][short-values]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-header-value-length-medium-values
run-hwire-req-header-value-length-medium-values: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_header_value_length_medium_values.jsonl \
		"[header-value-length][medium-values]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-header-value-length-long-values
run-hwire-req-header-value-length-long-values: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_header_value_length_long_values.jsonl \
		"[header-value-length][long-values]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-header-value-length-extra-long-values
run-hwire-req-header-value-length-extra-long-values: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_header_value_length_extra_long_values.jsonl \
		"[header-value-length][extra-long-values]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-case-sensitivity-all-lowercase
run-hwire-req-case-sensitivity-all-lowercase: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_case_sensitivity_all_lowercase.jsonl \
		"[case-sensitivity][all-lowercase]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-case-sensitivity-mixed-case
run-hwire-req-case-sensitivity-mixed-case: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_case_sensitivity_mixed_case.jsonl \
		"[case-sensitivity][mixed-case]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-real-world-browser
run-hwire-req-real-world-browser: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_real_world_browser.jsonl \
		"[real-world][browser]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-real-world-rest-api
run-hwire-req-real-world-rest-api: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_real_world_rest_api.jsonl \
		"[real-world][rest-api]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-real-world-mobile-app
run-hwire-req-real-world-mobile-app: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_real_world_mobile_app.jsonl \
		"[real-world][mobile-app]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-baseline-no-headers
run-hwire-req-baseline-no-headers: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_baseline_no_headers.jsonl \
		"[baseline][no-headers]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-baseline-host-only
run-hwire-req-baseline-host-only: deps patch $(HWIRE_TARGETS)
	@bash scripts/run-bench.sh results/req_hwire_baseline_host_only.jsonl \
		"[baseline][host-only]" $(HWIRE_TARGETS)

.PHONY: run-hwire-req-header-count
run-hwire-req-header-count: run-hwire-req-header-count-8-headers \
		run-hwire-req-header-count-15-headers \
		run-hwire-req-header-count-20-headers \
		run-hwire-req-header-count-28-headers

.PHONY: run-hwire-req-header-value-length
run-hwire-req-header-value-length: run-hwire-req-header-value-length-short-values \
		run-hwire-req-header-value-length-medium-values \
		run-hwire-req-header-value-length-long-values \
		run-hwire-req-header-value-length-extra-long-values

.PHONY: run-hwire-req-case-sensitivity
run-hwire-req-case-sensitivity: run-hwire-req-case-sensitivity-all-lowercase \
		run-hwire-req-case-sensitivity-mixed-case

.PHONY: run-hwire-req-real-world
run-hwire-req-real-world: run-hwire-req-real-world-browser \
		run-hwire-req-real-world-rest-api \
		run-hwire-req-real-world-mobile-app

.PHONY: run-hwire-req-baseline
run-hwire-req-baseline: run-hwire-req-baseline-no-headers \
		run-hwire-req-baseline-host-only

.PHONY: run-hwire-req
run-hwire-req: run-hwire-req-header-count \
		run-hwire-req-header-value-length \
		run-hwire-req-case-sensitivity \
		run-hwire-req-real-world \
		run-hwire-req-baseline

.PHONY: run-pico-req-header-count-8-headers
run-pico-req-header-count-8-headers: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_header_count_8_headers.jsonl \
		"[header-count][8-headers]" $(PICO_TARGETS)

.PHONY: run-pico-req-header-count-15-headers
run-pico-req-header-count-15-headers: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_header_count_15_headers.jsonl \
		"[header-count][15-headers]" $(PICO_TARGETS)

.PHONY: run-pico-req-header-count-20-headers
run-pico-req-header-count-20-headers: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_header_count_20_headers.jsonl \
		"[header-count][20-headers]" $(PICO_TARGETS)

.PHONY: run-pico-req-header-count-28-headers
run-pico-req-header-count-28-headers: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_header_count_28_headers.jsonl \
		"[header-count][28-headers]" $(PICO_TARGETS)

.PHONY: run-pico-req-header-value-length-short-values
run-pico-req-header-value-length-short-values: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_header_value_length_short_values.jsonl \
		"[header-value-length][short-values]" $(PICO_TARGETS)

.PHONY: run-pico-req-header-value-length-medium-values
run-pico-req-header-value-length-medium-values: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_header_value_length_medium_values.jsonl \
		"[header-value-length][medium-values]" $(PICO_TARGETS)

.PHONY: run-pico-req-header-value-length-long-values
run-pico-req-header-value-length-long-values: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_header_value_length_long_values.jsonl \
		"[header-value-length][long-values]" $(PICO_TARGETS)

.PHONY: run-pico-req-header-value-length-extra-long-values
run-pico-req-header-value-length-extra-long-values: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_header_value_length_extra_long_values.jsonl \
		"[header-value-length][extra-long-values]" $(PICO_TARGETS)

.PHONY: run-pico-req-case-sensitivity-all-lowercase
run-pico-req-case-sensitivity-all-lowercase: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_case_sensitivity_all_lowercase.jsonl \
		"[case-sensitivity][all-lowercase]" $(PICO_TARGETS)

.PHONY: run-pico-req-case-sensitivity-mixed-case
run-pico-req-case-sensitivity-mixed-case: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_case_sensitivity_mixed_case.jsonl \
		"[case-sensitivity][mixed-case]" $(PICO_TARGETS)

.PHONY: run-pico-req-real-world-browser
run-pico-req-real-world-browser: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_real_world_browser.jsonl \
		"[real-world][browser]" $(PICO_TARGETS)

.PHONY: run-pico-req-real-world-rest-api
run-pico-req-real-world-rest-api: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_real_world_rest_api.jsonl \
		"[real-world][rest-api]" $(PICO_TARGETS)

.PHONY: run-pico-req-real-world-mobile-app
run-pico-req-real-world-mobile-app: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_real_world_mobile_app.jsonl \
		"[real-world][mobile-app]" $(PICO_TARGETS)

.PHONY: run-pico-req-baseline-no-headers
run-pico-req-baseline-no-headers: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_baseline_no_headers.jsonl \
		"[baseline][no-headers]" $(PICO_TARGETS)

.PHONY: run-pico-req-baseline-host-only
run-pico-req-baseline-host-only: deps patch $(PICO_TARGETS)
	@bash scripts/run-bench.sh results/req_pico_baseline_host_only.jsonl \
		"[baseline][host-only]" $(PICO_TARGETS)

.PHONY: run-pico-req-header-count
run-pico-req-header-count: run-pico-req-header-count-8-headers \
		run-pico-req-header-count-15-headers \
		run-pico-req-header-count-20-headers \
		run-pico-req-header-count-28-headers

.PHONY: run-pico-req-header-value-length
run-pico-req-header-value-length: run-pico-req-header-value-length-short-values \
		run-pico-req-header-value-length-medium-values \
		run-pico-req-header-value-length-long-values \
		run-pico-req-header-value-length-extra-long-values

.PHONY: run-pico-req-case-sensitivity
run-pico-req-case-sensitivity: run-pico-req-case-sensitivity-all-lowercase \
		run-pico-req-case-sensitivity-mixed-case

.PHONY: run-pico-req-real-world
run-pico-req-real-world: run-pico-req-real-world-browser \
		run-pico-req-real-world-rest-api \
		run-pico-req-real-world-mobile-app

.PHONY: run-pico-req-baseline
run-pico-req-baseline: run-pico-req-baseline-no-headers \
		run-pico-req-baseline-host-only

.PHONY: run-pico-req
run-pico-req: run-pico-req-header-count \
		run-pico-req-header-value-length \
		run-pico-req-case-sensitivity \
		run-pico-req-real-world \
		run-pico-req-baseline

.PHONY: run-llhttp-req-header-count-8-headers
run-llhttp-req-header-count-8-headers: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_header_count_8_headers.jsonl \
		"[header-count][8-headers]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-header-count-15-headers
run-llhttp-req-header-count-15-headers: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_header_count_15_headers.jsonl \
		"[header-count][15-headers]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-header-count-20-headers
run-llhttp-req-header-count-20-headers: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_header_count_20_headers.jsonl \
		"[header-count][20-headers]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-header-count-28-headers
run-llhttp-req-header-count-28-headers: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_header_count_28_headers.jsonl \
		"[header-count][28-headers]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-header-value-length-short-values
run-llhttp-req-header-value-length-short-values: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_header_value_length_short_values.jsonl \
		"[header-value-length][short-values]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-header-value-length-medium-values
run-llhttp-req-header-value-length-medium-values: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_header_value_length_medium_values.jsonl \
		"[header-value-length][medium-values]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-header-value-length-long-values
run-llhttp-req-header-value-length-long-values: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_header_value_length_long_values.jsonl \
		"[header-value-length][long-values]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-header-value-length-extra-long-values
run-llhttp-req-header-value-length-extra-long-values: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_header_value_length_extra_long_values.jsonl \
		"[header-value-length][extra-long-values]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-case-sensitivity-all-lowercase
run-llhttp-req-case-sensitivity-all-lowercase: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_case_sensitivity_all_lowercase.jsonl \
		"[case-sensitivity][all-lowercase]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-case-sensitivity-mixed-case
run-llhttp-req-case-sensitivity-mixed-case: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_case_sensitivity_mixed_case.jsonl \
		"[case-sensitivity][mixed-case]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-real-world-browser
run-llhttp-req-real-world-browser: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_real_world_browser.jsonl \
		"[real-world][browser]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-real-world-rest-api
run-llhttp-req-real-world-rest-api: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_real_world_rest_api.jsonl \
		"[real-world][rest-api]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-real-world-mobile-app
run-llhttp-req-real-world-mobile-app: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_real_world_mobile_app.jsonl \
		"[real-world][mobile-app]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-baseline-no-headers
run-llhttp-req-baseline-no-headers: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_baseline_no_headers.jsonl \
		"[baseline][no-headers]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-baseline-host-only
run-llhttp-req-baseline-host-only: deps patch $(LLHTTP_TARGETS)
	@bash scripts/run-bench.sh results/req_llhttp_baseline_host_only.jsonl \
		"[baseline][host-only]" $(LLHTTP_TARGETS)

.PHONY: run-llhttp-req-header-count
run-llhttp-req-header-count: run-llhttp-req-header-count-8-headers \
		run-llhttp-req-header-count-15-headers \
		run-llhttp-req-header-count-20-headers \
		run-llhttp-req-header-count-28-headers

.PHONY: run-llhttp-req-header-value-length
run-llhttp-req-header-value-length: run-llhttp-req-header-value-length-short-values \
		run-llhttp-req-header-value-length-medium-values \
		run-llhttp-req-header-value-length-long-values \
		run-llhttp-req-header-value-length-extra-long-values

.PHONY: run-llhttp-req-case-sensitivity
run-llhttp-req-case-sensitivity: run-llhttp-req-case-sensitivity-all-lowercase \
		run-llhttp-req-case-sensitivity-mixed-case

.PHONY: run-llhttp-req-real-world
run-llhttp-req-real-world: run-llhttp-req-real-world-browser \
		run-llhttp-req-real-world-rest-api \
		run-llhttp-req-real-world-mobile-app

.PHONY: run-llhttp-req-baseline
run-llhttp-req-baseline: run-llhttp-req-baseline-no-headers \
		run-llhttp-req-baseline-host-only

.PHONY: run-llhttp-req
run-llhttp-req: run-llhttp-req-header-count \
		run-llhttp-req-header-value-length \
		run-llhttp-req-case-sensitivity \
		run-llhttp-req-real-world \
		run-llhttp-req-baseline

.PHONY: run-httparse-req-header-count-8-headers
run-httparse-req-header-count-8-headers: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_header_count_8_headers.jsonl \
		"[header-count][8-headers]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-header-count-15-headers
run-httparse-req-header-count-15-headers: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_header_count_15_headers.jsonl \
		"[header-count][15-headers]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-header-count-20-headers
run-httparse-req-header-count-20-headers: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_header_count_20_headers.jsonl \
		"[header-count][20-headers]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-header-count-28-headers
run-httparse-req-header-count-28-headers: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_header_count_28_headers.jsonl \
		"[header-count][28-headers]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-header-value-length-short-values
run-httparse-req-header-value-length-short-values: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_header_value_length_short_values.jsonl \
		"[header-value-length][short-values]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-header-value-length-medium-values
run-httparse-req-header-value-length-medium-values: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_header_value_length_medium_values.jsonl \
		"[header-value-length][medium-values]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-header-value-length-long-values
run-httparse-req-header-value-length-long-values: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_header_value_length_long_values.jsonl \
		"[header-value-length][long-values]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-header-value-length-extra-long-values
run-httparse-req-header-value-length-extra-long-values: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_header_value_length_extra_long_values.jsonl \
		"[header-value-length][extra-long-values]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-case-sensitivity-all-lowercase
run-httparse-req-case-sensitivity-all-lowercase: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_case_sensitivity_all_lowercase.jsonl \
		"[case-sensitivity][all-lowercase]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-case-sensitivity-mixed-case
run-httparse-req-case-sensitivity-mixed-case: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_case_sensitivity_mixed_case.jsonl \
		"[case-sensitivity][mixed-case]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-real-world-browser
run-httparse-req-real-world-browser: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_real_world_browser.jsonl \
		"[real-world][browser]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-real-world-rest-api
run-httparse-req-real-world-rest-api: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_real_world_rest_api.jsonl \
		"[real-world][rest-api]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-real-world-mobile-app
run-httparse-req-real-world-mobile-app: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_real_world_mobile_app.jsonl \
		"[real-world][mobile-app]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-baseline-no-headers
run-httparse-req-baseline-no-headers: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_baseline_no_headers.jsonl \
		"[baseline][no-headers]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-baseline-host-only
run-httparse-req-baseline-host-only: deps $(HTTPARSE_TARGETS)
	@bash scripts/run-bench.sh results/req_httparse_baseline_host_only.jsonl \
		"[baseline][host-only]" $(HTTPARSE_TARGETS)

.PHONY: run-httparse-req-header-count
run-httparse-req-header-count: run-httparse-req-header-count-8-headers \
		run-httparse-req-header-count-15-headers \
		run-httparse-req-header-count-20-headers \
		run-httparse-req-header-count-28-headers

.PHONY: run-httparse-req-header-value-length
run-httparse-req-header-value-length: run-httparse-req-header-value-length-short-values \
		run-httparse-req-header-value-length-medium-values \
		run-httparse-req-header-value-length-long-values \
		run-httparse-req-header-value-length-extra-long-values

.PHONY: run-httparse-req-case-sensitivity
run-httparse-req-case-sensitivity: run-httparse-req-case-sensitivity-all-lowercase \
		run-httparse-req-case-sensitivity-mixed-case

.PHONY: run-httparse-req-real-world
run-httparse-req-real-world: run-httparse-req-real-world-browser \
		run-httparse-req-real-world-rest-api \
		run-httparse-req-real-world-mobile-app

.PHONY: run-httparse-req-baseline
run-httparse-req-baseline: run-httparse-req-baseline-no-headers \
		run-httparse-req-baseline-host-only

.PHONY: run-httparse-req
run-httparse-req: run-httparse-req-header-count \
		run-httparse-req-header-value-length \
		run-httparse-req-case-sensitivity \
		run-httparse-req-real-world \
		run-httparse-req-baseline

# =============================================================================
# Run targets: resp benchmarks
# =============================================================================

.PHONY: run-hwire-resp-header-count-4-headers
run-hwire-resp-header-count-4-headers: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_header_count_4_headers.jsonl \
		"[header-count][4-headers]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-header-count-8-headers
run-hwire-resp-header-count-8-headers: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_header_count_8_headers.jsonl \
		"[header-count][8-headers]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-header-count-12-headers
run-hwire-resp-header-count-12-headers: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_header_count_12_headers.jsonl \
		"[header-count][12-headers]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-header-count-20-headers
run-hwire-resp-header-count-20-headers: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_header_count_20_headers.jsonl \
		"[header-count][20-headers]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-header-value-length-short-values
run-hwire-resp-header-value-length-short-values: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_header_value_length_short_values.jsonl \
		"[header-value-length][short-values]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-header-value-length-medium-values
run-hwire-resp-header-value-length-medium-values: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_header_value_length_medium_values.jsonl \
		"[header-value-length][medium-values]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-header-value-length-long-values
run-hwire-resp-header-value-length-long-values: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_header_value_length_long_values.jsonl \
		"[header-value-length][long-values]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-header-value-length-extra-long-values
run-hwire-resp-header-value-length-extra-long-values: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_header_value_length_extra_long_values.jsonl \
		"[header-value-length][extra-long-values]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-case-sensitivity-all-lowercase
run-hwire-resp-case-sensitivity-all-lowercase: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_case_sensitivity_all_lowercase.jsonl \
		"[case-sensitivity][all-lowercase]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-case-sensitivity-mixed-case
run-hwire-resp-case-sensitivity-mixed-case: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_case_sensitivity_mixed_case.jsonl \
		"[case-sensitivity][mixed-case]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-real-world-html-page
run-hwire-resp-real-world-html-page: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_real_world_html_page.jsonl \
		"[real-world][html-page]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-real-world-rest-api
run-hwire-resp-real-world-rest-api: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_real_world_rest_api.jsonl \
		"[real-world][rest-api]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-real-world-static-file
run-hwire-resp-real-world-static-file: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_real_world_static_file.jsonl \
		"[real-world][static-file]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-baseline-no-extra-headers
run-hwire-resp-baseline-no-extra-headers: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_baseline_no_extra_headers.jsonl \
		"[baseline][no-extra-headers]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-baseline-date-header-only
run-hwire-resp-baseline-date-header-only: deps patch $(HWIRE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_hwire_baseline_date_header_only.jsonl \
		"[baseline][date-header-only]" $(HWIRE_RESP_TARGETS)

.PHONY: run-hwire-resp-header-count
run-hwire-resp-header-count: run-hwire-resp-header-count-4-headers \
		run-hwire-resp-header-count-8-headers \
		run-hwire-resp-header-count-12-headers \
		run-hwire-resp-header-count-20-headers

.PHONY: run-hwire-resp-header-value-length
run-hwire-resp-header-value-length: run-hwire-resp-header-value-length-short-values \
		run-hwire-resp-header-value-length-medium-values \
		run-hwire-resp-header-value-length-long-values \
		run-hwire-resp-header-value-length-extra-long-values

.PHONY: run-hwire-resp-case-sensitivity
run-hwire-resp-case-sensitivity: run-hwire-resp-case-sensitivity-all-lowercase \
		run-hwire-resp-case-sensitivity-mixed-case

.PHONY: run-hwire-resp-real-world
run-hwire-resp-real-world: run-hwire-resp-real-world-html-page \
		run-hwire-resp-real-world-rest-api \
		run-hwire-resp-real-world-static-file

.PHONY: run-hwire-resp-baseline
run-hwire-resp-baseline: run-hwire-resp-baseline-no-extra-headers \
		run-hwire-resp-baseline-date-header-only

.PHONY: run-hwire-resp
run-hwire-resp: run-hwire-resp-header-count \
		run-hwire-resp-header-value-length \
		run-hwire-resp-case-sensitivity \
		run-hwire-resp-real-world \
		run-hwire-resp-baseline

.PHONY: run-pico-resp-header-count-4-headers
run-pico-resp-header-count-4-headers: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_header_count_4_headers.jsonl \
		"[header-count][4-headers]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-header-count-8-headers
run-pico-resp-header-count-8-headers: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_header_count_8_headers.jsonl \
		"[header-count][8-headers]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-header-count-12-headers
run-pico-resp-header-count-12-headers: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_header_count_12_headers.jsonl \
		"[header-count][12-headers]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-header-count-20-headers
run-pico-resp-header-count-20-headers: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_header_count_20_headers.jsonl \
		"[header-count][20-headers]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-header-value-length-short-values
run-pico-resp-header-value-length-short-values: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_header_value_length_short_values.jsonl \
		"[header-value-length][short-values]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-header-value-length-medium-values
run-pico-resp-header-value-length-medium-values: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_header_value_length_medium_values.jsonl \
		"[header-value-length][medium-values]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-header-value-length-long-values
run-pico-resp-header-value-length-long-values: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_header_value_length_long_values.jsonl \
		"[header-value-length][long-values]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-header-value-length-extra-long-values
run-pico-resp-header-value-length-extra-long-values: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_header_value_length_extra_long_values.jsonl \
		"[header-value-length][extra-long-values]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-case-sensitivity-all-lowercase
run-pico-resp-case-sensitivity-all-lowercase: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_case_sensitivity_all_lowercase.jsonl \
		"[case-sensitivity][all-lowercase]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-case-sensitivity-mixed-case
run-pico-resp-case-sensitivity-mixed-case: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_case_sensitivity_mixed_case.jsonl \
		"[case-sensitivity][mixed-case]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-real-world-html-page
run-pico-resp-real-world-html-page: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_real_world_html_page.jsonl \
		"[real-world][html-page]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-real-world-rest-api
run-pico-resp-real-world-rest-api: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_real_world_rest_api.jsonl \
		"[real-world][rest-api]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-real-world-static-file
run-pico-resp-real-world-static-file: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_real_world_static_file.jsonl \
		"[real-world][static-file]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-baseline-no-extra-headers
run-pico-resp-baseline-no-extra-headers: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_baseline_no_extra_headers.jsonl \
		"[baseline][no-extra-headers]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-baseline-date-header-only
run-pico-resp-baseline-date-header-only: deps patch $(PICO_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_pico_baseline_date_header_only.jsonl \
		"[baseline][date-header-only]" $(PICO_RESP_TARGETS)

.PHONY: run-pico-resp-header-count
run-pico-resp-header-count: run-pico-resp-header-count-4-headers \
		run-pico-resp-header-count-8-headers \
		run-pico-resp-header-count-12-headers \
		run-pico-resp-header-count-20-headers

.PHONY: run-pico-resp-header-value-length
run-pico-resp-header-value-length: run-pico-resp-header-value-length-short-values \
		run-pico-resp-header-value-length-medium-values \
		run-pico-resp-header-value-length-long-values \
		run-pico-resp-header-value-length-extra-long-values

.PHONY: run-pico-resp-case-sensitivity
run-pico-resp-case-sensitivity: run-pico-resp-case-sensitivity-all-lowercase \
		run-pico-resp-case-sensitivity-mixed-case

.PHONY: run-pico-resp-real-world
run-pico-resp-real-world: run-pico-resp-real-world-html-page \
		run-pico-resp-real-world-rest-api \
		run-pico-resp-real-world-static-file

.PHONY: run-pico-resp-baseline
run-pico-resp-baseline: run-pico-resp-baseline-no-extra-headers \
		run-pico-resp-baseline-date-header-only

.PHONY: run-pico-resp
run-pico-resp: run-pico-resp-header-count \
		run-pico-resp-header-value-length \
		run-pico-resp-case-sensitivity \
		run-pico-resp-real-world \
		run-pico-resp-baseline

.PHONY: run-llhttp-resp-header-count-4-headers
run-llhttp-resp-header-count-4-headers: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_header_count_4_headers.jsonl \
		"[header-count][4-headers]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-header-count-8-headers
run-llhttp-resp-header-count-8-headers: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_header_count_8_headers.jsonl \
		"[header-count][8-headers]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-header-count-12-headers
run-llhttp-resp-header-count-12-headers: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_header_count_12_headers.jsonl \
		"[header-count][12-headers]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-header-count-20-headers
run-llhttp-resp-header-count-20-headers: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_header_count_20_headers.jsonl \
		"[header-count][20-headers]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-header-value-length-short-values
run-llhttp-resp-header-value-length-short-values: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_header_value_length_short_values.jsonl \
		"[header-value-length][short-values]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-header-value-length-medium-values
run-llhttp-resp-header-value-length-medium-values: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_header_value_length_medium_values.jsonl \
		"[header-value-length][medium-values]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-header-value-length-long-values
run-llhttp-resp-header-value-length-long-values: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_header_value_length_long_values.jsonl \
		"[header-value-length][long-values]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-header-value-length-extra-long-values
run-llhttp-resp-header-value-length-extra-long-values: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_header_value_length_extra_long_values.jsonl \
		"[header-value-length][extra-long-values]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-case-sensitivity-all-lowercase
run-llhttp-resp-case-sensitivity-all-lowercase: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_case_sensitivity_all_lowercase.jsonl \
		"[case-sensitivity][all-lowercase]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-case-sensitivity-mixed-case
run-llhttp-resp-case-sensitivity-mixed-case: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_case_sensitivity_mixed_case.jsonl \
		"[case-sensitivity][mixed-case]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-real-world-html-page
run-llhttp-resp-real-world-html-page: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_real_world_html_page.jsonl \
		"[real-world][html-page]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-real-world-rest-api
run-llhttp-resp-real-world-rest-api: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_real_world_rest_api.jsonl \
		"[real-world][rest-api]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-real-world-static-file
run-llhttp-resp-real-world-static-file: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_real_world_static_file.jsonl \
		"[real-world][static-file]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-baseline-no-extra-headers
run-llhttp-resp-baseline-no-extra-headers: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_baseline_no_extra_headers.jsonl \
		"[baseline][no-extra-headers]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-baseline-date-header-only
run-llhttp-resp-baseline-date-header-only: deps patch $(LLHTTP_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_llhttp_baseline_date_header_only.jsonl \
		"[baseline][date-header-only]" $(LLHTTP_RESP_TARGETS)

.PHONY: run-llhttp-resp-header-count
run-llhttp-resp-header-count: run-llhttp-resp-header-count-4-headers \
		run-llhttp-resp-header-count-8-headers \
		run-llhttp-resp-header-count-12-headers \
		run-llhttp-resp-header-count-20-headers

.PHONY: run-llhttp-resp-header-value-length
run-llhttp-resp-header-value-length: run-llhttp-resp-header-value-length-short-values \
		run-llhttp-resp-header-value-length-medium-values \
		run-llhttp-resp-header-value-length-long-values \
		run-llhttp-resp-header-value-length-extra-long-values

.PHONY: run-llhttp-resp-case-sensitivity
run-llhttp-resp-case-sensitivity: run-llhttp-resp-case-sensitivity-all-lowercase \
		run-llhttp-resp-case-sensitivity-mixed-case

.PHONY: run-llhttp-resp-real-world
run-llhttp-resp-real-world: run-llhttp-resp-real-world-html-page \
		run-llhttp-resp-real-world-rest-api \
		run-llhttp-resp-real-world-static-file

.PHONY: run-llhttp-resp-baseline
run-llhttp-resp-baseline: run-llhttp-resp-baseline-no-extra-headers \
		run-llhttp-resp-baseline-date-header-only

.PHONY: run-llhttp-resp
run-llhttp-resp: run-llhttp-resp-header-count \
		run-llhttp-resp-header-value-length \
		run-llhttp-resp-case-sensitivity \
		run-llhttp-resp-real-world \
		run-llhttp-resp-baseline

.PHONY: run-httparse-resp-header-count-4-headers
run-httparse-resp-header-count-4-headers: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_header_count_4_headers.jsonl \
		"[header-count][4-headers]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-header-count-8-headers
run-httparse-resp-header-count-8-headers: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_header_count_8_headers.jsonl \
		"[header-count][8-headers]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-header-count-12-headers
run-httparse-resp-header-count-12-headers: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_header_count_12_headers.jsonl \
		"[header-count][12-headers]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-header-count-20-headers
run-httparse-resp-header-count-20-headers: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_header_count_20_headers.jsonl \
		"[header-count][20-headers]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-header-value-length-short-values
run-httparse-resp-header-value-length-short-values: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_header_value_length_short_values.jsonl \
		"[header-value-length][short-values]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-header-value-length-medium-values
run-httparse-resp-header-value-length-medium-values: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_header_value_length_medium_values.jsonl \
		"[header-value-length][medium-values]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-header-value-length-long-values
run-httparse-resp-header-value-length-long-values: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_header_value_length_long_values.jsonl \
		"[header-value-length][long-values]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-header-value-length-extra-long-values
run-httparse-resp-header-value-length-extra-long-values: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_header_value_length_extra_long_values.jsonl \
		"[header-value-length][extra-long-values]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-case-sensitivity-all-lowercase
run-httparse-resp-case-sensitivity-all-lowercase: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_case_sensitivity_all_lowercase.jsonl \
		"[case-sensitivity][all-lowercase]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-case-sensitivity-mixed-case
run-httparse-resp-case-sensitivity-mixed-case: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_case_sensitivity_mixed_case.jsonl \
		"[case-sensitivity][mixed-case]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-real-world-html-page
run-httparse-resp-real-world-html-page: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_real_world_html_page.jsonl \
		"[real-world][html-page]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-real-world-rest-api
run-httparse-resp-real-world-rest-api: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_real_world_rest_api.jsonl \
		"[real-world][rest-api]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-real-world-static-file
run-httparse-resp-real-world-static-file: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_real_world_static_file.jsonl \
		"[real-world][static-file]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-baseline-no-extra-headers
run-httparse-resp-baseline-no-extra-headers: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_baseline_no_extra_headers.jsonl \
		"[baseline][no-extra-headers]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-baseline-date-header-only
run-httparse-resp-baseline-date-header-only: deps $(HTTPARSE_RESP_TARGETS)
	@bash scripts/run-bench.sh results/rsp_httparse_baseline_date_header_only.jsonl \
		"[baseline][date-header-only]" $(HTTPARSE_RESP_TARGETS)

.PHONY: run-httparse-resp-header-count
run-httparse-resp-header-count: run-httparse-resp-header-count-4-headers \
		run-httparse-resp-header-count-8-headers \
		run-httparse-resp-header-count-12-headers \
		run-httparse-resp-header-count-20-headers

.PHONY: run-httparse-resp-header-value-length
run-httparse-resp-header-value-length: run-httparse-resp-header-value-length-short-values \
		run-httparse-resp-header-value-length-medium-values \
		run-httparse-resp-header-value-length-long-values \
		run-httparse-resp-header-value-length-extra-long-values

.PHONY: run-httparse-resp-case-sensitivity
run-httparse-resp-case-sensitivity: run-httparse-resp-case-sensitivity-all-lowercase \
		run-httparse-resp-case-sensitivity-mixed-case

.PHONY: run-httparse-resp-real-world
run-httparse-resp-real-world: run-httparse-resp-real-world-html-page \
		run-httparse-resp-real-world-rest-api \
		run-httparse-resp-real-world-static-file

.PHONY: run-httparse-resp-baseline
run-httparse-resp-baseline: run-httparse-resp-baseline-no-extra-headers \
		run-httparse-resp-baseline-date-header-only

.PHONY: run-httparse-resp
run-httparse-resp: run-httparse-resp-header-count \
		run-httparse-resp-header-value-length \
		run-httparse-resp-case-sensitivity \
		run-httparse-resp-real-world \
		run-httparse-resp-baseline

# =============================================================================
# Cross-parser category run targets
# =============================================================================

.PHONY: run-req-header-count
run-req-header-count: run-hwire-req-header-count \
		run-pico-req-header-count \
		run-llhttp-req-header-count \
		run-httparse-req-header-count

.PHONY: run-req-header-value-length
run-req-header-value-length: run-hwire-req-header-value-length \
		run-pico-req-header-value-length \
		run-llhttp-req-header-value-length \
		run-httparse-req-header-value-length

.PHONY: run-req-case-sensitivity
run-req-case-sensitivity: run-hwire-req-case-sensitivity \
		run-pico-req-case-sensitivity \
		run-llhttp-req-case-sensitivity \
		run-httparse-req-case-sensitivity

.PHONY: run-req-real-world
run-req-real-world: run-hwire-req-real-world \
		run-pico-req-real-world \
		run-llhttp-req-real-world \
		run-httparse-req-real-world

.PHONY: run-req-baseline
run-req-baseline: run-hwire-req-baseline \
		run-pico-req-baseline \
		run-llhttp-req-baseline \
		run-httparse-req-baseline

.PHONY: run-resp-header-count
run-resp-header-count: run-hwire-resp-header-count \
		run-pico-resp-header-count \
		run-llhttp-resp-header-count \
		run-httparse-resp-header-count

.PHONY: run-resp-header-value-length
run-resp-header-value-length: run-hwire-resp-header-value-length \
		run-pico-resp-header-value-length \
		run-llhttp-resp-header-value-length \
		run-httparse-resp-header-value-length

.PHONY: run-resp-case-sensitivity
run-resp-case-sensitivity: run-hwire-resp-case-sensitivity \
		run-pico-resp-case-sensitivity \
		run-llhttp-resp-case-sensitivity \
		run-httparse-resp-case-sensitivity

.PHONY: run-resp-real-world
run-resp-real-world: run-hwire-resp-real-world \
		run-pico-resp-real-world \
		run-llhttp-resp-real-world \
		run-httparse-resp-real-world

.PHONY: run-resp-baseline
run-resp-baseline: run-hwire-resp-baseline \
		run-pico-resp-baseline \
		run-llhttp-resp-baseline \
		run-httparse-resp-baseline

# =============================================================================
# Top-level run targets
# =============================================================================

.PHONY: run run-resp
run: run-hwire-req \
		run-pico-req \
		run-llhttp-req \
		run-httparse-req

run-resp: run-hwire-resp \
		run-pico-resp \
		run-llhttp-resp \
		run-httparse-resp

# single-parser aliases (backward-compatible)
.PHONY: run-hwire run-pico run-llhttp run-httparse run-hwire-resp run-pico-resp run-llhttp-resp run-httparse-resp
run-hwire: run-hwire-req
run-pico: run-pico-req
run-llhttp: run-llhttp-req
run-httparse: run-httparse-req

# =============================================================================
# Report targets
# =============================================================================

.PHONY: report report-resp

report:
	@node report.js results/req_*.jsonl

report-resp:
	@node report.js results/rsp_*.jsonl

# =============================================================================
# Bench targets (run + report)
# =============================================================================

.PHONY: bench-hwire bench-pico bench-llhttp bench-httparse bench-hwire-resp bench-pico-resp bench-llhttp-resp bench-httparse-resp bench bench-resp

bench-hwire:
	$(MAKE) run-hwire-req
	@node report.js results/req_hwire_*.jsonl

bench-hwire-resp:
	$(MAKE) run-hwire-resp
	@node report.js results/rsp_hwire_*.jsonl

bench-pico:
	$(MAKE) run-pico-req
	@node report.js results/req_pico_*.jsonl

bench-pico-resp:
	$(MAKE) run-pico-resp
	@node report.js results/rsp_pico_*.jsonl

bench-llhttp:
	$(MAKE) run-llhttp-req
	@node report.js results/req_llhttp_*.jsonl

bench-llhttp-resp:
	$(MAKE) run-llhttp-resp
	@node report.js results/rsp_llhttp_*.jsonl

bench-httparse:
	$(MAKE) run-httparse-req
	@node report.js results/req_httparse_*.jsonl

bench-httparse-resp:
	$(MAKE) run-httparse-resp
	@node report.js results/rsp_httparse_*.jsonl

bench:
	$(MAKE) run
	$(MAKE) report

bench-resp:
	$(MAKE) run-resp
	$(MAKE) report-resp
