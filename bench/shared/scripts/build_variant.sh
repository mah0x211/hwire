#!/bin/sh
# Compile the active adapters, their native sources and one standalone driver.
set -eu
variant=$1
extra=$2
dir="bin/$variant"
mkdir -p "$dir/objects"
objects=
for src in $SRCS; do
    obj="$dir/objects/$(echo "$src" | sed 's|[^A-Za-z0-9]|_|g').o"
    owner=${src%%/*}
    flags=$(printenv "${owner}_CFLAGS" || true)
    $CC $CFLAGS $extra $flags $INC -MMD -MP -MT "$obj" -MT "$dir/${DRIVER%.c}" -c "$src" -o "$obj"
    objects="$objects $obj"
done
pool_flags=
pool_link=
if [ "${POOL:-0}" = 1 ]; then
    system=$(uname -s)
    case "$system" in
        Linux|Darwin)
            pool_flags='-DBENCH_POOL -Ishared'
            $CC $CFLAGS $extra -Ishared -MMD -MP -MT "$dir/objects/pool.o" -MT "$dir/${DRIVER%.c}" -c shared/pool.c -o "$dir/objects/pool.o"
            ;;
    esac
    case "$system" in
        Linux)
            objects="$objects $dir/objects/pool.o"
            for symbol in malloc calloc realloc aligned_alloc posix_memalign free; do
                pool_link="$pool_link -Wl,--wrap=$symbol"
            done
            ;;
        Darwin)
            $CC $CFLAGS $extra -MMD -MP -MT "$dir/objects/pool_darwin.o" -MT "$dir/${DRIVER%.c}" -c shared/pool_darwin.c -o "$dir/objects/pool_darwin.o"
            $CC -dynamiclib "$dir/objects/pool.o" "$dir/objects/pool_darwin.o" -Wl,-install_name,@rpath/libbench_pool.dylib -o "$dir/libbench_pool.dylib"
            pool_link="-L$dir -lbench_pool -Wl,-rpath,@loader_path"
            ;;
    esac
fi
$CC $CFLAGS $extra $pool_flags $INC -I. -Ibin -I"$dir" -MMD -MP -MT "$dir/objects/driver.o" -MT "$dir/${DRIVER%.c}" -c "$DRIVER" -o "$dir/objects/driver.o"
$CC "$dir/objects/driver.o" $objects $pool_link $LDLIBS -lm -o "$dir/${DRIVER%.c}"
