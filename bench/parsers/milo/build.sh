#!/bin/sh
# Milo exposes SIMD through memchr; no standard scalar switch is available.
set -eu
variant=$1
cd "$(dirname "$0")"
if [ "$variant" != native ]; then
    echo "milo supports the native benchmark configuration only." >&2
    exit 1
fi
RUSTFLAGS="-C target-cpu=native" "${CARGO:-cargo}" build --release --locked --offline --target-dir "bin/$variant"
