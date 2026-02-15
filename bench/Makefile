CC = clang
CXX = clang++
CFLAGS = -std=c99 -O2 -DNDEBUG
CXXFLAGS = -std=c++17 -O2 -DNDEBUG -DCATCH_CONFIG_ENABLE_BENCHMARKING

# Detect Catch2 path
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CATCH2_PREFIX := $(shell brew --prefix catch2 2>/dev/null)
    CATCH2_INC := $(CATCH2_PREFIX)/include
    CATCH2_LIB := $(CATCH2_PREFIX)/lib
else
    CATCH2_INC := /usr/include
    CATCH2_LIB := /usr/lib/x86_64-linux-gnu
endif

# External dependency paths
PICO_DIR := deps/picohttpparser
LLHTTP_DIR := deps/llhttp

INCLUDES = -I../src -I$(PICO_DIR) -I$(LLHTTP_DIR) -I$(CATCH2_INC)
LDFLAGS = -L$(CATCH2_LIB) -lCatch2Main -lCatch2

# Architecture detection
UNAME_M := $(shell uname -m)

.PHONY: all clean run report bench deps patch unpatch

# Fetch external dependencies
deps:
	@if [ ! -d "$(PICO_DIR)" ]; then \
		echo "Cloning picohttpparser..."; \
		mkdir -p deps && git clone --depth 1 https://github.com/h2o/picohttpparser.git $(PICO_DIR); \
	fi
	@if [ ! -d "$(LLHTTP_DIR)" ]; then \
		echo "Cloning llhttp..."; \
		mkdir -p deps && git clone --depth 1 https://github.com/nodejs/llhttp.git $(LLHTTP_DIR); \
	fi
	@if [ ! -f "$(LLHTTP_DIR)/build/c/llhttp.c" ]; then \
		echo "Building llhttp..."; \
		cd $(LLHTTP_DIR) && npm install && make generate; \
	fi

# Apply NO_SIMD patches (script-based)
patch:
	@echo "Applying NO_SIMD patches..."
	@mkdir -p deps/hwire
	@cp ../src/hwire.c deps/hwire/hwire.c
	@./scripts/apply-no-simd.sh hwire
	@./scripts/apply-no-simd.sh pico
	@./scripts/apply-no-simd.sh llhttp

# Restore original files (script-based)
unpatch:
	@echo "Restoring original files..."
	@./scripts/restore-no-simd.sh hwire
	@./scripts/restore-no-simd.sh pico
	@./scripts/restore-no-simd.sh llhttp

# Benchmark targets (architecture-specific)
ifeq ($(UNAME_M),x86_64)
    # x86_64: hwire has SSE2 and AVX2, pico/llhttp have SSE4.2
    BENCH_TARGETS = bench_hwire_avx2 bench_hwire_sse2 bench_hwire_nosimd \
                    bench_pico_sse42 bench_pico_nosimd \
                    bench_llhttp_sse42 bench_llhttp_nosimd
else
    # ARM: hwire has NEON, pico has no SIMD, llhttp has NEON
    BENCH_TARGETS = bench_hwire_neon bench_hwire_nosimd \
                    bench_pico_nosimd \
                    bench_llhttp_neon bench_llhttp_nosimd
endif

# Build all targets
all:
	$(MAKE) deps
	$(MAKE) patch
	$(MAKE) $(BENCH_TARGETS)

# =============================================================================
# hwire benchmarks (always use deps/hwire/hwire.c)
# =============================================================================

# x86_64: SSE2 (default on x86_64, no flags needed)
bench_hwire_sse2: bench_hwire.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o hwire_sse2.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ bench_hwire.cc hwire_sse2.o $(LDFLAGS)

# x86_64: AVX2 (-mavx2 -mfma required)
bench_hwire_avx2: bench_hwire.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) -mavx2 -mfma $(INCLUDES) -c -o hwire_avx2.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) -mavx2 -mfma $(INCLUDES) -o $@ bench_hwire.cc hwire_avx2.o $(LDFLAGS)

# ARM: NEON (default on ARM, no flags needed)
bench_hwire_neon: bench_hwire.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o hwire_neon.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ bench_hwire.cc hwire_neon.o $(LDFLAGS)

