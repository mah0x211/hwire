# CI Build and Test Configuration

CI checks parser and table correctness, compiler diagnostics, sanitizer findings and coverage. Clang is the quality-check toolchain; GCC supplies a separate normal-build compatibility job. These are development settings, not flags required by applications embedding hwire.

## Compilers

| Compiler     | Baseline                                                     | Role                                                                           |
| ------------ | ------------------------------------------------------------ | ------------------------------------------------------------------------------ |
| Clang / LLVM | 23; verified with Homebrew 23.1.3 and Ubuntu packages 23.1.2 | Tests, ASan/UBSan, fuzzing and LLVM coverage                                   |
| GCC          | 13; verified with Ubuntu 13.3.0                              | One Ubuntu normal build/test job without sanitizers or coverage                |

The tables describe these baselines, not minimum supported downstream compiler versions. CI prints the actual LLVM tool versions. LLVM coverage tools and clang-tidy use the same major release as Clang. Public sources are checked in C99 mode; fuzz harnesses use C11.

## Compiler Options

`✓` means the compiler implements the option for C; an empty compiler cell means it does not. Compatibility spellings accepted without an implemented diagnostic are left empty. OS and target restrictions are stated in the notes. An empty **Used in** cell means the option is not currently selected by CI; it does not mean the option was overlooked. Availability is distinct from whether a particular program triggers a diagnostic.

This inventory covers the C diagnostics relevant to the parser and table, runtime error detection, and hardening candidates. C++/Objective-C-only checks, unrelated language modes and unrelated target options are outside its scope. The tables list selected options and reviewed candidates; narrower checks enabled by a selected group are marked accordingly. Compiler defaults still apply: omitting `-Wall` and `-Wextra` does not disable diagnostics enabled by default. Selected groups such as `-Wconversion` and `-Wformat=2` also contain narrower checks.

The compiler profiles enable selected warnings individually and treat them as errors. GCC's profile is used by the independent Ubuntu compatibility job. Clang diagnostics apply even when a caller overrides `CFLAGS`.

### Diagnostic policy and C conformance

Keep the public sources within the selected C standard and make declarations explicit.

| Option                            | Purpose and caveats                                                                                         | Clang | GCC   | Used in                                |
| --------------------------------- | ----------------------------------------------------------------------------------------------------------- | :---: | :---: | -------------------------------------- |
| `-Werror`                         | Make enabled warnings fatal; does not enable diagnostics itself.                                            | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-pedantic-errors`                | Reject required diagnostics for the selected ISO C standard; compiler-supported intrinsics remain usable.   | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-fno-common`                     | Reject multiple tentative definitions at link time.                                                         | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wstrict-prototypes`             | Require C function parameter prototypes.                                                                    | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wmissing-prototypes`            | Require declarations for externally visible function definitions.                                           | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wmissing-declarations`          | Diagnose missing declarations; Clang and GCC cover different cases.                                         | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wold-style-definition`          | Reject K&R definitions; accepted without effect by Clang.                                                   |       | ✓     | GCC compatibility                      |
| `-Wmissing-parameter-type`        | Detect missing parameter types in old-style declarations (GCC).                                             |       | ✓     | GCC compatibility                      |
| `-Wimplicit-function-declaration` | Detect calls without declarations.                                                                          | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wimplicit-int`                  | Detect omitted type specifiers.                                                                             | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wredundant-decls`               | Detect redundant declarations; accepted without effect by Clang.                                            |       | ✓     | GCC compatibility                      |
| `-Wgnu-statement-expression`      | Detect GNU statement expressions; no project-wide exception is needed.                                      | ✓     |       |                                        |
| `-Wgnu-folding-constant`          | Detect GNU constant-folding extensions.                                                                     | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wvla`                           | Detect variable-length arrays to keep stack use bounded.                                                    | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wdeclaration-after-statement`   | C90 portability only; C99 declarations are intentional, so not selected.                                    | ✓     | ✓     |                                        |
| `-Wc99-c11-compat`                | Detect C11 constructs incompatible with C99 (GCC); explicit C99 builds already enforce the public baseline. |       | ✓     |                                        |
| `-Wsystem-headers`                | Include system-header diagnostics; dependencies are outside the maintained source scope.                    | ✓     | ✓     |                                        |
| `-Wall`                           | Umbrella group; reviewed as an inventory source, not enabled.                                               | ✓     | ✓     |                                        |
| `-Wextra`                         | Umbrella group; reviewed as an inventory source, not enabled.                                               | ✓     | ✓     |                                        |
| `-Weverything`                    | All Clang diagnostics, including unrelated languages and style rules; not enabled.                          | ✓     |       |                                        |

### Conversions and numeric operations

Expose value-changing conversions and suspicious numeric operations without changing arithmetic semantics.

