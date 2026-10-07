/* dyld redirects external allocation calls to the shared arena wrappers.
 * Calls within this dylib still reach the system allocator. */
#include <stddef.h>
#include <stdlib.h>

void *__wrap_malloc(size_t size);
void *__wrap_calloc(size_t count, size_t size);
void *__wrap_realloc(void *ptr, size_t size);
void *__wrap_aligned_alloc(size_t alignment, size_t size);
int __wrap_posix_memalign(void **ptr, size_t alignment, size_t size);
void __wrap_free(void *ptr);

#define POOL_INTERPOSE(name) \
    __attribute__((used)) static const struct { \
        const void *replacement; \
        const void *original; \
    } interpose_##name __attribute__((section("__DATA,__interpose"))) = { \
        (const void *)&__wrap_##name, (const void *)&name \
    }

POOL_INTERPOSE(malloc);
POOL_INTERPOSE(calloc);
POOL_INTERPOSE(realloc);
POOL_INTERPOSE(aligned_alloc);
POOL_INTERPOSE(posix_memalign);
POOL_INTERPOSE(free);
