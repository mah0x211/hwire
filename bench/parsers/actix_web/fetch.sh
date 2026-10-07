#!/bin/sh
# Fetch the dependencies pinned by Cargo.lock before building or timing.
set -eu
cd "$(dirname "$0")"
${CARGO:-cargo} fetch --manifest-path Cargo.toml --locked