| Option                                  | Purpose and caveats                                                                                            | Clang | GCC   | Used in                                |
| --------------------------------------- | -------------------------------------------------------------------------------------------------------------- | :---: | :---: | -------------------------------------- |
| `-Wconversion`                          | Detect implicit value-changing conversions; includes several narrower groups.                                  | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wsign-conversion`                     | Detect implicit signedness changes.                                                                            | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wconstant-conversion`                 | Detect value-changing constant conversions.                                                                    | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wenum-conversion`                     | Detect conversions between enum and other types.                                                               | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wenum-enum-conversion`                | Detect arithmetic across enum types.                                                                           | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wdouble-promotion`                    | Detect implicit float-to-double promotion.                                                                     | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wfloat-equal`                         | Detect fragile floating-point equality.                                                                        | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wshift-count-negative`                | Detect negative shift counts.                                                                                  | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wshift-count-overflow`                | Detect shifts at least as wide as the operand.                                                                 | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wshift-negative-value`                | Detect shifting negative signed values; interpretation depends on the language standard.                       | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Warith-conversion`                    | Additional arithmetic narrowing diagnostics (GCC); candidate for later evaluation.                             |       | ✓     |                                        |
| `-Wstrict-overflow=5`                   | Optimizer assumptions about signed overflow; high levels can be noisy and are not a runtime overflow detector. |       | ✓     |                                        |
| `-Wcast-align`                          | Detect casts that increase alignment requirements.                                                             | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wcast-align=strict`                   | GCC alignment check independent of the target's tolerance for unaligned access.                                |       | ✓     | GCC compatibility                      |
| `-Wcast-qual`                           | Detect removal of const/volatile through casts.                                                                | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wcast-function-type`                  | Detect incompatible function pointer casts.                                                                    | ✓     | ✓     | GCC compatibility                      |
| `-Wincompatible-function-pointer-types` | Detect incompatible function pointer assignments.                                                              | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wcast-function-type-mismatch`         | Detect function pointer cast type mismatches (Clang); candidate for later evaluation.                          | ✓     |       |                                        |
| `-Wwrite-strings`                       | Give string literals const-qualified types for diagnostics.                                                    | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wignored-qualifiers`                  | Detect ineffective qualifiers.                                                                                 | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |

### Comparisons, branches and control flow

Check branch conditions, switch coverage and intentional fall-through.

| Option                                     | Purpose and caveats                                                                                                   | Clang | GCC   | Used in                                |
| ------------------------------------------ | --------------------------------------------------------------------------------------------------------------------- | :---: | :---: | -------------------------------------- |
| `-Wsign-compare`                           | Detect signed/unsigned comparisons.                                                                                   | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wtautological-compare`                   | Detect comparisons with predetermined outcomes; includes narrower Clang groups.                                       | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wtautological-bitwise-compare`           | Detect bitwise expressions compared against impossible values.                                                        | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wtautological-constant-in-range-compare` | Detect constant comparisons whose result follows from a type's range.                                                 | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wtautological-overlap-compare`           | Detect overlapping or contradictory comparisons.                                                                      | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wtautological-type-limit-compare`        | Detect comparisons bounded by type limits.                                                                            | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wtype-limits`                            | GCC type-range comparison diagnostics; Clang maps the spelling to narrower groups.                                    | ✓     | ✓     | GCC compatibility                      |
| `-Wimplicit-fallthrough`                   | Require explicit annotations for intentional switch fall-through.                                                     | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wswitch`                                 | Detect missing enum cases.                                                                                            | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wswitch-enum`                            | Detect missing enum cases even when a default exists.                                                                 | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wswitch-default`                         | Require a default in every switch; omitted because exhaustive switches need not have defaults.                        | ✓     | ✓     |                                        |
| `-Wduplicated-cond`                        | Detect repeated conditions in an if/else chain (GCC).                                                                 |       | ✓     | GCC compatibility                      |
| `-Wduplicated-branches`                    | Detect identical branches (GCC).                                                                                      |       | ✓     | GCC compatibility                      |
| `-Wlogical-op`                             | Detect suspicious logical operations (GCC).                                                                           |       | ✓     | GCC compatibility                      |
| `-Wlogical-not-parentheses`                | Detect logical-not precedence mistakes; covered by parentheses diagnostics.                                           | ✓     | ✓     |                                        |
| `-Wparentheses`                            | Detect suspicious precedence and assignments used as conditions.                                                      | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wempty-body`                             | Detect empty control-statement bodies.                                                                                | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wmisleading-indentation`                 | Detect indentation inconsistent with control flow.                                                                    | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wfor-loop-analysis`                      | Detect suspicious loop conditions and increments.                                                                     | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Winfinite-recursion`                     | Detect recursion without a terminating path.                                                                          | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunreachable-code`                       | Detect unreachable statements (Clang).                                                                                | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wjump-misses-init`                       | Detect jumps that bypass initialization (GCC).                                                                        | ✓     | ✓     | GCC compatibility                      |
| `-Wself-assign`                            | Detect self-assignment (Clang).                                                                                       | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wbitwise-instead-of-logical`             | Suggest logical operators for boolean values; deliberate branchless bitwise expressions are allowed, so not selected. | ✓     |       |                                        |

### Initialization and unused code

Check initialization, visibility and unused declarations; optimized diagnostics remain compiler-dependent.

