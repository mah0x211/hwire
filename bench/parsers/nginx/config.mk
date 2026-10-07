nginx_SOURCES := nginx/deps/src/http/ngx_http_parse.c nginx/deps/src/core/ngx_string.c
nginx_CPPFLAGS := -Inginx/deps/objs -Inginx/deps/src/core -Inginx/deps/src/event -Inginx/deps/src/event/modules -Inginx/deps/src/os/unix -Inginx/deps/src/http -Inginx/deps/src/http/modules
# Drop unrelated functions in the upstream translation unit at link time.
nginx_CFLAGS := -ffunction-sections -fdata-sections
ifeq ($(shell uname -s),Darwin)
nginx_LDLIBS := -Wl,-dead_strip
else
nginx_LDLIBS := -Wl,--gc-sections
endif
ifeq ($(UNAME_M),x86_64)
nginx_VARIANTS := nosimd native
else
nginx_VARIANTS := nosimd native
endif
