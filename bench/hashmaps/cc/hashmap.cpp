#include "hashmap_hash.h"
#include "hashmap_key.h"
#include "hwire.h"
#include <cstring>

struct cc_key_t {
    const char *ptr;
    size_t len;
    bool ci;
};
static int cc_key_compare(cc_key_t a, cc_key_t b)
{
    if (a.len != b.len) {
        return a.len < b.len ? -1 : 1;
    }
    if (!a.ci) {
        return a.len ? std::memcmp(a.ptr, b.ptr, a.len) : 0;
    }
    return hashmap_key_compare_ci(a.ptr, b.ptr, a.len);
}
static size_t cc_key_hash(cc_key_t key)
{
    return key.ci ? hashmap_hash_bytes_ci(key.ptr, key.len) :
                    hashmap_hash_bytes(key.ptr, key.len);
}
#include "cc.h"
#define CC_CMPR cc_key_t, { return cc_key_compare(val_1, val_2); }
#include "cc.h"
#define CC_HASH cc_key_t, { return cc_key_hash(val); }
#include "cc.h"

struct cc_hashmap_t {
    map(cc_key_t, hwire_kv_pair_t) values;
    size_t initial_slots;
    bool ci;
};
static void *cc_hashmap_create(size_t capacity, bool ci, bool growth)
{
    auto *ctx = static_cast<cc_hashmap_t *>(malloc(sizeof(cc_hashmap_t)));
    if (ctx == nullptr) {
        return nullptr;
    }
    init(&ctx->values);
    ctx->ci = ci;
    hashmap_hash_seed(UINT64_C(42));
    if (!reserve(&ctx->values, growth ? 32 : capacity)) {
        cleanup(&ctx->values);
        free(ctx);
        return nullptr;
    }
    ctx->initial_slots = cc_map_cap(ctx->values);
    return ctx;
}
extern "C" void *cc_hashmap_new_exact(size_t n) { return cc_hashmap_create(n, false, false); }
extern "C" void *cc_hashmap_new_ci(size_t n) { return cc_hashmap_create(n, true, false); }
extern "C" void *cc_hashmap_new_growth_exact(size_t n) { return cc_hashmap_create(n, false, true); }
extern "C" void *cc_hashmap_new_growth_ci(size_t n) { return cc_hashmap_create(n, true, true); }
extern "C" void cc_hashmap_free(void *ctx)
{
    auto *map = static_cast<cc_hashmap_t *>(ctx);
    cleanup(&map->values);
    free(map);
}
extern "C" size_t cc_hashmap_growths(const void *ctx)
{
    auto *map = static_cast<const cc_hashmap_t *>(ctx);
    size_t count = 0;
    for (size_t n = map->initial_slots; n < cc_map_cap(map->values); n *= 2) {
        count++;
    }
    return count;
}
extern "C" size_t cc_hashmap_bytes(const void *ctx)
{
    auto *map = static_cast<const cc_hashmap_t *>(ctx);
    const cc_map_hdr_ty *h = cc_map_hdr(map->values);
    return sizeof(*map) +
           (reinterpret_cast<const char *>(h->metadata) -
            reinterpret_cast<const char *>(h)) +
           (cc_map_cap(map->values) + 4) * sizeof(h->metadata[0]);
}
extern "C" int cc_hashmap_push(void *ctx, const hwire_kv_pair_t *pair)
{
    auto *map = static_cast<cc_hashmap_t *>(ctx);
    return get_or_insert(&map->values,
                         (cc_key_t{pair->key.ptr, pair->key.len, map->ci}),
                         *pair) != nullptr;
}
extern "C" const hwire_kv_pair_t *cc_hashmap_get(const void *ctx, const char *key, size_t len)
{
    auto *map = const_cast<cc_hashmap_t *>(static_cast<const cc_hashmap_t *>(ctx));
    return get(&map->values, (cc_key_t{key, len, false}));
}
extern "C" const hwire_kv_pair_t *cc_hashmap_get_ci(const void *ctx, const char *key, size_t len)
{
    auto *map = const_cast<cc_hashmap_t *>(static_cast<const cc_hashmap_t *>(ctx));
    return get(&map->values, (cc_key_t{key, len, true}));
}

extern "C" const char *cc_hashmap_name(void) { return "CC"; }

extern "C" double cc_hashmap_loadfactor(const void *ctx)
{
    const auto *map = static_cast<const cc_hashmap_t *>(ctx);
    return static_cast<double>(cc_map_hdr(map->values)->size) /
           static_cast<double>(cc_map_cap(map->values));
}