| Option                         | Purpose and caveats                                                                                                           | Clang | GCC   | Used in                                |
| ------------------------------ | ----------------------------------------------------------------------------------------------------------------------------- | :---: | :---: | -------------------------------------- |
| `-Wuninitialized`              | Detect provable uninitialized uses; not a substitute for MemorySanitizer.                                                     | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wconditional-uninitialized`  | Detect conditionally uninitialized variables (Clang).                                                                         | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wmaybe-uninitialized`        | Optimization-dependent possible uninitialized uses (GCC).                                                                     |       | ✓     | GCC compatibility                      |
| `-Wmissing-field-initializers` | Detect accidental omitted aggregate fields; intentional zero/designated initialization follows compiler rules.                | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Winitializer-overrides`      | Detect overridden initializers (Clang).                                                                                       | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Woverride-init`              | Detect overridden initializers (GCC).                                                                                         | ✓     | ✓     | GCC compatibility                      |
| `-Wmissing-braces`             | Detect missing aggregate braces.                                                                                              | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunused-function`            | Detect unused internal functions.                                                                                             | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunused-variable`            | Detect unused variables.                                                                                                      | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunused-parameter`           | Detect unused parameters.                                                                                                     | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunused-but-set-variable`    | Detect assigned but unread variables.                                                                                         | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunused-but-set-parameter`   | Detect assigned but unread parameters.                                                                                        | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunused-label`               | Detect unused labels.                                                                                                         | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunused-local-typedefs`      | Detect unused local typedefs.                                                                                                 | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunused-result`              | Detect ignored results of functions marked as requiring a check.                                                              | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunused-value`               | Detect expressions without useful effects.                                                                                    | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wunused-macros`              | Translation-unit-specific macro usage can flag architecture helpers; candidate, not selected.                                 | ✓     | ✓     |                                        |
| `-Wshadow`                     | Detect local declarations that hide other declarations; GCC also diagnoses function-pointer parameter names hiding functions. | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |

### Arrays, pointers and memory operations

Check statically known bounds, pointer contracts and memory-operation sizes.

| Option                       | Purpose and caveats                                                                                                      | Clang | GCC   | Used in                                |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------------------ | :---: | :---: | -------------------------------------- |
| `-Warray-bounds`             | Detect statically provable array bounds violations.                                                                      | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Warray-bounds=2`           | GCC's more aggressive optimized bounds analysis.                                                                         |       | ✓     | GCC compatibility                      |
| `-Warray-parameter`          | Detect inconsistent array parameter declarations.                                                                        | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wchar-subscripts`          | Detect plain char used as an array index.                                                                                | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wpointer-arith`            | Detect nonstandard arithmetic on void/function pointers.                                                                 | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wpointer-sign`             | Detect pointer target signedness mismatches.                                                                             | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wnonnull`                  | Detect null arguments to functions with nonnull contracts.                                                               | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wnull-dereference`         | Detect provable null dereferences; coverage differs by compiler.                                                         | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wnull-pointer-arithmetic`  | Detect invalid arithmetic on null pointers (Clang).                                                                      | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wnull-pointer-subtraction` | Detect invalid null-pointer subtraction (Clang).                                                                         | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wsizeof-array-argument`    | Detect sizeof applied to an array parameter that is actually a pointer.                                                  | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wsizeof-array-decay`       | Detect array decay inside sizeof (Clang).                                                                                | ✓     |       | Tests, Fuzz (Clang)                    |
| `-Wsizeof-pointer-memaccess` | Detect pointer-size mistakes in memory operations.                                                                       | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility |
| `-Wrestrict`                 | Detect violation of restricted-pointer aliasing (GCC).                                                                   |       | ✓     | GCC compatibility                      |
| `-Wstringop-overflow=2`      | Detect overflowing string/memory operations using object sizes (GCC).                                                    |       | ✓     | GCC compatibility                      |
| `-Wstringop-overread`        | Detect string/memory reads beyond known objects (GCC).                                                                   |       | ✓     | GCC compatibility                      |
| `-Wstringop-truncation`      | Detect potentially unintended string truncation (GCC).                                                                   |       | ✓     | GCC compatibility                      |
| `-Wunsafe-buffer-usage`      | Clang's broad buffer-safety migration diagnostics; raw slice APIs require individual evaluation, not blanket enablement. | ✓     |       |                                        |
| `-Wstrict-aliasing=3`        | GCC aliasing diagnostics; limited coverage, candidate for later evaluation.                                              |       | ✓     |                                        |
| `-Wreturn-local-addr`        | Detect returned addresses of expired locals (GCC).                                                                       | ✓     | ✓     |                                        |
| `-Wreturn-stack-address`     | Detect returned addresses of expired stack objects (Clang).                                                              | ✓     |       |                                        |

### Formatting, literals and preprocessing

Check format strings, literal representation and preprocessor mistakes.

