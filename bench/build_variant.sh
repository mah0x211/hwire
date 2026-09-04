#!/bin/sh
# Build one benchmark variant: compile every registered parser source and
# main.c with the variant's flags, then link bin/<variant>/bench.
#
# Usage (invoked by the Makefile):
#     build_variant.sh <variant> <extra-cflags>
#
# Environment: CC, CFLAGS, SRCS (parser sources incl. ../src/hwire.c),
# INC (include flags).

set -eu

variant=$1
extra=$2

mkdir -p "bin/$variant/obj"

for src in $SRCS; do
    obj="bin/$variant/obj/$(echo "$src" | sed 's|[^A-Za-z0-9]|_|g').o"
    # shellcheck disable=SC2086
    $CC $CFLAGS $extra $INC -c "$src" -o "$obj"
done

# shellcheck disable=SC2086
$CC $CFLAGS -I. -Ibin -I"bin/$variant" $INC main.c \
    bin/$variant/obj/*.o -o "bin/$variant/bench"
