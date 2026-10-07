# % expands to the plain build configuration in the timing binary's link flags.
actix_web_LDLIBS = actix_web/bin/%/release/libactix_web_adapter.a
ifeq ($(shell uname -s),Linux)
actix_web_LDLIBS += -ldl -lpthread
endif
actix_web_PARSE_NAME := Actix Web
actix_web_BUILD_INFO = $(shell rustc --version); opt-level=2; panic=abort; scalar for nosimd, compile-time SSE4.2 for sse42, NEON for neon, host CPU including AVX2 for native
actix_web_BUILD_DEPS := actix_web/parser.rs actix_web/Cargo.toml actix_web/Cargo.lock
ifeq ($(UNAME_M),x86_64)
actix_web_VARIANTS := nosimd sse42 native
else
actix_web_VARIANTS := nosimd neon native
endif