| Option                                 | Purpose and caveats                                                                                           | Clang | GCC   | Used in                                         |
| -------------------------------------- | ------------------------------------------------------------------------------------------------------------- | :---: | :---: | ----------------------------------------------- |
| `-Wformat=2`                           | Check format arguments, security and nonliteral formats; group contents differ by compiler.                   | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility          |
| `-Wformat-signedness`                  | Detect format argument signedness mismatches (GCC); accepted without effect by Clang.                         | ✓     | ✓     | GCC compatibility                               |
| `-Wformat-overflow=2`                  | Check optimized formatted-output overflow (GCC).                                                              |       | ✓     | GCC compatibility                               |
| `-Wformat-truncation=2`                | Check optimized formatted-output truncation (GCC).                                                            |       | ✓     | GCC compatibility                               |
| `-Wformat-pedantic`                    | Check format extensions against the selected standard (Clang).                                                | ✓     |       | Tests, Fuzz (Clang)                             |
| `-Wformat-nonliteral`                  | Check nonliteral format strings; included in format=2.                                                        | ✓     | ✓     | Tests, Fuzz (via -Wformat=2); GCC compatibility |
| `-Wformat-security`                    | Check unsafe nonliteral formats; included in format=2.                                                        | ✓     | ✓     | Tests, Fuzz (via -Wformat=2); GCC compatibility |
| `-Wunterminated-string-initialization` | Detect character-array string initializers lacking space for NUL; intentional byte arrays use explicit bytes. | ✓     |       | Tests, Fuzz (Clang)                             |
| `-Wstring-concatenation`               | Detect suspicious adjacent literals (Clang).                                                                  | ✓     |       | Tests, Fuzz (Clang)                             |
| `-Wstring-conversion`                  | Detect suspicious string-to-boolean conversions (Clang).                                                      | ✓     |       | Tests, Fuzz (Clang)                             |
| `-Wmultichar`                          | Detect implementation-defined multicharacter constants.                                                       | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility          |
| `-Wtrigraphs`                          | Detect trigraph interpretation.                                                                               | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility          |
| `-Wcomment`                            | Detect malformed/nested comments.                                                                             | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility          |
| `-Wundef`                              | Detect undefined identifiers in preprocessor conditions.                                                      | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility          |
| `-Wunknown-pragmas`                    | Detect unsupported pragmas.                                                                                   | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility          |
| `-Wextra-semi`                         | Detect redundant semicolons (Clang in C).                                                                     | ✓     |       | Tests, Fuzz (Clang)                             |
| `-Wint-in-bool-context`                | Detect suspicious integer expressions used as conditions.                                                     | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility          |
| `-Wreturn-type`                        | Detect return values inconsistent with the function type.                                                     | ✓     | ✓     | Tests, Fuzz (Clang); GCC compatibility          |

### Runtime error detection

Sanitizers detect exercised errors; they do not prove the absence of bugs. `-fsanitize=undefined` does not include every sanitizer check. Recovery controls affect only enabled checks and do not enable additional instrumentation.

| Option                                   | Purpose and caveats                                                                                                                                                 | Clang | GCC   | Used in                 |
| ---------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :---: | :---: | ----------------------- |
| `-fsanitize=address`                     | Detect out-of-bounds accesses and use-after-free.                                                                                                                   | ✓     | ✓     | ASan/UBSan, Fuzz        |
| `-fsanitize=undefined`                   | Detect supported undefined operations, including invalid shifts, misalignment and signed overflow.                                                                  | ✓     | ✓     | ASan/UBSan, Fuzz        |
| `-fno-sanitize-recover=all`              | Stop on findings from enabled checks that support recovery.                                                                                                         | ✓     | ✓     | ASan/UBSan, Fuzz        |
| `-fsanitize=fuzzer`                      | Link libFuzzer and enable its coverage instrumentation.                                                                                                             | ✓     |       | Fuzz                    |
| `-fsanitize=memory`                      | Detect uninitialized reads; Linux-only for this project's targets, separate from ASan, dependent code must be instrumented. Not selected pending a dedicated setup. | ✓     |       |                         |
| `-fsanitize=thread`                      | Detect data races; tables and contexts require application synchronization. Not selected for the single-threaded unit tests.                                        | ✓     | ✓     |                         |
| `-fsanitize=leak`                        | Standalone leak detection where supported; Linux ASan already supplies leak checking by default.                                                                    | ✓     | ✓     |                         |
| `-fsanitize=integer`                     | Includes unsigned wrap and conversion checks; intentional hash arithmetic requires narrower selection.                                                              | ✓     |       |                         |
| `-fsanitize=unsigned-integer-overflow`   | Detect unsigned wrap, including intentional hash operations. Not selected globally.                                                                                 | ✓     |       |                         |
| `-fsanitize=implicit-integer-conversion` | Detect implicit truncation and sign changes; candidate for targeted evaluation.                                                                                     | ✓     |       |                         |
| `-fsanitize=bounds`                      | Array-index instrumentation; individual coverage differs between compilers and UBSan groups. Candidate for targeted evaluation.                                     | ✓     | ✓     |                         |
| `-fsanitize=object-size`                 | Detect operations outside computable object sizes; optimization-dependent. Candidate for targeted evaluation.                                                       | ✓     | ✓     |                         |
| `-fsanitize=cfi`                         | Control-flow integrity; requires compatible LTO/visibility/link configuration. Not selected.                                                                        | ✓     |       |                         |
| `-fsanitize-trap=undefined`              | Trap instead of producing full UBSan runtime reports; omitted to retain diagnostic details.                                                                         | ✓     | ✓     |                         |
| `-fno-omit-frame-pointer`                | Improve sanitizer stack traces; not itself an error detector.                                                                                                       | ✓     | ✓     | ASan/UBSan, Fuzz        |
| `-g`                                     | Emit debug information for reports.                                                                                                                                 | ✓     | ✓     | Tests, ASan/UBSan, Fuzz |
| `-O1`                                    | Optimize sanitizer and fuzz builds while retaining useful diagnostics.                                                                                              | ✓     | ✓     | ASan/UBSan, Fuzz        |
| `-O2`                                    | Exercise optimized code and optimization-dependent diagnostics.                                                                                                     | ✓     | ✓     | Tests                   |

### Hardening and initialization

These options change generated code or libc checks. They are evaluated separately from benchmarks. Native Clang CI jobs run the hardening and pattern profiles separately from sanitizer builds.

