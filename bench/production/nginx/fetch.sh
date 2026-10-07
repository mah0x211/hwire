#!/bin/sh
# Fetch the fixed upstream revision before building or timing.
set -eu
cd "$(dirname "$0")"
revision=45a318d05a0fd23f57ffe9579f7f0969c0fe402a
if ! { [ -f deps/.revision ] && [ "$(cat deps/.revision)" = "$revision" ]; }; then
    tmp=$(mktemp -d ./fetch.XXXXXX)
    trap 'rm -rf "$tmp"' EXIT HUP INT TERM
    curl -fL --retry 2 "https://codeload.github.com/nginx/nginx/tar.gz/$revision" -o "$tmp/source.tar.gz"
    mkdir "$tmp/deps"
    tar -xzf "$tmp/source.tar.gz" -C "$tmp/deps" --strip-components=1
    printf '%s\n' "$revision" > "$tmp/deps/.revision"
    rm -rf deps
    mv "$tmp/deps" deps
fi
if ! grep -q '^#define NGX_COMPAT  *1' deps/objs/ngx_auto_config.h 2>/dev/null; then
    (cd deps && ./auto/configure --with-compat --without-http_rewrite_module --without-http_gzip_module --without-pcre > configure.log 2>&1) || {
        cat deps/configure.log
        exit 1
    }
fi
python3 gen_headers.py
