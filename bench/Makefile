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
# Run and report: request benchmarks
# =============================================================================

run: run-hwire run-pico run-llhttp run-httparse

run-hwire: deps patch $(HWIRE_TARGETS)
	@mkdir -p results
	@rm -f results/req_hwire.jsonl
	@for target in $(HWIRE_TARGETS); do \
		echo "Running $$target..." >&2; \
		./$$target --reporter xml 2>/dev/null | node scripts/xml2json.js >> results/req_hwire.jsonl; \
	done

run-pico: deps patch $(PICO_TARGETS)
	@mkdir -p results
	@rm -f results/req_pico.jsonl
	@for target in $(PICO_TARGETS); do \
		echo "Running $$target..." >&2; \
		./$$target --reporter xml 2>/dev/null | node scripts/xml2json.js >> results/req_pico.jsonl; \
	done

run-llhttp: deps patch $(LLHTTP_TARGETS)
	@mkdir -p results
	@rm -f results/req_llhttp.jsonl
	@for target in $(LLHTTP_TARGETS); do \
		echo "Running $$target..." >&2; \
		./$$target --reporter xml 2>/dev/null | node scripts/xml2json.js >> results/req_llhttp.jsonl; \
	done

run-httparse: deps $(HTTPARSE_TARGETS)
	@mkdir -p results
	@rm -f results/req_httparse.jsonl
	@for target in $(HTTPARSE_TARGETS); do \
		echo "Running $$target..." >&2; \
		./$$target --reporter xml 2>/dev/null | node scripts/xml2json.js >> results/req_httparse.jsonl; \
	done

report:
	@node report.js results/req_*.jsonl

bench:
	$(MAKE) run
	$(MAKE) report

bench-hwire:
	$(MAKE) run-hwire
	@node report.js results/req_hwire.jsonl

bench-pico:
	$(MAKE) run-pico
	@node report.js results/req_pico.jsonl

bench-llhttp:
	$(MAKE) run-llhttp
	@node report.js results/req_llhttp.jsonl

bench-httparse:
	$(MAKE) run-httparse
	@node report.js results/req_httparse.jsonl

# =============================================================================
# Run and report: response benchmarks
# =============================================================================

run-resp: run-hwire-resp run-pico-resp run-llhttp-resp run-httparse-resp

run-hwire-resp: deps patch $(HWIRE_RESP_TARGETS)
	@mkdir -p results
	@rm -f results/rsp_hwire.jsonl
	@for target in $(HWIRE_RESP_TARGETS); do \
		echo "Running $$target..." >&2; \
		./$$target --reporter xml 2>/dev/null | node scripts/xml2json.js >> results/rsp_hwire.jsonl; \
	done

run-pico-resp: deps patch $(PICO_RESP_TARGETS)
	@mkdir -p results
	@rm -f results/rsp_pico.jsonl
	@for target in $(PICO_RESP_TARGETS); do \
		echo "Running $$target..." >&2; \
		./$$target --reporter xml 2>/dev/null | node scripts/xml2json.js >> results/rsp_pico.jsonl; \
	done

run-llhttp-resp: deps patch $(LLHTTP_RESP_TARGETS)
	@mkdir -p results
	@rm -f results/rsp_llhttp.jsonl
	@for target in $(LLHTTP_RESP_TARGETS); do \
		echo "Running $$target..." >&2; \
		./$$target --reporter xml 2>/dev/null | node scripts/xml2json.js >> results/rsp_llhttp.jsonl; \
	done

run-httparse-resp: deps $(HTTPARSE_RESP_TARGETS)
	@mkdir -p results
	@rm -f results/rsp_httparse.jsonl
	@for target in $(HTTPARSE_RESP_TARGETS); do \
		echo "Running $$target..." >&2; \
		./$$target --reporter xml 2>/dev/null | node scripts/xml2json.js >> results/rsp_httparse.jsonl; \
	done

report-resp:
	@node report.js results/rsp_*.jsonl

bench-resp:
	$(MAKE) run-resp
	$(MAKE) report-resp

bench-hwire-resp:
	$(MAKE) run-hwire-resp
	@node report.js results/rsp_hwire.jsonl

bench-pico-resp:
	$(MAKE) run-pico-resp
	@node report.js results/rsp_pico.jsonl

bench-llhttp-resp:
	$(MAKE) run-llhttp-resp
	@node report.js results/rsp_llhttp.jsonl

bench-httparse-resp:
	$(MAKE) run-httparse-resp
	@node report.js results/rsp_httparse.jsonl