| Option                            | Purpose and caveats                                                                                                                                                                 | Clang | GCC   | Used in                  |
| --------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :---: | :---: | ------------------------ |
| `-fstack-protector-strong`        | Canary protection for selected stack frames; not a general bounds detector. Candidate for application builds.                                                                       | ✓     | ✓     |                          |
| `-fstack-protector-all`           | Canary protection for all functions; selected by the local hardening profile.                                                                                                       | ✓     | ✓     | Native hardening         |
| `-D_FORTIFY_SOURCE=3`             | Optimized glibc object-size checks for supported libc functions; selected by the Linux hardening profile. Darwin's fortification is different.                                      | ✓     | ✓     | Native hardening (Linux) |
| `-ftrivial-auto-var-init=pattern` | Pattern-fill automatic variables to expose dependence on incidental stack contents; local supplemental profile, not a direct uninitialized-read detector. Do not combine with MSan. | ✓     | ✓     | Native pattern tests     |
| `-ftrivial-auto-var-init=zero`    | Mitigate uninitialized-data exposure; may conceal uninitialized-use bugs, so not selected for testing.                                                                              | ✓     | ✓     |                          |
| `-fstack-clash-protection`        | Target-dependent stack probing against stack-clash attacks; not selected.                                                                                                           | ✓     | ✓     |                          |
| `-fcf-protection=full`            | x86 CET control-flow protection where supported; not selected.                                                                                                                      | ✓     | ✓     |                          |
| `-fPIE` / `-pie`                  | Position-independent executable generation/linking; final-application policy, not a library-source check.                                                                           | ✓     | ✓     |                          |
| `-Wl,-z,relro,-z,now`             | ELF linker hardening; not valid as the same configuration on macOS. Final-application policy.                                                                                       | ✓     | ✓     |                          |
| `-fhardened`                      | GCC GNU/Linux hardening bundle; contents may change across releases. GCC 13 does not provide it; explicit settings are used.                                                        |       |       |                          |
| `-ftrapv`                         | Trap signed arithmetic overflow; UBSan supplies more useful diagnostics here.                                                                                                       | ✓     | ✓     |                          |
| `-fwrapv`                         | Define signed overflow as wrapping; changes optimization semantics and is not a detector, so not selected.                                                                          | ✓     | ✓     |                          |
| `-fno-strict-aliasing`            | Disable aliasing-based optimizations; can hide aliasing defects, so not selected.                                                                                                   | ✓     | ✓     |                          |

### Coverage

| Option                     | Purpose and caveats                                                                | Clang | GCC   | Used in     |
| -------------------------- | ---------------------------------------------------------------------------------- | :---: | :---: | ----------- |
| `-fprofile-instr-generate` | Generate LLVM execution profiles; must be present at compilation and linking.      | ✓     |       | Tests, Fuzz |
| `-fcoverage-mapping`       | Generate LLVM source coverage mapping.                                             | ✓     |       | Tests, Fuzz |
| `--coverage`               | gcov-style instrumentation; not selected because reports use LLVM source coverage. | ✓     | ✓     |             |

## clang-tidy

The [`.clang-tidy`](.clang-tidy) file disables default checks and lists selected C checks explicitly. `WarningsAsErrors` makes their findings fatal. The target analyzes library translation units and their project headers using the same C standard and warning profile as tests. Compiler diagnostics and clang-tidy checks are distinct: enabling a tidy check does not enable the corresponding compiler warning.

`make tidy` checks the native architecture path by default. Set `TIDY_FLAGS` to analyze scalar or another supported architecture path. `make analyze` is an alias, avoiding duplicate Static Analyzer runs through scan-build. CI runs tidy for each parser variant in the test matrix.

