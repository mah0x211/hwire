CC = clang
CFLAGS = -std=c99 -Isrc -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes -Wmissing-declarations -Wunused-parameter -Werror -Wundef -Wcast-align -Wwrite-strings -Wunreachable-code -Wformat=2 -fno-common -Wno-gnu-statement-expression
COV_FLAGS = -fprofile-instr-generate -fcoverage-mapping

# Detect architecture for AVX2 support (only x86_64)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
    COV_FLAGS += -mavx2
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

# Common dependencies for tests
TEST_DEPS = $(TARGET_SRC) $(TEST_DIR)/test_helpers.c

.PHONY: all test test-nosimd coverage html-coverage analyze clean

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

# Compile and run tests without SIMD
test-nosimd:
	$(MAKE) clean
	$(MAKE) test CFLAGS="$(CFLAGS) -DHWIRE_NO_SIMD"

# Rule to build test executables
$(OBJ_DIR)/%: $(TEST_DIR)/%.c $(TEST_DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Coverage target
coverage:
	$(MAKE) clean
	mkdir -p $(OBJ_DIR) $(COV_DIR)
	$(MAKE) $(TEST_EXES) CFLAGS="$(CFLAGS) $(COV_FLAGS)" LDFLAGS="$(LDFLAGS) $(COV_FLAGS)"
	@echo "Running tests..."
	@for exe in $(TEST_EXES); do \
		echo "Running $$exe"; \
		LLVM_PROFILE_FILE="$(COV_DIR)/%m.profraw" ./$$exe || exit 1; \
	done
	@echo "All tests passed!"
	@echo "Generating coverage report..."
	llvm-profdata merge -sparse $(COV_DIR)/*.profraw -o $(COV_DIR)/coverage.profdata
	llvm-cov export -format=lcov $(TEST_EXES) -instr-profile=$(COV_DIR)/coverage.profdata $(SRC_DIR)/hwire.c > $(COV_DIR)/coverage.info
	@echo "Coverage report generated in $(COV_DIR)/coverage.info"

# HTML coverage report (local use)
html-coverage: coverage
	llvm-cov show -format=html $(TEST_EXES) -instr-profile=$(COV_DIR)/coverage.profdata $(SRC_DIR)/hwire.c --output-dir=$(COV_DIR)/html
	@echo "HTML coverage report generated in $(COV_DIR)/html/index.html"

# Static analysis with scan-build
analyze:
	$(MAKE) clean
	scan-build -o $(COV_DIR)/scan-build $(MAKE) test

clean:
	rm -rf $(OBJ_DIR) $(COV_DIR) *.gcno *.gcda *.profraw *.profdata
