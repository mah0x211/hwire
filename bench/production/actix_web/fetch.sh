#!/bin/sh
# Fetch the fixed upstream revision before building or timing.
set -eu
cd "$(dirname "$0")"
revision=714572c5eca7a012b771dd9a978767e7573697a8
if ! { [ -f deps/.revision ] && [ "$(cat deps/.revision)" = "$revision" ]; }; then
    tmp=$(mktemp -d ./fetch.XXXXXX)
    trap 'rm -rf "$tmp"' EXIT HUP INT TERM
    curl -fL --retry 2 "https://codeload.github.com/actix/actix-web/tar.gz/$revision" -o "$tmp/source.tar.gz"
    mkdir "$tmp/deps"
    tar -xzf "$tmp/source.tar.gz" -C "$tmp/deps" --strip-components=1
    printf '%s\n' "$revision" > "$tmp/deps/.revision"
    rm -rf deps
    mv "$tmp/deps" deps
    rm -rf "$tmp"
fi
${CARGO:-cargo} fetch --manifest-path Cargo.toml --locked