# No-SIMD version (uses patched source)
bench_hwire_nosimd: bench_hwire.cc deps/hwire/hwire.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -c -o hwire_nosimd.o deps/hwire/hwire.c
	$(CXX) $(CXXFLAGS) -DNO_SIMD $(INCLUDES) -o $@ bench_hwire.cc hwire_nosimd.o $(LDFLAGS)

# =============================================================================
# picohttpparser benchmarks
# =============================================================================

bench_pico_sse42: bench_pico.cc $(PICO_DIR)/picohttpparser.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -c -o pico_sse42.o $(PICO_DIR)/picohttpparser.c
	$(CXX) $(CXXFLAGS) -msse4.2 $(INCLUDES) -o $@ bench_pico.cc pico_sse42.o $(LDFLAGS)

bench_pico_nosimd: bench_pico.cc $(PICO_DIR)/picohttpparser.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -c -o pico_nosimd.o $(PICO_DIR)/picohttpparser.c
	$(CXX) $(CXXFLAGS) -DNO_SIMD $(INCLUDES) -o $@ bench_pico.cc pico_nosimd.o $(LDFLAGS)

# =============================================================================
# llhttp benchmarks
# =============================================================================

LLHTTP_SRC = $(LLHTTP_DIR)/build/c/llhttp.c $(LLHTTP_DIR)/src/native/api.c $(LLHTTP_DIR)/src/native/http.c

# x86_64: SSE4.2
bench_llhttp_sse42: bench_llhttp.cc $(LLHTTP_SRC)
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o llhttp_sse42.o $(LLHTTP_DIR)/build/c/llhttp.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o api_sse42.o $(LLHTTP_DIR)/src/native/api.c
	$(CC) $(CFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o http_sse42.o $(LLHTTP_DIR)/src/native/http.c
	$(CXX) $(CXXFLAGS) -msse4.2 $(INCLUDES) -I$(LLHTTP_DIR)/build -o $@ bench_llhttp.cc llhttp_sse42.o api_sse42.o http_sse42.o $(LDFLAGS)

# ARM: NEON (default)
bench_llhttp_neon: bench_llhttp.cc $(LLHTTP_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o llhttp_neon.o $(LLHTTP_DIR)/build/c/llhttp.c
	$(CC) $(CFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o api_neon.o $(LLHTTP_DIR)/src/native/api.c
	$(CC) $(CFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o http_neon.o $(LLHTTP_DIR)/src/native/http.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) -I$(LLHTTP_DIR)/build -o $@ bench_llhttp.cc llhttp_neon.o api_neon.o http_neon.o $(LDFLAGS)

# No-SIMD
bench_llhttp_nosimd: bench_llhttp.cc $(LLHTTP_SRC)
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o llhttp_nosimd.o $(LLHTTP_DIR)/build/c/llhttp.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o api_nosimd.o $(LLHTTP_DIR)/src/native/api.c
	$(CC) $(CFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -c -o http_nosimd.o $(LLHTTP_DIR)/src/native/http.c
	$(CXX) $(CXXFLAGS) -DNO_SIMD $(INCLUDES) -I$(LLHTTP_DIR)/build -o $@ bench_llhttp.cc llhttp_nosimd.o api_nosimd.o http_nosimd.o $(LDFLAGS)

# =============================================================================
# Utility targets
# =============================================================================

clean:
	$(MAKE) unpatch
	rm -f bench_hwire_avx2 bench_hwire_sse2 bench_hwire_neon bench_hwire_nosimd \
	      bench_pico_sse42 bench_pico_nosimd \
	      bench_llhttp_sse42 bench_llhttp_neon bench_llhttp_nosimd \
	      *.o results.txt
	rm -rf deps/

# Run benchmarks
run:
	$(MAKE) deps
	$(MAKE) patch
	$(MAKE) $(BENCH_TARGETS)
	@rm -f results.txt
	@for target in $(BENCH_TARGETS); do \
		echo "Running $$target..."; \
		./$$target 2>&1 | tee -a results.txt; \
	done

# Generate report
report: results.txt
	@node generate_report.js results.txt

# Run full benchmark (build + run + report)
bench:
	$(MAKE) run
	$(MAKE) report
