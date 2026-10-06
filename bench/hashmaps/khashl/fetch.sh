#!/bin/sh
# Fetch the fixed upstream revision before building or timing.
set -eu
cd "$(dirname "$0")"
revision=9e86261a66015c56c596ef31beee66d96dec5920
if [ -f deps/.revision ] && [ "$(cat deps/.revision)" = "$revision" ]; then
    exit 0
fi
tmp=$(mktemp -d ./fetch.XXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
curl -fL --retry 2 "https://codeload.github.com/attractivechaos/khashl/tar.gz/$revision" -o "$tmp/source.tar.gz"
mkdir "$tmp/deps"
tar -xzf "$tmp/source.tar.gz" -C "$tmp/deps" --strip-components=1
printf '%s\n' "$revision" > "$tmp/deps/.revision"
rm -rf deps
mv "$tmp/deps" deps
