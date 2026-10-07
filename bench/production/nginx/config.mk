nginx_SOURCES := nginx/runtime.c nginx/deps/src/http/ngx_http_parse.c nginx/deps/src/core/ngx_hash.c nginx/deps/src/core/ngx_list.c nginx/deps/src/core/ngx_palloc.c nginx/deps/src/core/ngx_string.c nginx/deps/src/os/unix/ngx_alloc.c
nginx_CPPFLAGS := -Inginx/deps/objs -Inginx/deps/src/core -Inginx/deps/src/event -Inginx/deps/src/event/modules -Inginx/deps/src/os/unix -Inginx/deps/src/http -Inginx/deps/src/http/modules
# Drop unrelated functions in the upstream translation unit at link time.
nginx_CFLAGS := -ffunction-sections -fdata-sections
ifeq ($(shell uname -s),Darwin)
nginx_LDLIBS := -Wl,-dead_strip
else
nginx_LDLIBS := -Wl,--gc-sections
endif

nginx_NAME := nginx
nginx_BUILD_DEPS := nginx/gen_headers.py nginx/bin/header_keys.h
nginx_VARIANTS := nosimd native
