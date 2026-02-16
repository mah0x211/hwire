#!/bin/bash

# Script to restore original library files from backup
# Usage: ./scripts/restore-no-simd.sh <hwire|pico|llhttp>

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <hwire|pico|llhttp>"
    exit 1
fi

LIBRARY=$1

echo "Restoring original files for $LIBRARY..."

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

# Check if backup exists
if [ ! -f "$FILE.backup" ]; then
    echo "Warning: No backup file found for $FILE"
    exit 0
fi

# Restore from backup
mv "$FILE.backup" "$FILE"
echo "Restored $LIBRARY successfully"