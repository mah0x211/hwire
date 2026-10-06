#!/bin/sh
# Fetch the fixed upstream revision before building or timing.
set -eu
cd "$(dirname "$0")"
revision=2d62942eb2369b5387e0c4f2aa06f052004d16e0
if [ -f deps/.revision ] && [ "$(cat deps/.revision)" = "$revision" ]; then
    exit 0
fi
tmp=$(mktemp -d ./fetch.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
curl -fL --retry 2 "https://codeload.github.com/JacksonAllan/CC/tar.gz/$revision" -o "$tmp/source.tar.gz"
mkdir "$tmp/deps"
tar -xzf "$tmp/source.tar.gz" -C "$tmp/deps" --strip-components=1
printf '%s\n' "$revision" > "$tmp/deps/.revision"
rm -rf deps
mv "$tmp/deps" deps
