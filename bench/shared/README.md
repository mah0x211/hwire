# Shared benchmark helpers

## Adapter dependency setup

An adapter's optional `setup.sh` runs before its `fetch.sh`. The runner passes
`check` for ordinary local runs and `install` when `INSTALL_DEPS=1`.
If setup fails, that adapter's fetch and build are skipped.

Source `scripts/dependencies.sh` to load the helpers below. Sourcing enables
`set -eu`, reads the caller's first argument (default: `check`), and initializes
an empty queue of missing dependencies. It does not install anything.
Package names, version requirements and toolchain installation remain in each
adapter's `setup.sh`.

| Helper | Contract |
| --- | --- |
| `require_command COMMAND` | Return success if `command -v COMMAND` succeeds, otherwise nonzero. |
| `require_package PKG_CONFIG_NAME` | Return success if `pkg-config --exists PKG_CONFIG_NAME` succeeds, otherwise nonzero. This checks library availability, not a package-manager installation record. |
| `require_apt_package PACKAGE` | Queue the apt package on Linux; do nothing on macOS. Unsupported platforms return nonzero. |
| `require_brew_package FORMULA` | Queue the Homebrew formula on macOS; do nothing on Linux. Unsupported platforms return nonzero. |
| `install_required_packages` | Return success if the queue is empty. In `check` mode, report queued packages and return nonzero without installing. In `install` mode, use Linux `apt-get` (root or `sudo`) or macOS Homebrew. Installation failures fail setup. |
| `version_at_least MINIMUM COMMAND [ARGUMENT ...]` | Compare the first `major.minor[.patch]` version in command stdout with `MINIMUM`; a missing patch is zero. Return success for an equal or newer version, and nonzero for an older/unreadable version or command failure. Requires Python 3. |

Availability checks do not queue or install anything. Call the package-manager
helpers only when a required command or library is unavailable.

After installation, the adapter must recheck the commands/modules or toolchain
version it needs. The shared installer only reports package-manager success.

For example, a script can collect command and module requirements together:

```sh
#!/bin/sh
set -eu
# Load the check/install mode and shared dependency-check helpers.
. "$(dirname "$0")/../../shared/scripts/dependencies.sh"

if ! require_command pkg-config; then
    require_apt_package pkg-config
    require_brew_package pkg-config
fi
if ! require_package example; then
    require_apt_package example-dev
    require_brew_package example
fi
install_required_packages
pkg-config --exists example
```

Replace the example module and package names with the adapter's dependencies.
Toolchains with specific version requirements may use `version_at_least` and
implement their own installation step in `setup.sh`.
