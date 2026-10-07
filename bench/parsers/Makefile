# Standalone parser benchmark.
CC ?= cc
CARGO ?= cargo
CFLAGS ?= -O2 -DNDEBUG -std=c11
BIN := bin
RESULTS := results
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
VARIANTS := nosimd sse2 sse42
VARIANT_FLAGS_sse42 := -msse4.2
else
VARIANTS := nosimd neon
endif
VARIANT_FLAGS_nosimd := -DHWIRE_NO_SIMD
GEN := python3 scripts/gen_parsers.py
IMPLEMENTATIONS := $(shell $(GEN) list)
-include $(foreach p,$(IMPLEMENTATIONS),$(p)/config.mk)
SRCS := $(sort $(foreach p,$(IMPLEMENTATIONS),$(wildcard $(p)/*.c) $($(p)_SOURCES)))
INC := -I../shared -I../../src $(foreach p,$(IMPLEMENTATIONS),-I$(p) $($(p)_CPPFLAGS))
LDLIBS = $(foreach p,$(IMPLEMENTATIONS),$($(p)_LDLIBS))
CONFIGS := $(wildcard $(foreach p,$(IMPLEMENTATIONS),$(p)/config.mk))
FETCH := $(wildcard $(foreach p,$(IMPLEMENTATIONS),$(p)/fetch.sh))
BUILD := $(wildcard $(foreach p,$(IMPLEMENTATIONS),$(p)/build.sh))
# Match the compiler's object names, retaining only active sources and variants.
DEP_NAMES := $(shell printf '%s\n' $(SRCS) | sed 's|[^A-Za-z0-9]|_|g;s|$$|.d|') driver.d
-include $(foreach v,$(VARIANTS),$(wildcard $(addprefix $(BIN)/$(v)/objects/,$(DEP_NAMES))))
DEPS := $(wildcard $(SRCS) $(foreach p,$(IMPLEMENTATIONS),$(p)/*.h $($(p)_BUILD_DEPS)))
BINS := $(if $(IMPLEMENTATIONS),$(foreach v,$(VARIANTS),$(BIN)/$(v)/bench_parsers))
.PHONY: all setup build run report update-readme list check clean FORCE request response nosimd sse2 sse42 neon
.PRECIOUS: $(BIN)/%/parserlist.c $(BIN)/%/config.txt
.DEFAULT_GOAL := run
.NOTPARALLEL:
all: build
setup:
	@for p in $(IMPLEMENTATIONS); do if [ -f "$$p/fetch.sh" ]; then CARGO='$(CARGO)' sh "$$p/fetch.sh" || exit 1; fi; done
build: $(BINS)
run: $(BINS)
	@mkdir -p $(RESULTS)
	@sh ../platform.sh $(RESULTS)/platform.txt '$(CC)' '$(CFLAGS)'
	@$(foreach p,$(IMPLEMENTATIONS),$(if $($(p)_BUILD_INFO),printf '%s\n' '$(p) build: $($(p)_BUILD_INFO)' >> $(RESULTS)/platform.txt;))
	@printf '%s\n' 'parser variants: $(foreach v,$(VARIANTS),$(v)=$(VARIANT_FLAGS_$(v)))' >> $(RESULTS)/platform.txt
	@$(foreach p,$(IMPLEMENTATIONS),$(p)_NAME='$($(p)_PARSE_NAME)') python3 -c 'import json, os; from pathlib import Path; Path("results/active.json").write_text(json.dumps({p+"-"+v: os.getenv(p+"_NAME") or p for p in "$(IMPLEMENTATIONS)".split() for v in "$(VARIANTS)".split()}))'
	@for binary in $(BINS); do ./$$binary $(DIRECTION) || exit 1; done
	@$(MAKE) --no-print-directory report
report:
	@if [ -n "$(IMPLEMENTATIONS)" ]; then python3 scripts/report.py; else echo "No implementations registered."; fi
update-readme:
	@python3 scripts/update_readme.py
list:
	@echo "implementations: $(IMPLEMENTATIONS)"
	@echo "variants: $(VARIANTS)"
check: $(BINS)
	@PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -v
	@for binary in $(BINS); do ./$$binary --check || exit 1; done
clean:
	rm -rf $(BIN) $(RESULTS) scripts/__pycache__ tests/__pycache__
FORCE:
$(BIN)/%/config.txt: FORCE
	@mkdir -p $(BIN)/$*
	@printf '%s\n' '$(CC)' '$(CARGO)' '$(CFLAGS)' '$(VARIANT_FLAGS_$*)' '$(SRCS)' '$(INC)' '$(LDLIBS)' '$(foreach p,$(IMPLEMENTATIONS),$($(p)_CFLAGS))' > $@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi
$(BIN)/%/parserlist.c: FORCE scripts/gen_parsers.py ../shared/scripts/registration.py
	@mkdir -p $(BIN)/$*
	@$(GEN) write $@ $*
$(BIN)/%/bench_parsers: $(BIN)/%/config.txt $(BIN)/%/parserlist.c Makefile bench_parsers.c ../shared/bench_stats.h ../shared/bench_timer.h ../shared/scripts/build_variant.sh $(BIN)/fixtures.c $(DEPS) $(CONFIGS) $(FETCH) $(BUILD) | setup
	@for p in $(IMPLEMENTATIONS); do if [ -f "$$p/build.sh" ]; then CARGO='$(CARGO)' sh "$$p/build.sh" '$*' || exit 1; fi; done
	@$(foreach p,$(IMPLEMENTATIONS),$(p)_CFLAGS='$($(p)_CFLAGS)') CC='$(CC)' CFLAGS='$(CFLAGS)' SRCS='$(SRCS)' INC='$(INC)' LDLIBS='$(subst %,$*,$(LDLIBS))' DRIVER='bench_parsers.c' POOL='0' sh ../shared/scripts/build_variant.sh '$*' '$(VARIANT_FLAGS_$*)'
request response:
	@$(MAKE) --no-print-directory run DIRECTION=$@
nosimd sse2 sse42 neon:
	@if [ "$(filter $@,$(VARIANTS))" = "" ]; then echo "variant '$@' unavailable"; exit 1; fi
	@$(MAKE) --no-print-directory run VARIANTS=$@
$(BIN)/fixtures.c: FORCE ../shared/scripts/gen_fixtures.py $(wildcard ../data/request/*.h ../data/response/*.h)
	@mkdir -p $(BIN)
	@python3 ../shared/scripts/gen_fixtures.py write $@
