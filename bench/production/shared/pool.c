/* A reusable arena models available pool memory without changing adapters. */
#define _POSIX_C_SOURCE 200809L
#include "pool.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Keep the allocation header suitably aligned on each platform. */
typedef union {
    long double scalar;
    void *pointer;
} pool_alignment_t;

typedef union {
    struct {
        size_t size;
        size_t previous_used;
    } block;
    pool_alignment_t alignment;
} pool_header_t;

enum {
    POOL_BYTES = 1024 * 1024,
    /* Apple malloc and Rust's System allocator require at least 16-byte alignment. */
    POOL_MIN_ALIGNMENT = 16,
    POOL_ALIGNMENT = _Alignof(max_align_t) > POOL_MIN_ALIGNMENT
                         ? _Alignof(max_align_t) : POOL_MIN_ALIGNMENT
};
static unsigned char *arena;
static size_t used;
static int active;

/* Calls from the interposing dylib itself resolve to the system allocator. */
#if defined(__APPLE__)
#define __real_malloc malloc
#define __real_free free
#define __real_calloc calloc
#define __real_realloc realloc
#define __real_aligned_alloc aligned_alloc
#define __real_posix_memalign posix_memalign
#else
void *__real_malloc(size_t size);
void __real_free(void *ptr);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *ptr, size_t size);
void *__real_aligned_alloc(size_t alignment, size_t size);
int __real_posix_memalign(void **ptr, size_t alignment, size_t size);
#endif

void parser_pool_begin(void)
{
    if (arena == NULL) {
        arena = __real_malloc(POOL_BYTES);
    }
    used = 0;
    active = 1;
}

void parser_pool_end(void)
{
    active = 0;
}

static int pool_contains(const void *ptr)
{
    uintptr_t address = (uintptr_t)ptr;
    uintptr_t base = (uintptr_t)arena;
    return arena != NULL && address >= base && address < base + POOL_BYTES;
}

static void *pool_alloc(size_t size, size_t alignment)
{
    if (alignment < POOL_ALIGNMENT) {
        alignment = POOL_ALIGNMENT;
    }
    if (arena == NULL || used > POOL_BYTES - sizeof(pool_header_t)) {
        return NULL;
    }
    size_t start = used + sizeof(pool_header_t);
    size_t padding = (alignment - ((uintptr_t)(arena + start) % alignment)) % alignment;
    size_t bytes = size != 0 ? size : 1;
    if (padding > POOL_BYTES - start || bytes > POOL_BYTES - start - padding) {
        return NULL;
    }
    void *ptr = arena + start + padding;
    ((pool_header_t *)ptr)[-1].block.size = size;
    ((pool_header_t *)ptr)[-1].block.previous_used = used;
    used = start + padding + bytes;
    return ptr;
}

void *__wrap_malloc(size_t size)
{
    return active ? pool_alloc(size, POOL_ALIGNMENT) : __real_malloc(size);
}

int __wrap_posix_memalign(void **ptr, size_t alignment, size_t size)
{
    if (!active) {
        return __real_posix_memalign(ptr, alignment, size);
    }
    void *block = pool_alloc(size, alignment);
    if (block == NULL) {
        return ENOMEM;
    }
    *ptr = block;
    return 0;
}

void *__wrap_calloc(size_t count, size_t size)
{
    if (!active) {
        return __real_calloc(count, size);
    }
    if (size != 0 && count > SIZE_MAX / size) {
        return NULL;
    }
    size_t bytes = count * size;
    void *ptr = pool_alloc(bytes, POOL_ALIGNMENT);
    if (ptr != NULL) {
        memset(ptr, 0, bytes);
    }
    return ptr;
}

void *__wrap_realloc(void *ptr, size_t size)
{
    if (!active) {
        return __real_realloc(ptr, size);
    }
    if (ptr == NULL) {
        return pool_alloc(size, POOL_ALIGNMENT);
    }
    if (!pool_contains(ptr)) {
        /* Native containers must acquire all their storage within this region. */
        abort();
    }
    void *next = pool_alloc(size, POOL_ALIGNMENT);
    if (next != NULL) {
        size_t old_size = ((pool_header_t *)ptr)[-1].block.size;
        memcpy(next, ptr, old_size < size ? old_size : size);
    }
    return next;
}

void *__wrap_aligned_alloc(size_t alignment, size_t size)
{
    return active ? pool_alloc(size, alignment) : __real_aligned_alloc(alignment, size);
}

void __wrap_free(void *ptr)
{
    if (!pool_contains(ptr)) {
        __real_free(ptr);
    } else {
        pool_header_t *header = (pool_header_t *)ptr - 1;
        size_t bytes = header->block.size != 0 ? header->block.size : 1;
        if ((unsigned char *)ptr + bytes == arena + used) {
            /* Reuse temporary lookup-name storage without resetting live headers.
             * Non-LIFO allocations remain owned by the request's arena reset. */
            used = header->block.previous_used;
        }
    }
}
