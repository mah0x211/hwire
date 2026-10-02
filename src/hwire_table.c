#include "hwire_table.h"
#include <string.h>

#define EMPTY UINT16_C(0)

enum {
    /* The default slot region starts at the beginning of the index array. */
    INDEX_SLOT_BASE_FACTOR = 0u,

    /* Open addressing uses two slots per pair so a full pair array still has
     * an empty slot that terminates every unsuccessful probe. */
    INDEX_SLOT_COUNT_FACTOR = 2u,

    /* The default next region follows its 2N-slot region. */
    INDEX_NEXT_BASE_FACTOR = INDEX_SLOT_COUNT_FACTOR,

    /* The default tail region follows the N-element next region. */
    INDEX_TAIL_BASE_FACTOR = INDEX_NEXT_BASE_FACTOR + 1u,

    /* One default index occupies 4N elements: 2N slots, N next, and N tail. */
    INDEX_CI_SLOT_BASE_FACTOR = HWIRE_TABLE_INDEX_FACTOR,

    /* The CI next region follows its 2N-slot region at offset 4N. */
    INDEX_CI_NEXT_BASE_FACTOR =
        INDEX_CI_SLOT_BASE_FACTOR + INDEX_SLOT_COUNT_FACTOR,

    /* The CI tail region follows the N-element CI next region. */
    INDEX_CI_TAIL_BASE_FACTOR = INDEX_CI_NEXT_BASE_FACTOR + 1u
};

/** Return the mutable default hash-slot region at offset 0N. */
static inline hwire_table_index_t *index_slot_region(hwire_table_t *table)
{
    return table->index + (size_t)table->capacity * INDEX_SLOT_BASE_FACTOR;
}

/** Return the read-only default hash-slot region at offset 0N. */
static inline const hwire_table_index_t *
index_slot_region_const(const hwire_table_t *table)
{
    return table->index + (size_t)table->capacity * INDEX_SLOT_BASE_FACTOR;
}

/** Return the mutable default duplicate-next region at offset 2N. */
static inline hwire_table_index_t *index_next_region(hwire_table_t *table)
{
    return table->index + (size_t)table->capacity * INDEX_NEXT_BASE_FACTOR;
}

/** Return the read-only default duplicate-next region at offset 2N. */
static inline const hwire_table_index_t *
index_next_region_const(const hwire_table_t *table)
{
    return table->index + (size_t)table->capacity * INDEX_NEXT_BASE_FACTOR;
}

/** Return the mutable default duplicate-tail region at offset 3N. */
static inline hwire_table_index_t *index_tail_region(hwire_table_t *table)
{
    return table->index + (size_t)table->capacity * INDEX_TAIL_BASE_FACTOR;
}

/** Return the mutable CI hash-slot region at offset 4N. */
static inline hwire_table_index_t *index_ci_slot_region(hwire_table_t *table)
{
    return table->index + (size_t)table->capacity * INDEX_CI_SLOT_BASE_FACTOR;
}

/** Return the read-only CI hash-slot region at offset 4N. */
static inline const hwire_table_index_t *
index_ci_slot_region_const(const hwire_table_t *table)
{
    return table->index + (size_t)table->capacity * INDEX_CI_SLOT_BASE_FACTOR;
}

/** Return the mutable CI duplicate-next region at offset 6N. */
static inline hwire_table_index_t *index_ci_next_region(hwire_table_t *table)
{
    return table->index + (size_t)table->capacity * INDEX_CI_NEXT_BASE_FACTOR;
}

/** Return the read-only CI duplicate-next region at offset 6N. */
static inline const hwire_table_index_t *
index_ci_next_region_const(const hwire_table_t *table)
{
    return table->index + (size_t)table->capacity * INDEX_CI_NEXT_BASE_FACTOR;
}

/** Return the mutable CI duplicate-tail region at offset 7N. */
static inline hwire_table_index_t *index_ci_tail_region(hwire_table_t *table)
{
    return table->index + (size_t)table->capacity * INDEX_CI_TAIL_BASE_FACTOR;
}

/** Fold ASCII uppercase only; preserve NUL and all non-ASCII bytes. */
static inline unsigned char fold(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c + ('a' - 'A')) : c;
}

