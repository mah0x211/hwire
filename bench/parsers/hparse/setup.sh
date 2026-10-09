#!/bin/sh
# Install the pinned Zig toolchain in this adapter's ignored build directory.
set -eu
cd "$(dirname "$0")"
# Load the check/install mode and shared dependency-check helpers.
. "../../shared/scripts/dependencies.sh"

expose_toolchain()
{
    # Subsequent Actions steps can benchmark older refs with the same toolchain.
    if [ -n "${GITHUB_PATH:-}" ]; then
        dirname "$(command -v "$zig")" >> "$GITHUB_PATH"
    fi
}

zig=${ZIG:-zig}
if [ "$zig" = zig ] && [ -x bin/toolchain/zig ]; then
    zig=$PWD/bin/toolchain/zig
fi
case "$("$zig" version 2>/dev/null || :)" in 0.15.*) expose_toolchain; exit 0 ;; esac
if [ "$mode" != install ]; then
    echo 'Requires Zig 0.15.x; set ZIG or use INSTALL_DEPS=1.' >&2
    exit 1
fi
if [ "$zig" != zig ] && [ "$zig" != "$PWD/bin/toolchain/zig" ]; then
    echo "The selected ZIG must be version 0.15.x: $zig" >&2
    exit 1
fi
if ! require_command xz; then
    require_apt_package xz-utils
    require_brew_package xz
fi
install_required_packages
version=0.15.1
case "$(uname -m)" in arm64|aarch64) arch=aarch64 ;; x86_64) arch=x86_64 ;; *) echo 'Unsupported Zig architecture.' >&2; exit 1 ;; esac
case "$(uname -s)" in Darwin) os=macos ;; Linux) os=linux ;; *) echo 'Unsupported Zig OS.' >&2; exit 1 ;; esac
mkdir -p bin
work=$(mktemp -d ./bin/toolchain.XXXXXX)
trap 'rm -rf "$work"' EXIT HUP INT TERM
curl -fL --retry 2 https://ziglang.org/download/index.json -o "$work/index.json"
python3 - "$work/index.json" "$version" "$arch-$os" > "$work/download" <<'PY'
import json, sys
with open(sys.argv[1]) as stream:
    archive = json.load(stream)[sys.argv[2]][sys.argv[3]]
print(archive['tarball'])
print(archive['shasum'])
PY
url=$(sed -n '1p' "$work/download")
checksum=$(sed -n '2p' "$work/download")
curl -fL --retry 2 "$url" -o "$work/zig.tar.xz"
python3 - "$work/zig.tar.xz" "$checksum" <<'PY'
import hashlib, pathlib, sys
if hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest() != sys.argv[2]:
    raise SystemExit('Zig archive checksum mismatch')
PY
mkdir "$work/extracted"
tar -xf "$work/zig.tar.xz" -C "$work/extracted" --strip-components=1
rm -rf bin/toolchain
mv "$work/extracted" bin/toolchain

zig=$PWD/bin/toolchain/zig
"$zig" version
expose_toolchain