| Category             | Checks                                                | Purpose and caveats                                                                                | Used in      |
| -------------------- | ----------------------------------------------------- | -------------------------------------------------------------------------------------------------- | ------------ |
| Bug-prone constructs | `bugprone-assert-side-effect`                         | Detect side effects that disappear when assertions are disabled.                                   | Clang matrix |
| Bug-prone constructs | `bugprone-assignment-in-if-condition`                 | Detect assignments inside if conditions.                                                           | Clang matrix |
| Bug-prone constructs | `bugprone-bool-pointer-implicit-conversion`           | Detect pointer-to-boolean conversions that may omit a dereference.                                 | Clang matrix |
| Bug-prone constructs | `bugprone-chained-comparison`                         | Detect mathematical-style comparisons with unintended C semantics.                                 | Clang matrix |
| Bug-prone constructs | `bugprone-implicit-widening-of-multiplication-result` | Detect multiplication that overflows before conversion to a wider type.                            | Clang matrix |
| Bug-prone constructs | `bugprone-infinite-loop`                              | Detect loops whose conditions cannot change.                                                       | Clang matrix |
| Bug-prone constructs | `bugprone-integer-division`                           | Detect integer division unexpectedly used in floating-point expressions.                           | Clang matrix |
| Bug-prone constructs | `bugprone-macro-parentheses`                          | Detect macro expressions and arguments missing protective parentheses.                             | Clang matrix |
| Bug-prone constructs | `bugprone-macro-repeated-side-effects`                | Detect macro arguments evaluated repeatedly with side effects.                                     | Clang matrix |
| Bug-prone constructs | `bugprone-misplaced-operator-in-strlen-in-alloc`      | Detect allocation-size arithmetic accidentally placed inside strlen.                               | Clang matrix |
| Bug-prone constructs | `bugprone-misplaced-pointer-arithmetic-in-alloc`      | Detect pointer arithmetic accidentally applied to allocation results.                              | Clang matrix |
| Bug-prone constructs | `bugprone-misplaced-widening-cast`                    | Detect casts that widen only after potentially overflowing arithmetic.                             | Clang matrix |
| Bug-prone constructs | `bugprone-multiple-statement-macro`                   | Detect unguarded macros containing multiple statements.                                            | Clang matrix |
| Bug-prone constructs | `bugprone-not-null-terminated-result`                 | Detect string operations producing unterminated results.                                           | Clang matrix |
| Bug-prone constructs | `bugprone-posix-return`                               | Detect incorrect comparisons against POSIX function return values.                                 | Clang matrix |
| Bug-prone constructs | `bugprone-redundant-branch-condition`                 | Detect branch conditions already established by an enclosing branch.                               | Clang matrix |
| Bug-prone constructs | `bugprone-sizeof-expression`                          | Detect suspicious sizeof operands and arithmetic.                                                  | Clang matrix |
| Bug-prone constructs | `bugprone-standalone-empty`                           | Detect stray empty statements.                                                                     | Clang matrix |
| Bug-prone constructs | `bugprone-suspicious-enum-usage`                      | Detect suspicious arithmetic and bitwise operations on enums.                                      | Clang matrix |
| Bug-prone constructs | `bugprone-suspicious-memory-comparison`               | Detect memory comparisons unsuitable for the compared types.                                       | Clang matrix |
| Bug-prone constructs | `bugprone-suspicious-memset-usage`                    | Detect swapped memset arguments and suspicious fill values.                                        | Clang matrix |
| Bug-prone constructs | `bugprone-suspicious-missing-comma`                   | Detect likely missing commas between adjacent string literals.                                     | Clang matrix |
| Bug-prone constructs | `bugprone-suspicious-realloc-usage`                   | Detect overwriting the only pointer with a potentially failed realloc result.                      | Clang matrix |
| Bug-prone constructs | `bugprone-suspicious-semicolon`                       | Detect semicolons that accidentally terminate control statements.                                  | Clang matrix |
| Bug-prone constructs | `bugprone-suspicious-string-compare`                  | Detect incorrect use of string-comparison return values.                                           | Clang matrix |
| Bug-prone constructs | `bugprone-swapped-arguments`                          | Detect likely argument-order mistakes.                                                             | Clang matrix |
| Bug-prone constructs | `bugprone-too-small-loop-variable`                    | Detect loop counters too narrow for the loop bound.                                                | Clang matrix |
| Bug-prone constructs | `bugprone-undefined-memory-manipulation`              | Detect undefined use of raw memory operations on unsuitable objects.                               | Clang matrix |
| C security rules     | `cert-arr39-c`                                        | Detect incorrect scaling of pointer arithmetic.                                                    | Clang matrix |
| C security rules     | `cert-dcl03-c`                                        | Prefer compile-time assertions for constant conditions; C99 compatibility limits applicable fixes. | Clang matrix |
| C security rules     | `cert-err34-c`                                        | Detect error-prone string-to-number conversions.                                                   | Clang matrix |
| C security rules     | `cert-exp42-c`                                        | Detect comparisons that depend on structure padding.                                               | Clang matrix |
| C security rules     | `cert-flp30-c`                                        | Detect floating-point loop counters.                                                               | Clang matrix |
| C security rules     | `cert-str34-c`                                        | Detect unsafe signed-character to wider-integer conversions.                                       | Clang matrix |
| Braces               | `readability-braces-around-statements`                | Require braces around control-statement bodies.                                                    | Clang matrix |
| Path analysis        | `clang-analyzer-core.BitwiseShift`                    | Track invalid shift counts along execution paths.                                                  | Clang matrix |
| Path analysis        | `clang-analyzer-core.CallAndMessage`                  | Track invalid call targets and uninitialized call arguments.                                       | Clang matrix |
| Path analysis        | `clang-analyzer-core.DivideZero`                      | Track division or remainder by zero.                                                               | Clang matrix |
| Path analysis        | `clang-analyzer-core.NonNullParamChecker`             | Track null arguments passed to nonnull parameters.                                                 | Clang matrix |
| Path analysis        | `clang-analyzer-core.NullDereference`                 | Track dereferences of null pointers.                                                               | Clang matrix |
| Path analysis        | `clang-analyzer-core.NullPointerArithm`               | Track arithmetic on null pointers.                                                                 | Clang matrix |
| Path analysis        | `clang-analyzer-core.StackAddressEscape`              | Track stack addresses escaping their lifetime.                                                     | Clang matrix |
| Path analysis        | `clang-analyzer-core.UndefinedBinaryOperatorResult`   | Track binary operations with undefined operands.                                                   | Clang matrix |
| Path analysis        | `clang-analyzer-core.VLASize`                         | Track invalid variable-length array sizes.                                                         | Clang matrix |
| Path analysis        | `clang-analyzer-core.uninitialized.ArraySubscript`    | Track uninitialized array indices.                                                                 | Clang matrix |
| Path analysis        | `clang-analyzer-core.uninitialized.Assign`            | Track assignment of uninitialized values.                                                          | Clang matrix |
| Path analysis        | `clang-analyzer-core.uninitialized.Branch`            | Track branch conditions using uninitialized values.                                                | Clang matrix |
| Path analysis        | `clang-analyzer-core.uninitialized.UndefReturn`       | Track returning uninitialized values.                                                              | Clang matrix |
| Path analysis        | `clang-analyzer-unix.API`                             | Check constraints of supported Unix APIs.                                                          | Clang matrix |
| Path analysis        | `clang-analyzer-unix.Malloc`                          | Track allocation lifetimes, leaks, invalid frees and use-after-free.                               | Clang matrix |
| Path analysis        | `clang-analyzer-unix.MallocSizeof`                    | Check allocated sizes against the destination pointer type.                                        | Clang matrix |
| Path analysis        | `clang-analyzer-unix.StdCLibraryFunctions`            | Track supported C-library argument and return constraints.                                         | Clang matrix |
| Path analysis        | `clang-analyzer-unix.cstring.BadSizeArg`              | Detect incorrect sizes passed to string functions.                                                 | Clang matrix |
| Path analysis        | `clang-analyzer-unix.cstring.NotNullTerminated`       | Track unterminated strings passed to C-string functions.                                           | Clang matrix |
| Path analysis        | `clang-analyzer-unix.cstring.NullArg`                 | Track null arguments passed to string and memory functions.                                        | Clang matrix |
| Path analysis        | `clang-analyzer-unix.cstring.UninitializedRead`       | Track string and memory operations reading uninitialized storage.                                  | Clang matrix |

