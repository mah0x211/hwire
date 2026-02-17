#!/bin/bash
# Setup httparse_bench Rust wrapper for FFI benchmarking

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BENCH_DIR="$(dirname "$SCRIPT_DIR")"
HTTPARSE_DIR="$BENCH_DIR/deps/httparse_bench"

# Create directory structure
mkdir -p "$HTTPARSE_DIR/src"

# Generate Cargo.toml
cat > "$HTTPARSE_DIR/Cargo.toml" << 'EOF'
[package]
name = "httparse_bench"
version = "0.1.0"
edition = "2021"

[lib]
crate-type = ["staticlib"]

[dependencies]
httparse = "1.10"

[profile.release]
opt-level = 3
lto = true
EOF

# Generate lib.rs
cat > "$HTTPARSE_DIR/src/lib.rs" << 'EOF'
use httparse::{Request, EMPTY_HEADER};
use std::slice;

const MAX_HEADERS: usize = 128;

/// Parse HTTP request
/// Returns: 0 on success, -1 on error
#[no_mangle]
pub extern "C" fn httparse_bench_parse(data: *const u8, len: usize) -> i32 {
    unsafe {
        let bytes = slice::from_raw_parts(data, len);
        let mut headers = [EMPTY_HEADER; MAX_HEADERS];
        let mut req = Request::new(&mut headers);

        match req.parse(bytes) {
            Ok(httparse::Status::Complete(_)) => 0,
            Ok(httparse::Status::Partial) => -1,
            Err(_) => -1,
        }
    }
}
EOF

echo "httparse_bench setup complete."
