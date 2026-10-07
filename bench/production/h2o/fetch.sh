#!/bin/sh
# Fetch the fixed upstream revision before building or timing.
set -eu
cd "$(dirname "$0")"
revision=5da50541a4b6a038c9cea493f740747cf964f4a8
if ! { [ -f deps/.revision ] && [ "$(cat deps/.revision)" = "$revision" ]; }; then
    tmp=$(mktemp -d ./fetch.XXXXXX)
    trap 'rm -rf "$tmp"' EXIT HUP INT TERM
    curl -fL --retry 2 "https://codeload.github.com/h2o/h2o/tar.gz/$revision" -o "$tmp/source.tar.gz"
    mkdir "$tmp/deps"
    tar -xzf "$tmp/source.tar.gz" -C "$tmp/deps" --strip-components=1
    printf '%s\n' "$revision" > "$tmp/deps/.revision"
    rm -rf deps
    mv "$tmp/deps" deps
    rm -rf "$tmp"
fi