Additional C candidates remain visible below; they are not enabled automatically when the toolchain is upgraded.

| Check                                       | Purpose and reason not selected                                                                                                      | Used in |
| ------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ | ------- |
| `bugprone-branch-clone`                     | Identical branches can be intentional in parser state handling; evaluate findings before adoption.                                   |         |
| `bugprone-easily-swappable-parameters`      | Same-typed pointer/length parameters are common in this public C API; evaluate signal first.                                         |         |
| `bugprone-reserved-identifier`              | Identifier policy check; candidate for later evaluation.                                                                             |         |
| `bugprone-signed-bitwise`                   | Low-level SIMD/hash arithmetic requires case-by-case review.                                                                         |         |
| `bugprone-unsafe-functions`                 | Broad libc migration policy does not directly fit bounded slice processing.                                                          |         |
| `cert-err33-c`                              | Checking libc return values is useful but includes allocation/error contracts outside the library's allocation-free code. Candidate. |         |
| `readability-identifier-naming`             | Naming policy enforcement; not selected as a correctness check.                                                                      |         |
| `readability-function-cognitive-complexity` | Parser state machines naturally have complex control flow; not selected as a correctness gate.                                       |         |
| `modernize-*`, `cppcoreguidelines-*`        | C++ rules are outside this C project's scope.                                                                                        |         |

## Profiles and Local Commands

| File                                                         | Contents                                                             | Invocation                                       |
| ------------------------------------------------------------ | -------------------------------------------------------------------- | ------------------------------------------------ |
| [`config/clang/warnings.cfg`](config/clang/warnings.cfg)     | Explicit Clang diagnostics, ISO conformance and common-symbol policy | `clang --config=./config/clang/warnings.cfg ...` |
| [`config/gcc/warnings.rsp`](config/gcc/warnings.rsp)         | GCC C diagnostics and conformance                                    | `gcc @config/gcc/warnings.rsp ...`               |
| [`config/clang/sanitizers.cfg`](config/clang/sanitizers.cfg) | ASan/UBSan, recovery and report settings                             | `make sanitizers`                                |
| [`config/clang/hardening.cfg`](config/clang/hardening.cfg)   | Optimized stack protection                                           | `make hardening`                                 |
| [`config/clang/fortify.cfg`](config/clang/fortify.cfg)       | glibc fortification, added by the Linux hardening target             | `make hardening`                                 |
| [`config/clang/pattern.cfg`](config/clang/pattern.cfg)       | Supplemental automatic-variable pattern initialization               | `make pattern`                                   |
| [`.clang-tidy`](.clang-tidy)                                 | Explicit C analysis checks                                           | `make tidy`                                      |

```sh
make test table-test
make test table-test CC=gcc OBJ_DIR=obj/gcc
make sanitizers
make hardening
make pattern
make tidy
make tidy TIDY_FLAGS=-DHWIRE_NO_SIMD
make coverage
```

Clang loads configuration-file options before command-line options. Dedicated sanitizer/hardening targets remove caller optimization flags so their profiles select `-O1`/`-O2` consistently.

The compiler must be on `PATH`; select the matching LLVM tool directory before running coverage or tidy. The Linux fortification target assumes glibc (the CI distribution uses glibc). Sanitizer, hardening, pattern and tidy targets require Clang. Test binaries depend on the compiler's configuration files and Makefile; use distinct `OBJ_DIR` values when changing command-line compiler/CPU options, since Make does not track command-line flag changes.

## CI Jobs

| Workflow                                 | Current checks                                                                                     | Configuration                                               |
| ---------------------------------------- | -------------------------------------------------------------------------------------------------- | ----------------------------------------------------------- |
| [`test.yml`](.github/workflows/test.yml) | Clang OS/variant tests, coverage, ASan/UBSan and tidy; native hardening/pattern; GCC compatibility | Shared LLVM 23 setup; GCC 13; compiler-specific profiles    |
| [`fuzz.yml`](.github/workflows/fuzz.yml) | Bounded fuzzing and separate Actions coverage reports on master push or manual dispatch            | Same LLVM setup and warning profile; libFuzzer + ASan/UBSan |

