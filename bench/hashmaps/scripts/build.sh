#!/bin/sh
# Build adapter objects or a standalone timing binary.
set -eu
variant=$1
extra=$2
suite=$3
dir="bin/$variant"
mkdir -p "$dir"
case "$(uname -s)" in
    Darwin) dead_strip='-Wl,-dead_strip' ;;
    *) dead_strip='-Wl,--gc-sections' ;;
esac
compile_objects()
{
    objects=
    for src; do
        owner=${src%%/*}
        cppflags=$(printenv "${owner}_CPPFLAGS" || :)
        cflags=$(printenv "${owner}_CFLAGS" || :)
        cxxflags=$(printenv "${owner}_CXXFLAGS" || :)
        obj="$objdir/$(echo "$src" | sed 's|[^A-Za-z0-9]|_|g').o"
        case "$src" in
            *.cpp) $CXX $CXXFLAGS $extra $INC $cppflags $cxxflags -c "$src" -o "$obj" ;;
            *.cc) $CXX $CXXFLAGS $extra $INC $cppflags $cxxflags -ffunction-sections -fdata-sections -c "$src" -o "$obj" ;;
            *) $CC $CFLAGS $extra $INC $cppflags $cflags -c "$src" -o "$obj" ;;
        esac
        objects="$objects $obj"
    done
}

case "$suite" in
    adapters)
        objdir="$dir/adapters"
        mkdir -p "$objdir"
        compile_objects shared/hashmap_hash.c $SRCS
        rm -f "$dir/libhashmaps.a"
        $AR rcs "$dir/libhashmaps.a" $objects
        ;;
    hashmap)
        $CC $CFLAGS $extra $INC -I"$dir" -c bench_hashmap.c -o "$dir/driver.o"
        $CXX "$dir/driver.o" "$dir/libhashmaps.a" $dead_strip $LDLIBS -lm -o "$dir/bench_hashmap"
        ;;
    *) echo "unknown hashmap build: $suite" >&2; exit 1 ;;
esac
