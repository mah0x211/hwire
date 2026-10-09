# Common lifecycle for standalone suites; compilation stays in each Makefile.
# BENCH_ADAPTERS names registered directories; BENCH_ARGUMENTS is driver-specific.
# Preserve quotes in adapter metadata without evaluating them as shell syntax.
bench_quote = '$(subst ','"'"',$(1))'
BENCH_RUN = INSTALL_DEPS='$(INSTALL_DEPS)' python3 ../shared/scripts/run.py --suite . $(foreach a,$(BENCH_ADAPTERS),--adapter $(a)) $(foreach v,$(VARIANTS),--variant $(v))
.PHONY: setup build build-adapter run benchmark-config benchmark-info
setup:
	@if [ '$(SKIP_SETUP)' != 1 ]; then $(BENCH_RUN) --setup-only; fi
build:
	@$(BENCH_RUN) --build-only
build-adapter: $(BINS)
run $(BENCH_RUN_TARGETS):
	@$(BENCH_RUN) $(foreach arg,$(BENCH_ARGUMENTS),--argument=$(arg))
benchmark-config:
	@printf '%s\n' $(call bench_quote,$(or $($(ADAPTER)_VARIANTS),$(VARIANTS))) $(call bench_quote,$(or $($(ADAPTER)_NAME),$($(ADAPTER)_PARSE_NAME),$(ADAPTER))) $(call bench_quote,$(CC)) $(call bench_quote,$(CFLAGS)) $(call bench_quote,$(CXX)) $(call bench_quote,$(CXXFLAGS)) $(call bench_quote,$($(ADAPTER)_ENV))
benchmark-info:
	@printf '%s\n' $(call bench_quote,$($(ADAPTER)_BUILD_INFO)) $(call bench_quote,$(ADAPTER) variants: $(foreach v,$(VARIANTS),$(v)=$(VARIANT_FLAGS_$(v))))
