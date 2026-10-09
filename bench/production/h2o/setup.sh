#!/bin/sh
# H2O headers use OpenSSL and the libuv socket binding on Linux and macOS.
set -eu
# Load the check/install mode and shared dependency-check helpers.
. "$(dirname "$0")/../../shared/scripts/dependencies.sh"
if ! require_command pkg-config; then
    require_apt_package pkg-config
    require_brew_package pkg-config
fi
if ! require_package openssl; then
    require_apt_package libssl-dev
    require_brew_package openssl
fi
if ! require_package libuv; then
    require_apt_package libuv1-dev
    require_brew_package libuv
fi
install_required_packages
pkg-config --exists openssl libuv
