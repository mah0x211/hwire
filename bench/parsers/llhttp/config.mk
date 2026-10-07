llhttp_SOURCES := llhttp/deps/src/api.c llhttp/deps/src/http.c llhttp/deps/src/llhttp.c
llhttp_CPPFLAGS := -Illhttp/deps/include
llhttp_PARSE_NAME := llhttp (Node.js)
ifeq ($(UNAME_M),x86_64)
llhttp_VARIANTS := nosimd sse42 native
else
llhttp_VARIANTS := nosimd native
endif
