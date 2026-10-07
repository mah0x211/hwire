#!/bin/sh
# Fetch the pinned upstream source before building or timing.
set -eu
cd "$(dirname "$0")"
revision=0cd8b8151c0a258352fe328f95cb4f5e85837223
if ! { [ -f deps/.revision ] && [ "$(cat deps/.revision)" = "$revision" ]; }; then
    tmp=$(mktemp -d ./fetch.XXXXXX)
    trap 'rm -rf "$tmp"' EXIT HUP INT TERM
    curl -fL --retry 2 "https://codeload.github.com/ShogunPanda/milo/tar.gz/$revision" -o "$tmp/source.tar.gz"
    mkdir "$tmp/deps"
    tar -xzf "$tmp/source.tar.gz" -C "$tmp/deps" --strip-components=1
    printf '%s\n' "$revision" > "$tmp/deps/.revision"
    rm -rf deps
    mv "$tmp/deps" deps
fi

"${CARGO:-cargo}" fetch --manifest-path Cargo.toml --locked
