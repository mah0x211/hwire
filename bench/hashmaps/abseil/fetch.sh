#!/bin/sh
# Fetch the fixed upstream revision before building or timing.
set -eu
cd "$(dirname "$0")"
revision=2065f4ded0558c6f89fee67c8e5228feb4eb960e
if [ -f deps/.revision ] && [ "$(cat deps/.revision)" = "$revision" ]; then
    exit 0
fi
tmp=$(mktemp -d ./fetch.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
curl -fL --retry 2 "https://codeload.github.com/abseil/abseil-cpp/tar.gz/$revision" -o "$tmp/source.tar.gz"
mkdir "$tmp/deps"
tar -xzf "$tmp/source.tar.gz" -C "$tmp/deps" --strip-components=1
printf '%s\n' "$revision" > "$tmp/deps/.revision"
rm -rf deps
mv "$tmp/deps" deps
