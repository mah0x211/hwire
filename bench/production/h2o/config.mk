h2o_SOURCES := h2o/deps/deps/picohttpparser/picohttpparser.c h2o/deps/lib/core/headers.c h2o/deps/lib/core/request.c h2o/deps/lib/common/token.c h2o/deps/lib/common/memory.c h2o/deps/lib/common/string.c h2o/deps/lib/common/file.c h2o/deps/lib/common/socket.c
h2o_CPPFLAGS := -Ih2o/deps/deps/picohttpparser -Ih2o/deps/include -Ih2o/deps/deps/picotls/include -Ih2o/deps/deps/quicly/include
# OpenSSL headers may live outside the compiler search path on macOS.
h2o_CPPFLAGS += $(shell pkg-config --cflags openssl 2>/dev/null)
h2o_CFLAGS := -D_GNU_SOURCE -ffunction-sections -fdata-sections
h2o_CPPFLAGS += $(shell pkg-config --cflags libuv 2>/dev/null)
ifeq ($(shell uname -s),Darwin)
h2o_LDLIBS := -Wl,-dead_strip
else
h2o_LDLIBS := -Wl,--gc-sections
endif
h2o_NAME := H2O
ifeq ($(UNAME_M),x86_64)
h2o_VARIANTS := nosimd sse42 native
else
h2o_VARIANTS := nosimd native
endif
