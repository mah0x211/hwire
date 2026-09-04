#!/bin/sh
# Collect platform information into results/platform.txt at measurement
# time, so the comparison report always shows the environment that
# produced the numbers. Usage:
#
#     platform.sh <outfile> <cc-command> <cflags>
#
# <cc-command>/<cflags> record what the build actually used. Every probe
# is optional: missing tools or files just omit the line.

set -u

out=${1:-results/platform.txt}
cc=${2:-cc}
cflags=${3:-}
mkdir -p "$(dirname "$out")"

{
    echo "date: $(date -Iseconds 2>/dev/null || date)"
    echo "uname: $(uname -srm 2>/dev/null)"

    fmt_clock_hz() {
        # integer Hz -> GHz with two decimals
        hz=$1
        [ -n "$hz" ] && [ "$hz" -gt 0 ] 2>/dev/null \
            && awk -v h="$hz" 'BEGIN { printf "%.2f GHz", h / 1e9 }'
    }

    fmt_cache() {
        # bytes -> MiB/KiB
        bytes=$1
        [ -n "$bytes" ] || return 0
        if [ "$bytes" -ge 1048576 ]; then
            echo "$((bytes / 1048576)) MiB"
        else
            echo "$((bytes / 1024)) KiB"
        fi
    }

    if [ "$(uname -s)" = "Darwin" ]; then
        sw_vers -productName 2>/dev/null \
            | while read -r name; do echo "os: $name $(sw_vers -productVersion 2>/dev/null)"; done
        echo "cpu: $(sysctl -n machdep.cpu.brand_string 2>/dev/null)"
        clock=$(fmt_clock_hz "$(sysctl -n hw.cpufrequency 2>/dev/null)")
        [ -n "$clock" ] && echo "clock: $clock"
        echo "cores: $(sysctl -n hw.ncpu 2>/dev/null)"
        mem=$(sysctl -n hw.memsize 2>/dev/null)
        [ -n "$mem" ] && echo "memory: $((mem / 1024 / 1024)) MiB"
        for lvl in l1i l1d l2 l3; do
            sz=$(fmt_cache "$(sysctl -n hw.$lvl"cachesize" 2>/dev/null)")
            [ -n "$sz" ] && echo "cache $lvl: $sz"
        done
    elif [ -r /proc/cpuinfo ]; then
        if [ -r /etc/os-release ]; then
            . /etc/os-release
            echo "os: ${PRETTY_NAME:-unknown}"
        fi
        cpu=$(grep -m1 -E '^(model name|Model)' /proc/cpuinfo 2>/dev/null \
            | cut -d: -f2- | sed 's/^ *//')
        [ -n "$cpu" ] && echo "cpu: $cpu"
        mhz=$(awk -F: '/cpu MHz/ { if ($2 + 0 > m) m = $2 + 0 }
                       END { printf "%.0f", m }' /proc/cpuinfo 2>/dev/null)
        [ -n "$mhz" ] && [ "$mhz" -gt 0 ] 2>/dev/null \
            && echo "clock: $(awk -v m="$mhz" 'BEGIN { printf "%.2f GHz", m / 1000 }')"
        echo "cores: $(nproc 2>/dev/null)"
        mem=$(grep -m1 '^MemTotal:' /proc/meminfo 2>/dev/null | awk '{print $2}')
        [ -n "$mem" ] && echo "memory: $((mem / 1024)) MiB"
        # per-level cache sizes from sysfs (type/level/size per index)
        for idx in /sys/devices/system/cpu/cpu0/cache/index*; do
            [ -r "$idx/level" ] || continue
            level=$(cat "$idx/level" 2>/dev/null)
            type=$(cat "$idx/type" 2>/dev/null)
            size=$(cat "$idx/size" 2>/dev/null)
            [ -n "$level" ] && [ -n "$size" ] \
                && echo "cache l$level${type:+-$type}: $size"
        done | sort -u
        if command -v systemd-detect-virt >/dev/null 2>&1; then
            virt=$(systemd-detect-virt 2>/dev/null)
            [ "$virt" != "none" ] && [ -n "$virt" ] && echo "virtualization: $virt"
        fi
    fi

    echo "compiler: $($cc --version 2>/dev/null | head -1)"
    [ -n "$cflags" ] && echo "cflags: $cflags"
} > "$out"
