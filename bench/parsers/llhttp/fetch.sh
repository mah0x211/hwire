#!/bin/sh
# Fetch the fixed upstream revision before building or timing.
set -eu
cd "$(dirname "$0")"
revision=0e815792b167a9bd8ace259b95b7da953776c288
if ! { [ -f deps/.revision ] && [ "$(cat deps/.revision)" = "$revision" ]; }; then
    tmp=$(mktemp -d ./fetch.XXXXXX)
    trap 'rm -rf "$tmp"' EXIT HUP INT TERM
    curl -fL --retry 2 "https://codeload.github.com/nodejs/llhttp/tar.gz/$revision" -o "$tmp/source.tar.gz"
    mkdir "$tmp/deps"
    tar -xzf "$tmp/source.tar.gz" -C "$tmp/deps" --strip-components=1
    printf '%s\n' "$revision" > "$tmp/deps/.revision"
    rm -rf deps
    mv "$tmp/deps" deps
fi
