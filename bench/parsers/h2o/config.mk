h2o_SOURCES := h2o/deps/deps/picohttpparser/picohttpparser.c
h2o_CPPFLAGS := -Ih2o/deps/deps/picohttpparser
h2o_PARSE_NAME := H2O
ifeq ($(UNAME_M),x86_64)
h2o_VARIANTS := nosimd sse42 native
else
h2o_VARIANTS := nosimd native
endif
