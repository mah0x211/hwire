CC = clang
CFLAGS = -std=c99 -Isrc -O2 -g
# Keep diagnostics independent of CFLAGS overrides in CI and local builds.
CONFIG_DIR := $(abspath config)
COMPILER := $(if $(findstring clang,$(shell $(CC) --version)),clang,gcc)
ifeq ($(COMPILER),clang)
QUALITY_FLAGS = --config=$(CONFIG_DIR)/clang/warnings.cfg
else
QUALITY_FLAGS = @$(CONFIG_DIR)/gcc/warnings.rsp
endif
CONFIG_DEPS = $(wildcard $(CONFIG_DIR)/$(COMPILER)/*) Makefile
CLANG_TIDY ?= clang-tidy
TIDY_FLAGS ?= $(NATIVE_TEST_FLAGS)

PYTHON ?= python3
COV_FLAGS = -fprofile-instr-generate -fcoverage-mapping

# Exercise the SSE4.2 parser path in x86 coverage builds.
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
    COV_FLAGS += -msse4.2
endif
# Match native test flags to the host architecture.
ifneq ($(filter arm64 aarch64,$(UNAME_M)),)
    NATIVE_TEST_FLAGS = -mcpu=native
    AES_TEST_FLAGS = -march=armv8-a+crypto
else ifneq ($(filter x86_64 amd64 i386 i486 i586 i686,$(UNAME_M)),)
    NATIVE_TEST_FLAGS = -march=native
    AES_TEST_FLAGS = -maes
endif
LDFLAGS =

SRC_DIR = src
TEST_DIR = tests
OBJ_DIR = obj
COV_DIR = coverage

# Project source (excluding hwire.c main if it had one, but it is a lib)
# We compile hwire.c separately for tests
TARGET_SRC = $(SRC_DIR)/hwire.c

# Test sources
TEST_SRCS = $(wildcard $(TEST_DIR)/test_*.c)
# Filter out test_helpers.c from standalone test targets
TEST_CASES = $(filter-out $(TEST_DIR)/test_helpers.c, $(TEST_SRCS))
# Test executables
TEST_EXES = $(patsubst $(TEST_DIR)/%.c, $(OBJ_DIR)/%, $(TEST_CASES))
TABLE_TEST_EXES = $(OBJ_DIR)/table_test_slots $(OBJ_DIR)/table_test_slots_noaes $(OBJ_DIR)/table_test_multitable $(OBJ_DIR)/table_test_multitable_noaes $(OBJ_DIR)/table_test_table $(OBJ_DIR)/table_test_siphash $(OBJ_DIR)/table_test_aes $(OBJ_DIR)/table_test_table_noaes $(OBJ_DIR)/table_test_aes_noaes
COV_EXES = $(TEST_EXES) $(TABLE_TEST_EXES)
COV_OBJECTS = $(patsubst %,-object=%,$(wordlist 2,$(words $(COV_EXES)),$(COV_EXES)))

# Common dependencies for tests
TEST_DEPS = $(TARGET_SRC) $(SRC_DIR)/hwire.h $(TEST_DIR)/test_helpers.c $(TEST_DIR)/test_helpers.h $(CONFIG_DEPS)

.PHONY: all test table-test test-nosimd coverage html-coverage analyze clean

all: test

# Create obj directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Compile and run all tests
test: $(OBJ_DIR) $(TEST_EXES)
	@echo "Running tests..."
	@for exe in $(TEST_EXES); do \
		echo "Running $$exe"; \
		./$$exe || exit 1; \
	done
	@echo "All tests passed!"

# Build and run the optional table tests without linking hwire.c.
table-test: $(TABLE_TEST_EXES)
	@for exe in $(TABLE_TEST_EXES); do \
		echo "Running $$exe"; \
		./$$exe || exit 1; \
	done

$(OBJ_DIR)/table_test_slots: $(TEST_DIR)/table/test_slots.c $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table.h $(SRC_DIR)/hwire_table_aes.h $(SRC_DIR)/hwire.h $(CONFIG_DEPS) | $(OBJ_DIR)
	$(CC) $(QUALITY_FLAGS) $(CFLAGS) $(NATIVE_TEST_FLAGS) $(AES_TEST_FLAGS) $(LDFLAGS) -o $@ $(TEST_DIR)/table/test_slots.c

$(OBJ_DIR)/table_test_slots_noaes: $(TEST_DIR)/table/test_slots.c $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table.h $(SRC_DIR)/hwire_table_aes.h $(SRC_DIR)/hwire.h $(CONFIG_DEPS) | $(OBJ_DIR)
	$(CC) $(QUALITY_FLAGS) $(CFLAGS) -DHWIRE_NO_AES $(LDFLAGS) -o $@ $(TEST_DIR)/table/test_slots.c

$(OBJ_DIR)/table_test_multitable: $(TEST_DIR)/table/test_multitable.c $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table.h $(SRC_DIR)/hwire_table_aes.h $(SRC_DIR)/hwire.h $(CONFIG_DEPS) | $(OBJ_DIR)
	$(CC) $(QUALITY_FLAGS) $(CFLAGS) $(NATIVE_TEST_FLAGS) $(AES_TEST_FLAGS) $(LDFLAGS) -o $@ $(TEST_DIR)/table/test_multitable.c $(SRC_DIR)/hwire_table.c

$(OBJ_DIR)/table_test_multitable_noaes: $(TEST_DIR)/table/test_multitable.c $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table.h $(SRC_DIR)/hwire_table_aes.h $(SRC_DIR)/hwire.h $(CONFIG_DEPS) | $(OBJ_DIR)
	$(CC) $(QUALITY_FLAGS) $(CFLAGS) -DHWIRE_NO_AES $(LDFLAGS) -o $@ $(TEST_DIR)/table/test_multitable.c $(SRC_DIR)/hwire_table.c

$(OBJ_DIR)/table_test_table: $(TEST_DIR)/table/test_table.c $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table.h $(SRC_DIR)/hwire_table_aes.h $(SRC_DIR)/hwire.h $(CONFIG_DEPS) | $(OBJ_DIR)
	$(CC) $(QUALITY_FLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(TEST_DIR)/table/test_table.c $(SRC_DIR)/hwire_table.c

$(OBJ_DIR)/table_test_siphash: $(TEST_DIR)/table/test_siphash.c $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table.h $(SRC_DIR)/hwire_table_aes.h $(SRC_DIR)/hwire.h $(CONFIG_DEPS) | $(OBJ_DIR)
	$(CC) $(QUALITY_FLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(TEST_DIR)/table/test_siphash.c

$(OBJ_DIR)/table_test_aes: $(TEST_DIR)/table/test_aes.c $(TEST_DIR)/table/aes_vectors.h $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table.h $(SRC_DIR)/hwire_table_aes.h $(SRC_DIR)/hwire.h $(CONFIG_DEPS) | $(OBJ_DIR)
	$(CC) $(QUALITY_FLAGS) $(CFLAGS) $(NATIVE_TEST_FLAGS) $(AES_TEST_FLAGS) -DHWIRE_TEST_EXPECT_AES=1 $(LDFLAGS) -o $@ $(TEST_DIR)/table/test_aes.c

$(OBJ_DIR)/table_test_table_noaes: $(TEST_DIR)/table/test_table.c $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table.h $(SRC_DIR)/hwire_table_aes.h $(SRC_DIR)/hwire.h $(CONFIG_DEPS) | $(OBJ_DIR)
	$(CC) $(QUALITY_FLAGS) $(CFLAGS) -DHWIRE_NO_AES $(LDFLAGS) -o $@ $(TEST_DIR)/table/test_table.c $(SRC_DIR)/hwire_table.c

# Explicit CPU flags require AES on supported test hosts; the disabled build checks fallback.
$(OBJ_DIR)/table_test_aes_noaes: $(TEST_DIR)/table/test_aes.c $(TEST_DIR)/table/aes_vectors.h $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table.h $(SRC_DIR)/hwire_table_aes.h $(SRC_DIR)/hwire.h $(CONFIG_DEPS) | $(OBJ_DIR)
	$(CC) $(QUALITY_FLAGS) $(CFLAGS) $(NATIVE_TEST_FLAGS) $(AES_TEST_FLAGS) -DHWIRE_NO_AES -DHWIRE_TEST_EXPECT_AES=0 $(LDFLAGS) -o $@ $(TEST_DIR)/table/test_aes.c

# Compile and run tests without SIMD
test-nosimd:
	$(MAKE) clean
	$(MAKE) test CFLAGS="$(CFLAGS) -DHWIRE_NO_SIMD"

# Rule to build test executables
$(OBJ_DIR)/%: $(TEST_DIR)/%.c $(TEST_DEPS)
	$(CC) $(QUALITY_FLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.c,$^)

# LCOV_EXCL_LINE / LCOV_EXCL_BR_LINE omit documented defensive paths.
# Raw LCOV and LLVM HTML remain available without exclusions.
# Coverage target
coverage:
	$(MAKE) clean
	mkdir -p $(OBJ_DIR) $(COV_DIR)
	$(MAKE) $(COV_EXES) CFLAGS="$(CFLAGS) $(COV_FLAGS)" LDFLAGS="$(LDFLAGS) $(COV_FLAGS)"
	@echo "Running tests..."
	@for exe in $(COV_EXES); do \
		echo "Running $$exe"; \
		LLVM_PROFILE_FILE="$(COV_DIR)/%m.profraw" ./$$exe || exit 1; \
	done
	@echo "All tests passed!"
	@echo "Generating coverage report..."
	llvm-profdata merge -sparse $(COV_DIR)/*.profraw -o $(COV_DIR)/coverage.profdata
	llvm-cov export -format=lcov $(firstword $(COV_EXES)) $(COV_OBJECTS) -instr-profile=$(COV_DIR)/coverage.profdata --sources $(SRC_DIR)/hwire.c $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table_aes.h > $(COV_DIR)/coverage.raw.info
	$(PYTHON) tests/filter_coverage.py $(COV_DIR)/coverage.raw.info $(COV_DIR)/coverage.info
	@echo "Coverage report generated in $(COV_DIR)/coverage.info"

# HTML coverage report (local use)
html-coverage: coverage
	llvm-cov show -format=html $(firstword $(COV_EXES)) $(COV_OBJECTS) -instr-profile=$(COV_DIR)/coverage.profdata --sources $(SRC_DIR)/hwire.c $(SRC_DIR)/hwire_table.c $(SRC_DIR)/hwire_table_aes.h --output-dir=$(COV_DIR)/html
	@echo "HTML coverage report generated in $(COV_DIR)/html/index.html"

require-clang:
	@test '$(COMPILER)' = clang || { echo 'This target requires Clang.' >&2; exit 1; }

# Compile library translation units directly; no generated database is needed.
.PHONY: tidy sanitizers hardening pattern require-clang
analyze: tidy

tidy: require-clang
	$(CLANG_TIDY) --config-file=.clang-tidy src/hwire.c src/hwire_table.c -- $(QUALITY_FLAGS) $(CFLAGS) $(TIDY_FLAGS)

# Clang loads configuration options before command-line options. Strip caller
# optimization flags so the selected sanitizer/hardening profile controls them.
sanitizers: require-clang
	$(MAKE) test table-test OBJ_DIR=$(OBJ_DIR)/sanitizers \
	  CFLAGS='$(filter-out -O%,$(CFLAGS)) $(NATIVE_TEST_FLAGS) --config=$(CONFIG_DIR)/clang/sanitizers.cfg' \
	  LDFLAGS='$(LDFLAGS) --config=$(CONFIG_DIR)/clang/sanitizers.cfg'

# Fortify level 3 requires Linux/glibc; stack protection is also tested on macOS.
FORTIFY_CONFIG = $(if $(filter Linux,$(shell uname -s)),--config=$(CONFIG_DIR)/clang/fortify.cfg)
hardening: require-clang
	$(MAKE) test table-test OBJ_DIR=$(OBJ_DIR)/hardening \
	  CFLAGS='$(filter-out -O%,$(CFLAGS)) $(NATIVE_TEST_FLAGS) --config=$(CONFIG_DIR)/clang/hardening.cfg $(FORTIFY_CONFIG)'

# A separate supplemental build, never an input to benchmarks or MSan.
pattern: require-clang
	$(MAKE) test table-test OBJ_DIR=$(OBJ_DIR)/pattern \
	  CFLAGS='$(CFLAGS) $(NATIVE_TEST_FLAGS) --config=$(CONFIG_DIR)/clang/pattern.cfg'

clean:
	rm -rf $(OBJ_DIR) $(COV_DIR) *.gcno *.gcda *.profraw *.profdata

# Bounded libFuzzer runs; variables such as FUZZ_SECONDS propagate to the submake.
.PHONY: fuzz
fuzz:
	$(MAKE) -C tests/fuzz run

.PHONY: fuzz-coverage
fuzz-coverage:
	$(MAKE) -C tests/fuzz coverage
