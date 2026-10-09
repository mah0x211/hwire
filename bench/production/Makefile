# Standalone production benchmark.
CC ?= cc
CARGO ?= cargo
CFLAGS ?= -O2 -DNDEBUG -std=c11
BIN ?= bin
RESULTS := results
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
NATIVE_FLAGS := -march=native
VARIANTS ?= nosimd sse2 sse42 native
VARIANT_FLAGS_sse42 := -msse4.2
else
NATIVE_FLAGS := -mcpu=native
VARIANTS ?= nosimd neon native
endif
VARIANT_FLAGS_nosimd := -DHWIRE_NO_SIMD -DHWIRE_NO_AES -fno-tree-vectorize
VARIANT_FLAGS_native := $(NATIVE_FLAGS)
VARIANT_FLAGS_siphash := $(NATIVE_FLAGS) -DHWIRE_NO_AES
GEN := python3 scripts/gen_production.py
IMPLEMENTATIONS := $(shell $(GEN) list)
-include $(foreach p,$(IMPLEMENTATIONS),$(p)/config.mk)
# Register and compile only the instruction targets supported by each adapter.
variant_implementations = $(foreach p,$(IMPLEMENTATIONS),$(if $(filter $(1),$(or $($(p)_VARIANTS),$(VARIANTS))),$(p)))
variant_sources = $(sort $(foreach p,$(call variant_implementations,$(1)),$(wildcard $(p)/*.c) $($(p)_SOURCES)))
variant_includes = -I../shared -I../../src $(foreach p,$(call variant_implementations,$(1)),-I$(p) $($(p)_CPPFLAGS))
variant_libraries = $(foreach p,$(call variant_implementations,$(1)),$($(p)_LDLIBS))
SRCS := $(sort $(foreach p,$(IMPLEMENTATIONS),$(wildcard $(p)/*.c) $($(p)_SOURCES)))
INC := -I../shared -I../../src $(foreach p,$(IMPLEMENTATIONS),-I$(p) $($(p)_CPPFLAGS))
LDLIBS = $(foreach p,$(IMPLEMENTATIONS),$($(p)_LDLIBS))
CONFIGS := $(wildcard $(foreach p,$(IMPLEMENTATIONS),$(p)/config.mk))
FETCH := $(wildcard $(foreach p,$(IMPLEMENTATIONS),$(p)/fetch.sh))
BUILD := $(wildcard $(foreach p,$(IMPLEMENTATIONS),$(p)/build.sh))
# Match the compiler's object names, retaining only active sources and variants.
DEP_NAMES := $(shell printf '%s\n' $(SRCS) | sed 's|[^A-Za-z0-9]|_|g;s|$$|.d|') driver.d $(if $(filter Linux Darwin,$(shell uname -s)),pool.d) $(if $(filter Darwin,$(shell uname -s)),pool_darwin.d)
-include $(foreach v,$(VARIANTS),$(wildcard $(addprefix $(BIN)/$(v)/objects/,$(DEP_NAMES))))
DEPS := $(wildcard $(SRCS) $(foreach p,$(IMPLEMENTATIONS),$(p)/*.h $($(p)_BUILD_DEPS)))
BINS := $(if $(IMPLEMENTATIONS),$(foreach v,$(VARIANTS),$(if $(call variant_implementations,$(v)),$(BIN)/$(v)/bench_production)))
POOL_TESTS := $(if $(filter Linux Darwin,$(shell uname -s)),$(patsubst %/bench_production,%/test_pool,$(BINS)))
ifeq ($(shell uname -s),Darwin)
POOL_TEST_LINK = -L$(BIN)/$* -lbench_pool -Wl,-rpath,@loader_path
else
POOL_TEST_LINK = $(BIN)/$*/objects/pool.o $(foreach symbol,malloc calloc realloc aligned_alloc posix_memalign free,-Wl,--wrap=$(symbol))
endif
.PHONY: all report update-readme list check clean FORCE
.PRECIOUS: $(BIN)/%/implementationlist.c $(BIN)/%/config.txt
.DEFAULT_GOAL := run
.NOTPARALLEL:
all: build
BENCH_ADAPTERS = $(IMPLEMENTATIONS)
include ../shared/runner.mk

report:
	@if [ -n "$(IMPLEMENTATIONS)" ]; then python3 scripts/report.py; else echo "No implementations registered."; fi
update-readme:
	@python3 scripts/update_readme.py
list:
	@echo "implementations: $(IMPLEMENTATIONS)"
	@echo "variants: $(VARIANTS)"
check: $(BINS) $(POOL_TESTS)
	@for binary in $(POOL_TESTS); do ./$$binary || exit 1; done
	@for binary in $(BINS); do ./$$binary --check || exit 1; done

$(BIN)/%/test_pool: tests/test_pool.c shared/pool.h $(BIN)/%/bench_production
	@$(CC) $(CFLAGS) $(VARIANT_FLAGS_$*) -UNDEBUG -fno-builtin -Ishared $< $(POOL_TEST_LINK) -o $@
clean:
	rm -rf $(BIN) $(RESULTS) scripts/__pycache__ tests/__pycache__
FORCE:
$(BIN)/%/config.txt: FORCE
	@mkdir -p $(BIN)/$*
	@printf '%s\n' '$(CC)' '$(CARGO)' '$(CFLAGS)' '$(VARIANT_FLAGS_$*)' '$(SRCS)' '$(INC)' '$(LDLIBS)' '$(foreach p,$(IMPLEMENTATIONS),$($(p)_CFLAGS))' > $@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi
$(BIN)/%/implementationlist.c: FORCE scripts/gen_production.py ../shared/scripts/registration.py
	@mkdir -p $(BIN)/$*
	@$(GEN) write $@ $* $(call variant_implementations,$*)
$(BIN)/%/bench_production: $(BIN)/%/config.txt $(BIN)/%/implementationlist.c Makefile ../shared/runner.mk bench_production.c ../shared/bench_stats.h ../shared/bench_timer.h ../shared/scripts/build_variant.sh shared/pool.c shared/pool.h shared/pool_darwin.c ../data/scenarios/browser_cdn.h ../data/scenarios/browser_auth.h $(DEPS) $(CONFIGS) $(FETCH) $(BUILD) | setup
	@for p in $(call variant_implementations,$*); do if [ -f "$$p/build.sh" ]; then CARGO='$(CARGO)' sh "$$p/build.sh" '$*' || exit 1; fi; done
	@$(foreach p,$(IMPLEMENTATIONS),$(p)_CFLAGS='$($(p)_CFLAGS)') CC='$(CC)' CFLAGS='$(CFLAGS)' SRCS='$(call variant_sources,$*)' INC='$(call variant_includes,$*)' LDLIBS='$(subst %,$*,$(call variant_libraries,$*))' BIN='$(BIN)' DRIVER='bench_production.c' POOL='1' sh ../shared/scripts/build_variant.sh '$*' '$(VARIANT_FLAGS_$*)'