/** Fold ASCII uppercase in eight independent byte lanes. The 0x80 guard in
 * every lane prevents subtraction from borrowing into the next byte. */
static inline uint64_t fold_word(uint64_t word)
{
    const uint64_t high = UINT64_C(0x8080808080808080);
    uint64_t low        = (word & ~high) | high;
    uint64_t ge_a       = (low - UINT64_C(0x4141414141414141)) & high;
    uint64_t ge_end     = (low - UINT64_C(0x5b5b5b5b5b5b5b5b)) & high;
    uint64_t upper      = ge_a & ~ge_end & ~word;
    return word | (upper >> 2u);
}

#include "hwire_table_aes.h"

#if !defined(HWIRE_TABLE_HAVE_AES)
/** Rotate a 64-bit SipHash state word by a nonzero, sub-64 bit count. */
static inline uint64_t rotate_left(uint64_t x, unsigned n)
{
    return (x << n) | (x >> (64u - n));
}

/**
 * Compute SipHash-1-3 over a borrowed slice. In CI mode, fold each byte while
 * loading it, without allocating a normalized copy. An empty slice may have
 * a NULL data pointer. The byte loads avoid alignment and aliasing assumptions.
 */
static uint64_t hash_siphash(const hwire_table_key_t *key, const char *data,
                             size_t len, int ci)
{
    uint64_t v0 = UINT64_C(0x736f6d6570736575) ^ key->words[0];
    uint64_t v1 = UINT64_C(0x646f72616e646f6d) ^ key->words[1];
    uint64_t v2 = UINT64_C(0x6c7967656e657261) ^ key->words[0];
    uint64_t v3 = UINT64_C(0x7465646279746573) ^ key->words[1];
    size_t i    = 0;

# define SIPROUND                                                              \
     do {                                                                      \
         v0 += v1;                                                             \
         v1 = rotate_left(v1, 13);                                             \
         v1 ^= v0;                                                             \
         v0 = rotate_left(v0, 32);                                             \
         v2 += v3;                                                             \
         v3 = rotate_left(v3, 16);                                             \
         v3 ^= v2;                                                             \
         v0 += v3;                                                             \
         v3 = rotate_left(v3, 21);                                             \
         v3 ^= v0;                                                             \
         v2 += v1;                                                             \
         v1 = rotate_left(v1, 17);                                             \
         v1 ^= v2;                                                             \
         v2 = rotate_left(v2, 32);                                             \
     } while (0)

    while (len - i >= 8u) {
        uint64_t m = 0;
        if (ci) {
# if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            memcpy(&m, data + i, sizeof(m));
            m = fold_word(m);
# else
            for (unsigned j = 0; j < 8u; ++j) {
                unsigned char c = (unsigned char)data[i + j];
                c               = fold(c);
                m |= (uint64_t)c << (8u * j);
            }
# endif
        } else {
            for (unsigned j = 0; j < 8u; ++j) {
                m |= (uint64_t)(unsigned char)data[i + j] << (8u * j);
            }
        }
        v3 ^= m;
        SIPROUND;
        v0 ^= m;
        i += 8u;
    }

    uint64_t b = (uint64_t)len << 56u;
    for (unsigned j = 0; i < len; ++i, ++j) {
        unsigned char c = (unsigned char)data[i];
        if (ci) {
            c = fold(c);
        }
        b |= (uint64_t)c << (8u * j);
    }

    v3 ^= b;
    SIPROUND;
    v0 ^= b;
    v2 ^= UINT64_C(0xff);
    SIPROUND;
    SIPROUND;
    SIPROUND;
    return v0 ^ v1 ^ v2 ^ v3;

# undef SIPROUND
}
#endif

/** Hash with the backend selected by the compiler target feature macros. */
static uint64_t hash_key(const hwire_table_t *table, const char *data,
                         size_t len, int ci)
{
#if defined(HWIRE_TABLE_HAVE_AES)
    return hash_aes(&table->key, data, len, ci);
#else
    return hash_siphash(&table->key, data, len, ci);
#endif
}

/** Advance and mix a deterministic seed into one 64-bit key word. */
static inline uint64_t splitmix64(uint64_t *state)
{
    *state += UINT64_C(0x9e3779b97f4a7c15);
    uint64_t x = *state;
    x          = (x ^ (x >> 30u)) * UINT64_C(0xbf58476d1ce4e5b9);
    x          = (x ^ (x >> 27u)) * UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31u);
}

