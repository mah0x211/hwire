#include "hwire.h"
#include "hashmap_hash.h"
#include "hashmap_key.h"
#include <string.h>

#include "khashl.h"

typedef struct {
    const char *ptr;
    size_t len;
    int ci;
} khashl_key_t;
static khint_t key_hash(khashl_key_t key)
{
    return (khint_t)(key.ci ? hashmap_hash_bytes_ci(key.ptr, key.len) :
                             hashmap_hash_bytes(key.ptr, key.len));
}
static int key_equal(khashl_key_t a, khashl_key_t b)
{
    if (a.len != b.len) {
        return 0;
    }
    if (!a.ci) {
        return a.len == 0 || memcmp(a.ptr, b.ptr, a.len) == 0;
    }
    return hashmap_key_compare_ci(a.ptr, b.ptr, a.len) == 0;
}
KHASHL_MAP_INIT(KH_LOCAL, storage_t, storage, khashl_key_t, hwire_kv_pair_t, key_hash, key_equal)
typedef struct {
    storage_t values;
    size_t initial_slots;
    int ci;
} khashl_hashmap_t;
static void *khashl_hashmap_create(size_t capacity, int ci, int growth)
{
    khashl_hashmap_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }
    size_t n = growth ? 32 : capacity;
    size_t buckets = 4;
    while (kh_max_count(buckets) < n) {
        buckets *= 2;
    }
    hashmap_hash_seed(UINT64_C(42));
    storage_resize(&ctx->values, (khint_t)buckets);
    if (ctx->values.keys == NULL) {
        free(ctx);
        return NULL;
    }
    ctx->initial_slots = buckets;
    ctx->ci = ci;
    return ctx;
}
void *khashl_hashmap_new_exact(size_t n) { return khashl_hashmap_create(n, 0, 0); }
void *khashl_hashmap_new_ci(size_t n) { return khashl_hashmap_create(n, 1, 0); }
void *khashl_hashmap_new_growth_exact(size_t n) { return khashl_hashmap_create(n, 0, 1); }
void *khashl_hashmap_new_growth_ci(size_t n) { return khashl_hashmap_create(n, 1, 1); }
void khashl_hashmap_free(void *ctx)
{
    khashl_hashmap_t *map = ctx;
    free(map->values.keys);
    free(map->values.used);
    free(map);
}
size_t khashl_hashmap_growths(const void *ctx)
{
    const khashl_hashmap_t *map = ctx;
    size_t count = 0;
    for (size_t n = map->initial_slots; n < kh_end(&map->values); n *= 2) {
        count++;
    }
    return count;
}
size_t khashl_hashmap_bytes(const void *ctx)
{
    const khashl_hashmap_t *map = ctx;
    size_t slots = kh_end(&map->values);
    return sizeof(*map) + slots * sizeof(map->values.keys[0]) +
           __kh_fsize(slots) * sizeof(khint32_t);
}
int khashl_hashmap_push(void *ctx, const hwire_kv_pair_t *pair)
{
    khashl_hashmap_t *map = ctx;
    int absent;
    khint_t pos = storage_put(&map->values,
                             (khashl_key_t){pair->key.ptr, pair->key.len, map->ci},
                             &absent);
    if (absent < 0) {
        return 0;
    }
    kh_val(&map->values, pos) = *pair;
    return 1;
}
const hwire_kv_pair_t *khashl_hashmap_get(const void *ctx, const char *key, size_t len)
{
    const khashl_hashmap_t *map = ctx;
    khint_t pos = storage_get(&map->values, (khashl_key_t){key, len, 0});
    return pos == kh_end(&map->values) ? NULL : &kh_val(&map->values, pos);
}
const hwire_kv_pair_t *khashl_hashmap_get_ci(const void *ctx, const char *key, size_t len)
{
    const khashl_hashmap_t *map = ctx;
    khint_t pos = storage_get(&map->values, (khashl_key_t){key, len, 1});
    return pos == kh_end(&map->values) ? NULL : &kh_val(&map->values, pos);
}

const char *khashl_hashmap_name(void) { return "khashl"; }

double khashl_hashmap_loadfactor(const void *ctx)
{
    const khashl_hashmap_t *map = ctx;
    return (double)map->values.count / (double)kh_end(&map->values);
}
