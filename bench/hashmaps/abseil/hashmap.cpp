#include "absl/container/flat_hash_map.h"
#include "absl/container/internal/hashtable_debug.h"
#include "hashmap_hash.h"
#include "hashmap_key.h"
#include "hwire.h"

#include <cstring>
#include <new>

struct abseil_key_t {
    const char *ptr;
    size_t len;
};

struct abseil_hash_t {
    bool ci;

    size_t operator()(const abseil_key_t &key) const
    {
        return static_cast<size_t>(ci ?
                                       hashmap_hash_bytes_ci(key.ptr, key.len) :
                                       hashmap_hash_bytes(key.ptr, key.len));
    }
};

struct abseil_equal_t {
    bool ci;

    bool operator()(const abseil_key_t &lhs, const abseil_key_t &rhs) const
    {
        if (lhs.len != rhs.len) {
            return false;
        }
        if (!ci) {
            return lhs.len == 0 || std::memcmp(lhs.ptr, rhs.ptr, lhs.len) == 0;
        }
        return hashmap_key_compare_ci(lhs.ptr, rhs.ptr, lhs.len) == 0;
    }
};

using abseil_values_t =
    absl::flat_hash_map<abseil_key_t, hwire_kv_pair_t, abseil_hash_t,
                        abseil_equal_t>;

struct abseil_hashmap_t {
    abseil_values_t values;
    size_t initial_slots;

    abseil_hashmap_t(size_t initial, bool insensitive)
        : values(0, abseil_hash_t{insensitive}, abseil_equal_t{insensitive}),
          initial_slots(0)
    {
        values.reserve(initial);
        initial_slots = values.capacity();
    }
};

static void *abseil_hashmap_create(size_t capacity, bool ci, bool growth = false)
{
    hashmap_hash_seed(UINT64_C(42));
    size_t initial = growth && capacity > 32u ? 32u : capacity;
    try {
        return new abseil_hashmap_t(initial, ci);
    } catch (const std::bad_alloc &) {
        return nullptr;
    }
}

extern "C" void *abseil_hashmap_new_exact(size_t capacity)
{
    return abseil_hashmap_create(capacity, false);
}

extern "C" void *abseil_hashmap_new_ci(size_t capacity)
{
    return abseil_hashmap_create(capacity, true);
}

extern "C" void *abseil_hashmap_new_growth_exact(size_t capacity)
{
    return abseil_hashmap_create(capacity, false, true);
}

extern "C" void *abseil_hashmap_new_growth_ci(size_t capacity)
{
    return abseil_hashmap_create(capacity, true, true);
}

extern "C" void abseil_hashmap_free(void *ctx)
{
    delete static_cast<abseil_hashmap_t *>(ctx);
}

extern "C" size_t abseil_hashmap_bytes(const void *ctx)
{
    const abseil_hashmap_t *map = static_cast<const abseil_hashmap_t *>(ctx);
    return sizeof(*map) +
           absl::container_internal::AllocatedByteSize(map->values);
}

extern "C" int abseil_hashmap_push(void *ctx, const hwire_kv_pair_t *data)
{
    abseil_hashmap_t *map = static_cast<abseil_hashmap_t *>(ctx);
    try {
        map->values.insert({abseil_key_t{data->key.ptr, data->key.len}, *data});
    } catch (const std::bad_alloc &) {
        return 0;
    }
    return 1;
}

static const hwire_kv_pair_t *
abseil_hashmap_find(const void *ctx, const char *key, size_t len)
{
    const abseil_hashmap_t *map = static_cast<const abseil_hashmap_t *>(ctx);
    auto iter = map->values.find(abseil_key_t{key, len});
    return iter == map->values.end() ? nullptr : &iter->second;
}

extern "C" const hwire_kv_pair_t *
abseil_hashmap_get(const void *ctx, const char *key, size_t len)
{
    return abseil_hashmap_find(ctx, key, len);
}

extern "C" const hwire_kv_pair_t *
abseil_hashmap_get_ci(const void *ctx, const char *key, size_t len)
{
    return abseil_hashmap_find(ctx, key, len);
}

extern "C" const char *abseil_hashmap_name(void) { return "absl::flat_hash_map"; }
extern "C" size_t abseil_hashmap_growths(const void *ctx)
{
    const auto *map = static_cast<const abseil_hashmap_t *>(ctx);
    size_t count = 0;
    for (size_t n = map->initial_slots + 1; n < map->values.capacity() + 1; n *= 2) {
        count++;
    }
    return count;
}

extern "C" double abseil_hashmap_loadfactor(const void *ctx)
{
    const auto *map = static_cast<const abseil_hashmap_t *>(ctx);
    return map->values.load_factor();
}