/** Produce two key words from consecutive SplitMix64 outputs. */
void hwire_table_key_init(hwire_table_key_t *key, uint64_t seed)
{
    key->words[0] = splitmix64(&seed);
    key->words[1] = splitmix64(&seed);
}

/**
 * Validate both caller-owned arrays before any write. Zero the complete index
 * storage selected by the CI mode in one call. All references use index + 1,
 * so zero is both the empty-slot and chain-end sentinel. Copy the key first
 * because it may refer to the prior descriptor.
 */
hwire_table_code_t hwire_table_init(hwire_table_t *table,
                                    hwire_kv_pair_t *entries,
                                    hwire_table_index_t *index, size_t capacity,
                                    const hwire_table_key_t *key,
                                    int enabled_ci)
{
    if (!table || !entries || !index || !key) {
        return HWIRE_TABLE_EINVAL;
    }
    if (capacity == 0 || capacity > 32768u ||
        (capacity & (capacity - 1u)) != 0) {
        return HWIRE_TABLE_ECAPACITY;
    }

    /* Copy before modifying the table: key may point into the old table. */
    hwire_table_key_t key_copy = *key;
    uint16_t ci                = enabled_ci != 0 ? UINT16_C(1) : UINT16_C(0);
    size_t index_count = ci != 0 ? HWIRE_TABLE_INDEX_CI_CAPACITY(capacity) :
                                   HWIRE_TABLE_INDEX_CAPACITY(capacity);
    memset(index, 0, index_count * sizeof(*index));

    table->entries    = entries;
    table->index      = index;
    table->key        = key_copy;
    table->capacity   = (uint16_t)capacity;
    table->len        = 0;
    table->mask       = (uint16_t)(capacity * INDEX_SLOT_COUNT_FACTOR - 1u);
    table->enabled_ci = ci;
    return HWIRE_TABLE_OK;
}

/** Compare binary slices by length and bytes under the requested key rule.
 * CI comparison folds complete 8-byte words without depending on byte order.
 * memcpy permits unaligned slices; the tail is compared byte by byte. */
static int equal_key(hwire_str_t a, const char *data, size_t len, int ci)
{
    if (a.len != len) {
        return 0;
    } else if (!ci) {
        return len == 0 || memcmp(a.ptr, data, len) == 0;
    }

    size_t i = 0;
    while (len - i >= sizeof(uint64_t)) {
        uint64_t left;
        uint64_t right;
        memcpy(&left, a.ptr + i, sizeof(left));
        memcpy(&right, data + i, sizeof(right));
        if (fold_word(left) != fold_word(right)) {
            return 0;
        }
        i += sizeof(uint64_t);
    }
    for (; i < len; ++i) {
        if (fold((unsigned char)a.ptr[i]) != fold((unsigned char)data[i])) {
            return 0;
        }
    }
    return 1;
}

/**
 * Probe one index from its hash bucket, wrapping at mask. Return the first
 * empty or equal slot and write its representative to *head. An empty slot
 * always exists because each index has twice as many slots as pair capacity.
 */
static uint32_t find_slot(const hwire_table_t *table, const char *key,
                          size_t keylen, int ci, uint16_t *head_out)
{
    uint32_t pos = (uint32_t)hash_key(table, key, keylen, ci) & table->mask;
    const hwire_table_index_t *slots =
        ci ? index_ci_slot_region_const(table) : index_slot_region_const(table);

    for (;;) {
        uint16_t head = slots[pos];
        if (head != EMPTY &&
            equal_key(table->entries[head - 1u].key, key, keylen, ci)) {
            *head_out = head;
            return pos;
        }
        if (head == EMPTY) {
            *head_out = EMPTY;
            return pos;
        }
        pos = (pos + 1u) & table->mask;
    }
}

/**
 * Locate both representatives before publishing a pair. Update each duplicate
 * chain through the representative's tail, then publish the new length.
 */
