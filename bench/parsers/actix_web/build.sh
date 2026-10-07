#!/bin/sh
# Build one static library with the same instruction target as the C adapters.
set -eu
variant=$1
cd "$(dirname "$0")"
cargo=${CARGO:-cargo}
flags='-C target-cpu=generic'
disable=0
case "$variant" in
    native|siphash) flags="-C target-cpu=native" ;;
    nosimd) disable=1 ;;
    sse2) disable=1; flags="$flags -C target-feature=+sse2,-sse4.2,-avx2" ;;
    sse42) flags="$flags -C target-feature=+sse4.2,-avx2" ;;
    neon) flags="$flags -C target-feature=+neon" ;;
esac
CARGO_CFG_HTTPARSE_DISABLE_SIMD=$disable RUSTFLAGS="$flags" \
    "$cargo" build --release --offline --target-dir "bin/$variant"
