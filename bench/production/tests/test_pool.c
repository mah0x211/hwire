/* Exercise the real platform interception path outside benchmark timing. */
#define _POSIX_C_SOURCE 200809L
#include "pool.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    MIN_ALIGNMENT = 16,
    DEFAULT_ALIGNMENT = _Alignof(max_align_t) > MIN_ALIGNMENT
                            ? _Alignof(max_align_t) : MIN_ALIGNMENT
};

static void check_alignment(const void *ptr, size_t alignment)
{
    assert(ptr != NULL);
    assert((uintptr_t)ptr % alignment == 0);
}

int main(void)
{
    /* Odd byte counts expose alignment gaps between consecutive allocations. */
    for (size_t size = 1; size <= 33; size += 2) {
        parser_pool_begin();
        void *padding = malloc(size);
        check_alignment(padding, DEFAULT_ALIGNMENT);

        unsigned char *block = malloc(16);
        check_alignment(block, DEFAULT_ALIGNMENT);
        memset(block, 42, 16);

        unsigned char *zero = calloc(1, 17);
        check_alignment(zero, DEFAULT_ALIGNMENT);
        for (size_t i = 0; i < 17; i++) {
            assert(zero[i] == 0);
        }

        block = realloc(block, 67);
        check_alignment(block, DEFAULT_ALIGNMENT);
        for (size_t i = 0; i < 16; i++) {
            assert(block[i] == 42);
        }

        void *aligned = aligned_alloc(128, 128);
        check_alignment(aligned, 128);
        void *posix = NULL;
        assert(posix_memalign(&posix, 64, 64) == 0);
        check_alignment(posix, 64);
        free(posix);
        void *again = NULL;
        assert(posix_memalign(&again, 64, 64) == 0);
        assert(again == posix);
        free(again);
        free(aligned);
        free(block);
        free(zero);
        free(padding);
        parser_pool_end();

        parser_pool_begin();
        assert(malloc(size) == padding);
        parser_pool_end();

        void *system = malloc(size);
        assert(system != NULL && system != padding);
        free(system);
    }
    return 0;
}
