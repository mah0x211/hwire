#include "hwire_table.h"
#include <string.h>

#define EMPTY UINT16_C(0)

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
 * Validate the array size before any write and zero the caller's entry array.
 * All index references use index + 1, so zero is the empty sentinel. Copy the
 * key first because it may refer to the prior descriptor.
 */
hwire_table_code_t hwire_table_init(hwire_table_t *table,
                                    hwire_table_entry_t *entries,
                                    size_t capacity,
                                    const hwire_table_key_t *key)
{
    if (!table || !entries || !key) {
        return HWIRE_TABLE_EINVAL;
    }
    if (capacity == 0 || capacity > 32768u ||
        (capacity & (capacity - 1u)) != 0 ||
        capacity > SIZE_MAX / sizeof(*entries)) {
        return HWIRE_TABLE_ECAPACITY;
    }

    /* Copy before modifying the table: key may point into the old table. */
    hwire_table_key_t key_copy = *key;
    table->entries             = entries;
    table->key                 = key_copy;
    table->capacity            = (uint16_t)capacity;
    table->len                 = 0;
    table->mask                = (uint32_t)(2u * capacity - 1u);
    memset(entries, 0, capacity * sizeof(*entries));
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

/** Read a logical slot without changing either index. */
static inline uint16_t slot_read(const hwire_table_t *table, uint32_t pos,
                                 int ci)
{
    const hwire_table_entry_t *entry = &table->entries[pos / 2u];
    return ci ? entry->slots_ci[pos % 2u] : entry->slots[pos % 2u];
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

    for (;;) {
        uint16_t head = slot_read(table, pos, ci);
        if (head != EMPTY &&
            equal_key(table->entries[head - 1u].kv.key, key, keylen, ci)) {
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

/** Map a logical slot to its mutable field inside the caller's entry array. */
static inline uint16_t *slot_at(hwire_table_t *table, uint32_t pos, int ci)
{
    hwire_table_entry_t *entry = &table->entries[pos / 2u];
    return ci ? &entry->slots_ci[pos % 2u] : &entry->slots[pos % 2u];
}

/**
 * Locate both representatives before publishing a pair. Update each duplicate
 * chain through the representative's tail, then publish the new length.
 */
hwire_table_code_t hwire_table_push(hwire_table_t *table,
                                    const hwire_kv_pair_t *kv,
                                    const hwire_table_entry_t **out_entry)
{
    if (out_entry) {
        *out_entry = NULL;
    }
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
    uint16_t ci_head;
    uint32_t exact_pos =
        find_slot(table, pair.key.ptr, pair.key.len, 0, &exact_head);
    uint32_t ci_pos = find_slot(table, pair.key.ptr, pair.key.len, 1, &ci_head);
    uint16_t index  = table->len;
    uint16_t ref    = (uint16_t)(index + 1u);
    hwire_table_entry_t *entry = &table->entries[index];
    entry->kv                  = pair;
    entry->next                = EMPTY;
    entry->next_ci             = EMPTY;
    entry->tail                = ref;
    entry->tail_ci             = ref;

    if (exact_head == EMPTY) {
        *slot_at(table, exact_pos, 0) = ref;
    } else {
        hwire_table_entry_t *head            = &table->entries[exact_head - 1u];
        table->entries[head->tail - 1u].next = ref;
        head->tail                           = ref;
    }

    if (ci_head == EMPTY) {
        *slot_at(table, ci_pos, 1) = ref;
    } else {
        hwire_table_entry_t *head = &table->entries[ci_head - 1u];
        table->entries[head->tail_ci - 1u].next_ci = ref;
        head->tail_ci                              = ref;
    }

    table->len = (uint16_t)(index + 1u);
    if (out_entry) {
        *out_entry = entry;
    }
    return HWIRE_TABLE_OK;
}

/** Resolve one key group for a read without rereading the final slot. */
static inline const hwire_table_entry_t *
get_key(const hwire_table_t *table, const char *key, size_t keylen, int ci)
{
    if (!table || !table->len || (keylen && !key)) {
        return NULL;
    }
    uint16_t head;
    (void)find_slot(table, key, keylen, ci, &head);
    return (head == EMPTY) ? NULL : &table->entries[head - 1u];
}

/** Select the exact index for a public lookup. */
const hwire_table_entry_t *hwire_table_get(const hwire_table_t *table,
                                           const char *key, size_t keylen)
{
    return get_key(table, key, keylen, 0);
}

/** Select the ASCII-CI index for a public lookup. */
const hwire_table_entry_t *hwire_table_get_ci(const hwire_table_t *table,
                                              const char *key, size_t keylen)
{
    return get_key(table, key, keylen, 1);
}

/** Follow the stored exact duplicate link; no probe or comparison is needed. */
const hwire_table_entry_t *hwire_table_next(const hwire_table_t *table,
                                            const hwire_table_entry_t *entry)
{
    return (!entry || entry->next == EMPTY) ? NULL :
                                              &table->entries[entry->next - 1u];
}

/** Follow the stored ASCII-CI duplicate link. */
const hwire_table_entry_t *hwire_table_next_ci(const hwire_table_t *table,
                                               const hwire_table_entry_t *entry)
{
    return (!entry || entry->next_ci == EMPTY) ?
               NULL :
               &table->entries[entry->next_ci - 1u];
}

/** Return the next physical entry and advance the caller's position. */
const hwire_table_entry_t *hwire_table_iterate(const hwire_table_t *table,
                                               hwire_table_iter_t *iter)
{
    return (*iter >= table->len) ? NULL : &table->entries[(*iter)++];
}
