# hwire benchmark suite
#
# One plain-C benchmark driver (main.c) measures every registered parser
# with the identical loop. Third-party parsers are registered by a
# parsers/<name>/ directory containing their sources plus the two uniform
# adapters request.c and response.c:
#
#   int <name>_request(const unsigned char *data, size_t len);   /* 0 = ok */
#   int <name>_response(const unsigned char *data, size_t len);  /* 0 = ok */
#
# The suite is built once per SIMD variant (see VARIANTS below) with the
# variant's flags applied to EVERY parser, so each run compares all
# parsers under the same build configuration. `make` runs every variant;
# `make <variant>` (e.g. `make nosimd`, `make sse42`) runs only that one.
# Results accumulate as results/<parser>-<variant>.txt and report.py
# renders the comparison.

CC       ?= cc
CFLAGS   ?= -O2 -std=c99

REPORT   := python3 report.py

BIN      := bin
RESULTS  := results

# SIMD variants. On x86_64 the four builds are the SIMD levels; the
# native/default build is the SSE2 baseline. Elsewhere (e.g. ARM64) the
# variants are the scalar build and the native NEON build.
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
VARIANTS := nosimd sse2 sse42 avx2
VARIANT_FLAGS_nosimd := -DHWIRE_NO_SIMD
VARIANT_FLAGS_sse2   :=
VARIANT_FLAGS_sse42  := -msse4.2
VARIANT_FLAGS_avx2   := -mavx2
else
VARIANTS := nosimd neon
VARIANT_FLAGS_nosimd := -DHWIRE_NO_SIMD
VARIANT_FLAGS_neon   :=
endif

# Registered parsers: a parsers/<name>/ directory with both adapters.
# hwire's own adapters live in parsers/hwire/; the library itself is
# always compiled from ../src.
GEN_PARSERS   := python3 gen_parsers.py
GEN_FIXTURES  := python3 gen_fixtures.py

# gen_parsers.py owns parser discovery and validation under parsers/.
PARSERS := $(shell $(GEN_PARSERS) list)

PARSER_SRCS := $(foreach p,$(PARSERS),$(wildcard parsers/$(p)/*.c))
ifeq ($(filter hwire,$(PARSERS)),hwire)
PARSER_SRCS += ../src/hwire.c
endif
PARSER_INCLUDES := $(foreach p,$(PARSERS),-Iparsers/$(p)) -I../src

# Message fixtures: messages/<name>.h defines MSG_<NAME>[] (the symbol is
# derived from the file name). The "req_"/"resp_" prefix selects the
# direction; names must be lowercase [a-z0-9_] with a [a-z] first letter
# after the prefix. Others are skipped with a warning. The Makefile
# generates bin/fixtures.c with the two tables.
# gen_fixtures.py owns fixture discovery and validation under
# messages/request/ and messages/response/.

VARIANT_BINS := $(foreach v,$(VARIANTS),$(BIN)/$(v)/bench)

# All variant names that can be requested via `make <variant>`; only the
# ones listed in $(VARIANTS) exist on this architecture.
POSSIBLE_VARIANTS := nosimd sse2 neon sse42 avx2

.PHONY: all run report request response clean list $(POSSIBLE_VARIANTS)

# `make` with no arguments builds and runs everything.
.DEFAULT_GOAL := run

all: $(VARIANT_BINS)

run: $(VARIANT_BINS)
	@mkdir -p $(RESULTS)
	@sh platform.sh $(RESULTS)/platform.txt '$(CC)' '$(CFLAGS)'
	@for b in $(VARIANT_BINS); do \
		./$$b || exit 1; \
	done
	@$(REPORT)

# Run a single variant, e.g. `make nosimd`, `make sse42`, `make neon`.
$(POSSIBLE_VARIANTS):
	@if [ "$(findstring $@,$(VARIANTS))" = "" ]; then \
		echo "variant '$@' is not available on $(UNAME_M) (available: $(VARIANTS))"; \
		exit 1; \
	fi
	@$(MAKE) --no-print-directory $(BIN)/$@/bench
	@mkdir -p $(RESULTS)
	@sh platform.sh $(RESULTS)/platform.txt '$(CC)' '$(CFLAGS)'
	./$(BIN)/$@/bench
	@$(REPORT)

# Direction-limited runs: requests or responses only.
request response: $(VARIANT_BINS)
	@mkdir -p $(RESULTS)
	@sh platform.sh $(RESULTS)/platform.txt '$(CC)' '$(CFLAGS)'
	@for b in $(VARIANT_BINS); do 		./$$b $@ || exit 1; 	done
	@$(REPORT)

report:
	@$(REPORT)

list:
	@echo "parsers: $(PARSERS)"
	@echo "variants ($(UNAME_M)): $(VARIANTS)"

clean:
	rm -rf $(BIN) $(RESULTS)

# --- one binary per SIMD variant ---------------------------------------
# Pattern rules: the stem is the variant name. gen_parsers.py writes the
# per-variant parser table; build_variant.sh compiles every parser source
# plus main.c with the variant's flags and links the binary.

$(BIN)/%/parserlist.c: Makefile gen_parsers.py $(wildcard parsers/*/request.c parsers/*/response.c)
	@mkdir -p $(BIN)/$*
	@$(GEN_PARSERS) write $@ $*

$(BIN)/%/bench: main.c $(BIN)/%/parserlist.c $(BIN)/fixtures.c build_variant.sh \
		../src/hwire.c ../src/hwire.h $(PARSER_SRCS)
	@CC='$(CC)' CFLAGS='$(CFLAGS)' SRCS='$(PARSER_SRCS)' INC='$(PARSER_INCLUDES)' \
		sh build_variant.sh $* '$(VARIANT_FLAGS_$*)'

# Fixture tables (shared by every variant): one #include and one table
# row per message header.
$(BIN)/fixtures.c: Makefile gen_fixtures.py $(wildcard messages/request/*.h messages/response/*.h)
	@mkdir -p $(BIN)
	@$(GEN_FIXTURES) write $@
