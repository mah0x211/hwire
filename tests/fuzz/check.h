#ifndef FUZZ_CHECK_H
#define FUZZ_CHECK_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hwire.h"

#if defined(FUZZ_EXPECT_SCALAR) && !defined(HWIRE_NO_SIMD)
#error Scalar harness requires HWIRE_NO_SIMD
#endif
#if defined(FUZZ_EXPECT_SSE2) && (!defined(__SSE2__) || defined(__SSE4_2__))
#error SSE2 harness must enable SSE2 and disable SSE4.2
#endif
#if defined(FUZZ_EXPECT_SSE42) && !defined(__SSE4_2__)
#error SSE4.2 harness requires SSE4.2
#endif
#if defined(FUZZ_EXPECT_NEON) && !defined(__ARM_NEON)
#error NEON harness requires ARM NEON
#endif

/* Keep checks enabled even when callers supply -DNDEBUG. */
#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        abort(); \
    } \
} while (0)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

typedef struct {
    const char *input;
    size_t len;
    const char *decoded;
    size_t decoded_size;
    size_t calls;
    size_t stop_at;
    int stopped;
} callback_state_t;

static inline void check_slice(hwire_str_t slice, const char *base, size_t len)
{
    /* Empty values can use a static empty string (e.g. chunk extensions). */
    if (slice.len == 0) {
        return;
    }
    CHECK(slice.ptr);
    uintptr_t p = (uintptr_t)slice.ptr;
    uintptr_t b = (uintptr_t)base;
    CHECK(p >= b && p - b <= len);
    CHECK(slice.len <= len - (size_t)(p - b));
}

static inline int callback_done(callback_state_t *state)
{
    CHECK(!state->stopped);
    state->calls++;
    state->stopped = state->stop_at && state->calls == state->stop_at;
    return state->stopped;
}

static inline int check_pair(hwire_ctx_t *ctx, hwire_kv_pair_t *pair)
{
    callback_state_t *s = ctx->uctx;
    check_slice(pair->key, s->input, s->len);
    check_slice(pair->value, s->input, s->len);
    return callback_done(s);
}

static inline char *copy_input(const uint8_t *data, size_t size)
{
    char *copy = malloc(size ? size : 1);
    CHECK(copy);
    if (size) {
        memcpy(copy, data, size);
    }
    return copy;
}
#endif