hwire_table_code_t hwire_table_push(hwire_table_t *table,
                                    const hwire_kv_pair_t *kv)
{
    if (!table || !kv || (kv->key.len && !kv->key.ptr) ||
        (kv->value.len && !kv->value.ptr)) {
        return HWIRE_TABLE_EINVAL;
    }
    if (table->len >= table->capacity) {
        return HWIRE_TABLE_EFULL;
    }

    /* kv is allowed to refer to an existing entry. */
    hwire_kv_pair_t pair = *kv;
    uint16_t exact_head;
    uint16_t ci_head = EMPTY;
    uint32_t exact_pos =
        find_slot(table, pair.key.ptr, pair.key.len, 0, &exact_head);
    uint32_t ci_pos            = 0;
    uint16_t index             = table->len;
    uint16_t ref               = (uint16_t)(index + 1u);
    hwire_table_index_t *slots = index_slot_region(table);
    hwire_table_index_t *next  = index_next_region(table);
    hwire_table_index_t *tail  = index_tail_region(table);

    if (table->enabled_ci != 0) {
        ci_pos = find_slot(table, pair.key.ptr, pair.key.len, 1, &ci_head);
    }

    table->entries[index] = pair;
    next[index]           = EMPTY;
    tail[index]           = ref;

    if (exact_head == EMPTY) {
        slots[exact_pos] = ref;
    } else {
        uint16_t head_index         = (uint16_t)(exact_head - 1u);
        next[tail[head_index] - 1u] = ref;
        tail[head_index]            = ref;
    }

    if (table->enabled_ci != 0) {
        hwire_table_index_t *ci_slots = index_ci_slot_region(table);
        hwire_table_index_t *ci_next  = index_ci_next_region(table);
        hwire_table_index_t *ci_tail  = index_ci_tail_region(table);
        ci_next[index]                = EMPTY;
        ci_tail[index]                = ref;

        if (ci_head == EMPTY) {
            ci_slots[ci_pos] = ref;
        } else {
            uint16_t head_index               = (uint16_t)(ci_head - 1u);
            ci_next[ci_tail[head_index] - 1u] = ref;
            ci_tail[head_index]               = ref;
        }
    }

    table->len = (uint16_t)(index + 1u);
    return HWIRE_TABLE_OK;
}

/** Resolve one key group for a read without rereading the final slot. */
static inline const hwire_kv_pair_t *
get_key(const hwire_table_t *table, const char *key, size_t keylen, int ci)
{
    if (!table || !table->len || (keylen && !key) ||
        (ci && table->enabled_ci == 0)) {
        return NULL;
    }
    uint16_t head;
    (void)find_slot(table, key, keylen, ci, &head);
    return (head == EMPTY) ? NULL : &table->entries[head - 1u];
}

/** Select the exact index for a public lookup. */
const hwire_kv_pair_t *hwire_table_get(const hwire_table_t *table,
                                       const char *key, size_t keylen)
{
    return get_key(table, key, keylen, 0);
}

/** Select the ASCII-CI index for a public lookup. */
const hwire_kv_pair_t *hwire_table_get_ci(const hwire_table_t *table,
                                          const char *key, size_t keylen)
{
    return get_key(table, key, keylen, 1);
}

/** Follow the exact next region; no probe or comparison is needed. */
const hwire_kv_pair_t *hwire_table_next(const hwire_table_t *table,
                                        const hwire_kv_pair_t *pair)
{
    if (pair) {
        size_t index  = (size_t)(pair - table->entries);
        uint16_t next = index_next_region_const(table)[index];
        return next == EMPTY ? NULL : &table->entries[next - 1u];
    }
    return NULL;
}

/** Follow the CI next region when that optional index is enabled. */
const hwire_kv_pair_t *hwire_table_next_ci(const hwire_table_t *table,
                                           const hwire_kv_pair_t *pair)
{
    if (pair && table->enabled_ci) {
        size_t index  = (size_t)(pair - table->entries);
        uint16_t next = index_ci_next_region_const(table)[index];
        return next == EMPTY ? NULL : &table->entries[next - 1u];
    }
    return NULL;
}

/** Return the next physical pair and advance the caller's position. */
const hwire_kv_pair_t *hwire_table_iterate(const hwire_table_t *table,
                                           hwire_table_iter_t *iter)
{
    return (*iter >= table->len) ? NULL : &table->entries[(*iter)++];
}
