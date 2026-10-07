hparse_LDLIBS = hparse/bin/%/libhparse_adapter.a
hparse_PARSE_NAME := hparse
hparse_BUILD_INFO = $(shell $(ZIG) version); ReleaseFast; CPU baseline without SSE/SSE2 or NEON for nosimd, baseline for sse2, NEON for neon, host CPU for native
hparse_BUILD_DEPS := hparse/parser.zig hparse/deps/src/root.zig
ifeq ($(UNAME_M),x86_64)
hparse_VARIANTS := nosimd sse2 native
else
hparse_VARIANTS := nosimd neon native
endif
