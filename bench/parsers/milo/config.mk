milo_LDLIBS = milo/bin/%/release/libmilo_adapter.a
ifeq ($(shell uname -s),Linux)
milo_LDLIBS += -ldl -lpthread
endif
milo_PARSE_NAME := milo
milo_BUILD_INFO = $(shell rustc --version); opt-level=2; panic=abort; target-cpu=native; native parser reset/reuse; upstream memchr SIMD dispatch; no scalar switch
milo_BUILD_DEPS := milo/parser.rs milo/Cargo.toml milo/Cargo.lock milo/deps/parser/src/lib.rs milo/deps/parser/src/parse.rs
milo_VARIANTS := native

# Dependency environment belongs to this adapter.
milo_ENV = CARGO="$(CARGO)" $(if $(filter 1,$(INSTALL_DEPS)),RUSTUP_TOOLCHAIN=$(or $(RUSTUP_TOOLCHAIN),1.88.0),)
