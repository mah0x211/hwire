CC = clang
PYTHON ?= python3
FUZZ_COVERAGE ?= 0
ifeq ($(FUZZ_COVERAGE),1)
COVERAGE_FLAGS = -fprofile-instr-generate -fcoverage-mapping
endif
ARCH := $(shell uname -m)
ifeq ($(filter x86_64 amd64 arm64 aarch64,$(ARCH)),)
$(error Unsupported architecture: $(ARCH))
endif
FUZZ_SECONDS ?= 30
FUZZ_TARGETS ?= messages values table
FUZZ_VARIANTS ?= auto
FUZZ_INPUT ?=
OUT ?= out
CFLAGS = -std=c11 -O1 -g -Wall -Wextra -Werror -I../../src -fno-omit-frame-pointer
export ASAN_OPTIONS ?= quarantine_size_mb=32:malloc_context_size=5
export UBSAN_OPTIONS ?= halt_on_error=1:print_stacktrace=1
SANITIZERS = -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined

ifeq ($(filter arm64 aarch64,$(ARCH)),)
PARSER_VARIANTS = scalar sse2 sse42 native
FLAGS_sse2 = -msse2 -mno-sse4.2 -DFUZZ_EXPECT_SSE2
FLAGS_sse42 = -msse4.2 -DFUZZ_EXPECT_SSE42
FLAGS_native = -march=native
else
PARSER_VARIANTS = scalar neon native
FLAGS_neon = -DFUZZ_EXPECT_NEON
FLAGS_native = -mcpu=native
endif
FLAGS_scalar = -DHWIRE_NO_SIMD -DFUZZ_EXPECT_SCALAR
FLAGS_siphash = $(FLAGS_native) -DHWIRE_NO_AES

.PHONY: all build run replay coverage coverage-report clean
all: run
build run replay:
	@set -eu; \
	if [ '$@' = run ] && [ '$(FUZZ_COVERAGE)' = 1 ]; then mkdir -p '$(OUT)'; : > '$(OUT)/coverage-runs.tsv'; fi; \
	if [ '$@' = run ]; then \
	  case '$(FUZZ_SECONDS)' in ''|*[!0-9]*) echo 'FUZZ_SECONDS must be an integer from 1 to 86400' >&2; exit 1 ;; esac; \
	  if [ '$(FUZZ_SECONDS)' -lt 1 ] || [ '$(FUZZ_SECONDS)' -gt 86400 ]; then echo 'FUZZ_SECONDS must be from 1 to 86400' >&2; exit 1; fi; \
	fi; \
	for target in $(FUZZ_TARGETS); do \
	  case "$$target" in messages|values) source=../../src/hwire.c; variants='$(PARSER_VARIANTS)' ;; table) source=../../src/hwire_table.c; variants='native siphash' ;; *) echo "Unknown target: $$target" >&2; exit 1 ;; esac; \
	  allowed="$$variants"; \
	  if [ '$(FUZZ_VARIANTS)' != auto ]; then variants='$(FUZZ_VARIANTS)'; fi; \
	  for variant in $$variants; do \
	    case " $$allowed " in *" $$variant "*) ;; *) echo "Unsupported variant: $$variant ($(ARCH))" >&2; exit 1 ;; esac; \
	    case "$$variant" in scalar) flags='$(FLAGS_scalar)' ;; sse2) flags='$(FLAGS_sse2)' ;; sse42) flags='$(FLAGS_sse42)' ;; neon) flags='$(FLAGS_neon)' ;; native) flags='$(FLAGS_native)' ;; siphash) flags='$(FLAGS_siphash)' ;; esac; \
	    dir='$(OUT)'/$$target/$$variant; mkdir -p "$$dir/artifacts"; \
	    echo "=== $$target / $$variant: build ==="; \
	    if [ '$@' = run ] && [ '$(FUZZ_COVERAGE)' = 1 ]; then \
	      printf '%s\t%s\t%s\n' "$$target" "$$variant" '$(FUZZ_SECONDS)' >> '$(OUT)/coverage-runs.tsv'; \
	      rm -f "$$dir/exit-code" "$$dir/run.log"; \
	    fi; \
	    $(CC) $(CFLAGS) $(COVERAGE_FLAGS) $$flags $(SANITIZERS) $$target.c $$source -o "$$dir/fuzz" >"$$dir/build.log" 2>&1 || { cat "$$dir/build.log"; exit 1; }; \
	    if [ '$@' = build ]; then continue; fi; \
	    if [ '$@' = replay ]; then \
	      echo "=== $$target / $$variant: replay ==="; \
	      if [ -n "$(FUZZ_INPUT)" ]; then "$$dir/fuzz" "$(FUZZ_INPUT)"; else "$$dir/fuzz" -runs=0 seeds/$$target/*; fi; \
	    else \
	      mkdir -p "$$dir/corpus"; cp seeds/$$target/* "$$dir/corpus/"; \
	      echo "=== $$target / $$variant: $(FUZZ_SECONDS) seconds ==="; \
	      status=0; \
	      LLVM_PROFILE_FILE="$$dir/%m.profraw" "$$dir/fuzz" "$$dir/corpus" -max_total_time=$(FUZZ_SECONDS) -timeout=2 -max_len=8192 -rss_limit_mb=512 -artifact_prefix="$$dir/artifacts/" >"$$dir/run.log" 2>&1 || status=$$?; \
	      printf '%s\n' "$$status" >"$$dir/exit-code"; \
	      tail -n 12 "$$dir/run.log"; \
	      if [ $$status -ne 0 ]; then echo "FAILED: $$target / $$variant (exit $$status)" >&2; exit $$status; fi; \
	      echo "PASS: $$target / $$variant"; \
	    fi; \
	  done; \
	done
# Keep coverage binaries and profiles separate from ordinary fuzz runs.
coverage:
	@status=0; $(MAKE) run FUZZ_COVERAGE=1 OUT='$(OUT)/coverage' || status=$$?; \
	$(PYTHON) coverage.py '$(OUT)/coverage' || exit $$?; exit $$status

coverage-report:
	$(PYTHON) coverage.py '$(OUT)/coverage'

clean:
	rm -rf $(OUT)
