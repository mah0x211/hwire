#!/bin/bash

# Script to apply NO_SIMD macro to specified library files
# Usage: ./scripts/apply-no-simd.sh <hwire|pico|llhttp>

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <hwire|pico|llhttp>"
    exit 1
fi

LIBRARY=$1
MARKER="#if defined(NO_SIMD)"

echo "Applying NO_SIMD macro to $LIBRARY..."

case $LIBRARY in
    hwire)
        FILE="deps/hwire/hwire.c"
        ;;
    pico)
        FILE="deps/picohttpparser/picohttpparser.c"
        ;;
    llhttp)
        FILE="deps/llhttp/build/c/llhttp.c"
        ;;
    *)
        echo "Error: Unknown library '$LIBRARY'. Use hwire, pico, or llhttp."
        exit 1
        ;;
esac

# Check if file exists
if [ ! -f "$FILE" ]; then
    echo "Error: File '$FILE' not found"
    exit 1
fi

# Check if already patched
if head -n 1 "$FILE" | grep -q "^$MARKER$"; then
    echo "$LIBRARY is already patched"
    exit 0
fi

# Backup the original file
cp "$FILE" "$FILE.backup"

# Apply NO_SIMD macro at the beginning of the file
cat > "$FILE.tmp" << 'EOF'
#if defined(NO_SIMD)
#undef __SSE2__
#undef __SSE4_2__
#undef __AVX2__
#undef __aarch64__
#undef __arm__
#undef __ARM_NEON
#undef __ARM_NEON__
#endif

EOF
cat "$FILE" >> "$FILE.tmp"
mv "$FILE.tmp" "$FILE"

echo "Applied NO_SIMD macro to $LIBRARY successfully"