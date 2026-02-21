#!/bin/bash
# Usage: bash scripts/run-bench.sh <result-file> <tag-filter> <binary> [<binary2>...]
# Runs each binary with the given Catch2 tag filter, appends JSONL output.
# Tag filter example: "[header-count][8-headers]"
set -e

RESULT_FILE="$1"
TAG_FILTER="$2"
shift 2

mkdir -p "$(dirname "$RESULT_FILE")"
rm -f "$RESULT_FILE"

for target in "$@"; do
    echo "Running $target $TAG_FILTER..." >&2
    ./"$target" "$TAG_FILTER" --reporter xml 2>/dev/null | node scripts/xml2json.js >> "$RESULT_FILE"
done
