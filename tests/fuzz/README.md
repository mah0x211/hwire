# Bounded fuzzing

These harnesses exercise the public parser and table APIs with libFuzzer,
AddressSanitizer and UndefinedBehaviorSanitizer. They complement the deterministic
regression tests; a successful timed run does not prove RFC compliance or safety.

## Requirements

Use Clang with the libFuzzer and sanitizer runtimes. On Ubuntu, install `clang`
and `libclang-rt-dev`. On macOS, install Homebrew LLVM and use its compiler:

```sh
brew install llvm
export PATH="$(brew --prefix llvm)/bin:$PATH"
export SDKROOT="$(xcrun --show-sdk-path)"
```

## Commands

From the repository root:

```sh
make fuzz FUZZ_SECONDS=5
make fuzz FUZZ_SECONDS=10 FUZZ_TARGETS=messages FUZZ_VARIANTS=native
```

FUZZ_SECONDS must be an integer from 1 to 86400; zero is rejected because
libFuzzer interprets it as unlimited. The duration applies to each target/variant,
excluding compilation. libFuzzer
checks its time limit between inputs, so a run can exceed it slightly. Each input
has a two-second timeout, inputs are limited to 8 KiB, and RSS is limited to
512 MiB. ASan quarantine defaults to 32 MiB to keep allocator bookkeeping within
that limit; checks remain enabled. `ASAN_OPTIONS` and `UBSAN_OPTIONS` can be
explicitly overridden.

From this directory:

```sh
make build FUZZ_TARGETS=table FUZZ_VARIANTS=siphash
make replay FUZZ_TARGETS=table FUZZ_VARIANTS=siphash
make replay FUZZ_TARGETS=table FUZZ_VARIANTS=siphash FUZZ_INPUT=/path/to/crash
```

`FUZZ_TARGETS` accepts `messages`, `values`, and `table`, separated by spaces.
`FUZZ_VARIANTS=auto` (default) runs the following supported configurations:

| Host | Parser variants | Table variants |
| --- | --- | --- |
| x86-64 | scalar, sse2, sse42, native | native, siphash |
| ARM64 | scalar, neon, native | native, siphash |

`scalar` defines `HWIRE_NO_SIMD`. `sse2` disables SSE4.2, `sse42` enables it,
`neon` uses the default ARM64 target, and `native` uses `-march=native` or
`-mcpu=native`. `siphash` adds `HWIRE_NO_AES` to the native target. Native table
hashing uses AES when enabled by the host compiler target, otherwise SipHash.
Unsupported architectures or variants are rejected.

Generated binaries, writable corpora, logs and failure inputs are under
`out/<target>/<variant>/` and ignored by Git. Each variant receives the same
checked-in seeds and has its own writable corpus. Subsequent runs retain discoveries.
`make clean` removes generated files. Promote minimized failure inputs to regression
tests when fixing a bug; do not commit the entire generated corpus.

## Targets

- **messages**: Request, response and header parsing. Four control bytes select
  the API, byte budget, starting offset, output capacity and callback interruption.
  The remainder is parsed first as a prefix, then in full with fresh state.
  Checks cover returned positions, callback slices and callback interruption.
  Exact-size input allocations let ASan detect reads beyond the available prefix.
- **values**: Parameters, decoded query parameters, chunk sizes and extensions,
  quoted strings, and character scanners. The same four-byte control layout
  selects the API and limits. Callback slices are checked against the input or
  decode buffer, as appropriate; scanners are compared with character predicates.
- **table**: The first eight bytes select comparison modes, capacities, slot
  multipliers and the deterministic hash key. Remaining two-byte operations select
  push, lookup, duplicate traversal with comparison switching, link, unlink,
  reset or invalid-push checks. A linear model verifies results and insertion
  order independently of the implementation hash/index. Capacities range from
  1 to 32, with up to four segments, all comparison modes and 2N/4N/8N slots.
  Keys include case variants, empty and embedded-NUL keys plus input-derived bytes.
  Linked segments, cross-segment duplicates and repeated unlink/relink are covered.
  Each input is limited to 128 operations to bound reference-model work.

Harnesses respect pointer/lifetime requirements. Only documented error contracts
are asserted, and iterators are recreated after link, unlink or reset.

## CI

The test workflow runs separate Ubuntu and macOS jobs, in parallel with the
ordinary tests. Each target/variant runs for 30 seconds. The default search budget
is five minutes on x86-64 and four minutes on ARM64, with a ten-minute job timeout
including setup and compilation. A failure stops that job and uploads its logs
and failure inputs as an artifact. No network services or credentials are needed.
