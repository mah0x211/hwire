#!/bin/sh
# Match the suite's instruction target without modifying upstream sources.
set -eu
variant=$1
cd "$(dirname "$0")"
zig=${ZIG:-zig}
case "$("$zig" version)" in
    0.15.*) ;;
    *) echo "hparse requires Zig 0.15.x; set ZIG to a compatible executable." >&2; exit 1 ;;
esac
case "$variant" in
    nosimd)
        case "$(uname -m)" in
            x86_64) cpu=baseline-sse-sse2 ;;
            *) cpu=baseline-neon ;;
        esac ;;
    sse2) cpu=baseline ;;
    neon) cpu=baseline+neon ;;
    native) cpu=native ;;
esac
mkdir -p "bin/$variant"
"$zig" build-lib -fPIC -femit-bin="bin/$variant/libhparse_adapter.a" \
    -O ReleaseFast -mcpu="$cpu" --dep hparse -Mroot=parser.zig \
    -O ReleaseFast -mcpu="$cpu" -Mhparse=deps/src/root.zig