The Clang matrix combines Ubuntu scalar/SSE2/SSE4.2/native and macOS scalar/NEON/native. Each member builds and executes tests on the same runner, runs coverage and ASan/UBSan separately, and checks its parser path with clang-tidy. Native jobs also run table tests with AES and disabled-AES fallback builds. Other matrix members run parser tests only: `HWIRE_NO_SIMD` also disables AES, so scalar flags cannot be combined with the AES-required table test. Native matrix members additionally run hardening and pattern initialization. Matrix fail-fast cancels queued or running members when one fails; independent jobs such as GCC are not part of that cancellation group. Fuzz coverage is not mixed into Codecov unit-test coverage.

## Updating the Toolchain

1. Update the compiler baseline and shared setup together. Confirm available stable packages on both CI platforms; do not substitute a development snapshot.
2. Compare the official diagnostic reference, group membership and release notes with this inventory. Add new relevant options and record the reason for unused candidates.
3. Check actual C-mode support and diagnostic behavior for each compiler. A recognized compatibility spelling is not proof that it implements a check.
4. Update compiler profiles and `.clang-tidy`, run affected tests and architecture paths, and update **Used in** only when the workflow actually selects the option.

Reference inventories: [Clang diagnostics](https://clang.llvm.org/docs/DiagnosticsReference.html), [Clang driver/configuration options](https://clang.llvm.org/docs/UsersManual.html), [GCC 13 warning options](https://gcc.gnu.org/onlinedocs/gcc-13.3.0/gcc/Warning-Options.html), [GCC instrumentation options](https://gcc.gnu.org/onlinedocs/gcc-13.3.0/gcc/Instrumentation-Options.html), [clang-tidy checks](https://clang.llvm.org/extra/clang-tidy/checks/list.html), [glibc fortification](https://sourceware.org/glibc/manual/latest/html_node/Source-Fortification.html).


## Releases

The manual [release workflow](.github/workflows/release.yml) accepts a CalVer
`YYYY.MM.SEQUENCE` version without the `v` prefix. Run it on `master`:

```sh
gh workflow run release.yml --ref master -f version=2026.10.0
```

The workflow fixes the source commit at dispatch time, exports its tracked
files, and stamps `src/hwire.h` in that snapshot. It runs parser and table
tests with LLVM 23 and packages only `src/`, `LICENSE`, and `README.md` in
`hwire-<version>.tar.gz`. It then creates a draft release with the archive,
download instructions, and automatically generated PR changelog. The draft
specifies `v<version>` and the full source commit SHA; no tag is created yet.
Release runs are serialized and existing tags are rejected. If an interrupted
run leaves a draft, inspect and complete it or delete it before retrying.

Review the draft, add API compatibility and migration notes where needed,
and publish only after reviewing the final release body:

```sh
gh release edit v2026.10.0 --notes-file release-notes.md
gh release edit v2026.10.0 --draft=false --latest
```

GitHub creates the tag at the fixed source commit when the draft is published.
Repository headers remain development versions, including the sources
referenced by the tag. The attached archive contains the stamped version;
GitHub's automatic **Source code** archives do not.

### Published Release Policy

Review and edit releases while they are drafts. After publication, treat the
release as immutable: do not move it back to a draft or replace its assets.
Publish corrections under a new version with an incremented CalVer sequence.
To withdraw a release, delete it while it is still published:

```sh
gh release delete v2026.10.0 --yes
```

Use a new version and tag for any subsequent release, including a replacement
for a deleted release. This policy applies whether or not GitHub's immutable
releases setting is enabled.

### Tag Cleanup

The [release cleanup workflow](.github/workflows/release-cleanup.yml) subscribes
to `release.deleted` and `release.unpublished`. Deleting a published release
triggers automatic deletion of its CalVer tag; this was verified for both
regular releases and prereleases.

In verification with `gh release edit <tag> --draft=true`, both regular releases
and prereleases returned to draft, but no `unpublished` workflow run was
observed and their tags remained. The cause is undetermined; support for
`unpublished` is retained, but tag cleanup must not depend on that event.
Deleting a draft does not trigger the cleanup workflow. If a published release
has already been returned to draft, check its tag and manually delete it if
withdrawal is intended:

```sh
gh api --method DELETE repos/OWNER/REPO/git/refs/tags/v2026.10.0
```

A newly created, never-published draft has no tag to clean up.

Cleanup only targets `vYYYY.MM.SEQUENCE` tags, tolerates an already absent tag,
and preserves a tag if its release is currently published when cleanup runs.
API failures are reported rather than ignored. Cleanup and release creation
share the same concurrency group with `queue: max`, so pending runs are queued
instead of replacing earlier pending runs (up to GitHub's 100-run queue limit).

Publish and delete through the GitHub UI or an authenticated local `gh` command.
Operations performed with a workflow `GITHUB_TOKEN` do not trigger another
workflow, so automated deletion must perform its own tag cleanup.

With immutable releases enabled, GitHub also prevents changes to published
assets and tags and conversion back to a draft. After deleting an immutable
release, its tag can be removed, but its name cannot be reused. Tag protection
rules must permit the cleanup workflow to delete release tags.
