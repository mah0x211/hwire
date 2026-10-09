# Independent storage benchmarks; adapters may also be linked by parsers.
CC ?= cc
CXX ?= c++
AR ?= ar
CFLAGS ?= -O2 -DNDEBUG -std=c11
CXXFLAGS ?= -O2 -DNDEBUG -std=c++17
BIN ?= bin
RESULTS := results/storage
UNAME_M := $(shell uname -m)
ifneq ($(filter arm64 aarch64,$(UNAME_M)),)
NATIVE_FLAGS := -mcpu=native
else ifneq ($(filter x86_64 amd64,$(UNAME_M)),)
NATIVE_FLAGS := -march=native
endif
VARIANTS ?= native
VARIANT_FLAGS_native := $(NATIVE_FLAGS)
VARIANT_FLAGS_siphash := $(NATIVE_FLAGS) -DHWIRE_NO_AES
GEN := python3 scripts/gen_hashmaps.py
MAPS := $(shell $(GEN) list)
-include $(foreach m,$(MAPS),$(m)/config.mk)
REGISTRATIONS := $(shell $(GEN) registrations)
SOURCES := $(shell $(GEN) sources) $(foreach m,$(MAPS),$($(m)_SOURCES))
DEPS := $(shell $(GEN) deps) $(wildcard $(SOURCES))
INCLUDES := -I. -I../shared -Ishared -I../../src $(foreach m,$(MAPS),-I$(m))
LIBS := $(foreach v,$(VARIANTS),$(BIN)/$(v)/libhashmaps.a)
BINS := $(foreach v,$(VARIANTS),$(BIN)/$(v)/bench_hashmap)
MAP_ENV = $(foreach m,$(MAPS),$(m)_CPPFLAGS='$($(m)_CPPFLAGS)' $(m)_CFLAGS='$($(m)_CFLAGS)' $(m)_CXXFLAGS='$($(m)_CXXFLAGS)')
BUILD_ENV = BIN='$(BIN)' $(MAP_ENV) LDLIBS='$(foreach m,$(MAPS),$($(m)_LDLIBS))' CC='$(CC)' CXX='$(CXX)' AR='$(AR)' CFLAGS='$(CFLAGS)' CXXFLAGS='$(CXXFLAGS)'
.DEFAULT_GOAL := run
.NOTPARALLEL:
.PHONY: all adapters growth report check list clean FORCE
.PRECIOUS: $(BIN)/%/maplist.c $(BIN)/%/libhashmaps.a $(BIN)/%/config.txt
all: build
BENCH_ADAPTERS = $(MAPS)
BENCH_RUN_TARGETS := growth
BENCH_ARGUMENTS = $(if $(filter growth,$@),--growth,)
include ../shared/runner.mk
adapters: $(LIBS)

report:
	@python3 scripts/report_hashmaps.py
# Statistical helper checks only; no container/data validation benchmark path.
check: build $(BIN)/test_stats
	@./$(BIN)/test_stats
list:
	@echo "hashmaps: $(REGISTRATIONS)"
	@echo "hashmap variants: $(VARIANTS)"
clean:
	rm -rf $(BIN)
FORCE:
$(BIN)/%/config.txt: FORCE
	@mkdir -p $(BIN)/$*
	@printf '%s\n' '$(CC)' '$(CXX)' '$(AR)' '$(CFLAGS)' '$(CXXFLAGS)' '$(VARIANT_FLAGS_$*)' '$(SOURCES)' '$(DEPS)' '$(MAP_ENV)' '$(foreach m,$(MAPS),$($(m)_LDLIBS))' > $@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi
$(BIN)/%/libhashmaps.a: $(BIN)/%/config.txt Makefile ../shared/runner.mk scripts/build.sh shared/hashmap_hash.c shared/hashmap_hash.h shared/hashmap_key.h $(DEPS) ../../src/hwire_table.c ../../src/hwire_table.h ../../src/hwire_table_aes.h ../../src/hwire.h | setup
	@$(BUILD_ENV) SRCS='$(SOURCES)' INC='$(INCLUDES)' sh scripts/build.sh $* '$(VARIANT_FLAGS_$*)' adapters
$(BIN)/%/maplist.c: FORCE scripts/gen_hashmaps.py $(DEPS)
	@mkdir -p $(BIN)/$*
	@$(GEN) write $@ $*
$(BIN)/test_stats: Makefile ../shared/tests/test_stats.c ../shared/bench_stats.h
	@mkdir -p $(BIN)
	@$(CC) $(CFLAGS) -UNDEBUG -I../shared ../shared/tests/test_stats.c -lm -o $@
$(BIN)/%/bench_hashmap: bench_hashmap.c ../shared/bench_input.h ../data/headers/headers_512.h ../shared/bench_stats.h scripts/build.sh $(BIN)/%/maplist.c $(BIN)/%/libhashmaps.a
	@$(BUILD_ENV) INC='$(INCLUDES)' sh scripts/build.sh $* '$(VARIANT_FLAGS_$*)' hashmap
