#!/bin/sh
# Prepare the Rust toolchain declared by this adapter.
set -eu
# Load the check/install mode and shared dependency-check helpers.
. "$(dirname "$0")/../../shared/scripts/dependencies.sh"
minimum=1.88.0

rust_ready()
{
    version_at_least "$minimum" rustc --version && "${CARGO:-cargo}" --version >/dev/null 2>&1
}

if rust_ready; then exit 0; fi
if [ "$mode" != install ]; then
    echo "Requires rustc and Cargo $minimum+; use INSTALL_DEPS=1 to install." >&2
    exit 1
fi
command -v rustup >/dev/null 2>&1 || { echo 'Requires rustup to install this adapter toolchain.' >&2; exit 1; }
export RUSTUP_TOOLCHAIN=${RUSTUP_TOOLCHAIN:-$minimum}
rustup toolchain install "$RUSTUP_TOOLCHAIN" --profile minimal
rust_ready
