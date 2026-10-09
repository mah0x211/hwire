#!/bin/sh
# Sourced by adapter setup.sh scripts; see ../README.md for the helper contract.
# Read the caller's check/install argument and start an empty dependency queue.
# Loading this file does not install packages.
set -eu
mode=${1:-check}
case "$mode" in check|install) ;; *) echo 'Usage: setup.sh check|install' >&2; exit 1 ;; esac
dependency_os=$(uname -s)
required_packages=

# require_command COMMAND
# Return success if COMMAND is available; do not queue or install packages.
require_command()
{
    command -v "$1" >/dev/null 2>&1
}

# require_package PKG_CONFIG_NAME
# Return success if pkg-config can find the package; do not queue or install.
require_package()
{
    pkg-config --exists "$1" 2>/dev/null
}

# require_apt_package PACKAGE
# Queue a missing dependency's apt package on Linux; ignore it on macOS.
require_apt_package()
{
    case "$dependency_os" in
        Linux) required_packages="$required_packages $1" ;;
        Darwin) ;;
        *) echo 'Package setup is unsupported on this OS.' >&2; return 1 ;;
    esac
}

# require_brew_package FORMULA
# Queue a missing dependency's Homebrew formula on macOS; ignore it on Linux.
require_brew_package()
{
    case "$dependency_os" in
        Darwin) required_packages="$required_packages $1" ;;
        Linux) ;;
        *) echo 'Package setup is unsupported on this OS.' >&2; return 1 ;;
    esac
}

# install_required_packages
# Check mode reports queued packages and fails without installing. Install mode
# installs them through apt-get or Homebrew; callers then recheck requirements.
install_required_packages()
{
    if [ -z "$required_packages" ]; then return 0; fi
    if [ "$mode" != install ]; then
        echo "Missing packages:$required_packages; use INSTALL_DEPS=1 to install." >&2
        return 1
    fi
    case "$dependency_os" in
        Linux)
            command -v apt-get >/dev/null 2>&1 || { echo 'Automatic installation requires apt-get.' >&2; return 1; }
            if [ "$(id -u)" = 0 ]; then
                apt-get update
                apt-get install -y $required_packages
            else
                sudo apt-get update
                sudo apt-get install -y $required_packages
            fi ;;
        Darwin)
            command -v brew >/dev/null 2>&1 || { echo 'Automatic installation requires Homebrew.' >&2; return 1; }
            brew install $required_packages ;;
        *) echo 'Automatic package installation is unsupported on this OS.' >&2; return 1 ;;
    esac
}

# version_at_least MINIMUM COMMAND [ARGUMENT ...]
# Return success if stdout contains a version >= MINIMUM (missing patch = 0).
# Command failure, an unreadable version, or an older version returns nonzero.
version_at_least()
(
    minimum=$1
    shift
    output=$("$@" 2>/dev/null) || return 1
    python3 -c 'import re, sys
match = re.search(r"(\d+)\.(\d+)(?:\.(\d+))?", sys.argv[2])
actual = tuple(int(part or 0) for part in match.groups()) if match else ()
minimum = tuple(map(int, sys.argv[1].split(".")))
minimum += (0,) * (3 - len(minimum))
raise SystemExit(not actual or actual < minimum)' "$minimum" "$output"
)
