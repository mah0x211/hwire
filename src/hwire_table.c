#include "hwire_table.h"
#include <string.h>

#define EMPTY UINT16_C(0)

enum {
    /* Duplicate-next and duplicate-tail each occupy N references. */
    INDEX_NEXT_COUNT_FACTOR  = 1u,
    INDEX_TAIL_COUNT_FACTOR  = 1u,
    INDEX_CHAIN_COUNT_FACTOR = INDEX_NEXT_COUNT_FACTOR +
        INDEX_TAIL_COUNT_FACTOR,

    /* Both comparison modes have independent index regions. */
    INDEX_BOTH_COUNT_FACTOR = 2u
};

/** Return the selected number of hash slots in one index. */
static inline size_t get_index_slot_count(const hwire_table_t *table)
{
    return (size_t)table->mask + 1u;
}

/** Return the full single-index length, including duplicate chains. */
static inline size_t get_index_count(const hwire_table_t *table)
{
    return get_index_slot_count(table) +
           (size_t)table->capacity * INDEX_CHAIN_COUNT_FACTOR;
}

/** Return the total index length for all enabled comparison modes. */
static inline size_t get_index_base_count(const hwire_table_t *table)
{
    size_t count = get_index_count(table);
    return table->mode ==
                   (HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE) ?
               count * INDEX_BOTH_COUNT_FACTOR :
               count;
}

/** Return the default hash-slot region at offset zero. */
static inline hwire_table_index_t *get_slot_index_cs(const hwire_table_t *table)
{
    return table->index;
}

/** Return the secondary slots of a dual-index table. */
static inline hwire_table_index_t *
get_secondary_slot_index(const hwire_table_t *table)
{
    return table->index + get_index_count(table);
}

/** Return CI slots, sharing the default slots in CI-only mode. */
static inline hwire_table_index_t *get_slot_index_ci(const hwire_table_t *table)
{
    return table->mode ==
                   (HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE) ?
               table->index + get_index_count(table) :
               table->index;
}

/** Function pointer type for selecting slot indices. */
typedef hwire_table_index_t *(*get_slot_index_fn)(const hwire_table_t *table);

/** Return duplicate-next references immediately after the default slots. */
static inline hwire_table_index_t *get_next_index_cs(const hwire_table_t *table)
{
    return table->index + get_index_slot_count(table);
}

/** Return CI duplicate-next references after the selected CI slots. */
static inline hwire_table_index_t *get_next_index_ci(const hwire_table_t *table)
{
    return get_slot_index_ci(table) + get_index_slot_count(table);
}

typedef hwire_table_index_t *(*get_next_index_fn)(const hwire_table_t *table);

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

/** Apply one SipHash mixing round to four distinct state words. */
static inline void siphash_round(uint64_t *v0, uint64_t *v1, uint64_t *v2,
                                 uint64_t *v3)
{
    *v0 += *v1;
    *v1 = rotate_left(*v1, 13);
    *v1 ^= *v0;
    *v0 = rotate_left(*v0, 32);
    *v2 += *v3;
    *v3 = rotate_left(*v3, 16);
    *v3 ^= *v2;
    *v0 += *v3;
    *v3 = rotate_left(*v3, 21);
    *v3 ^= *v0;
    *v2 += *v1;
    *v1 = rotate_left(*v1, 17);
    *v1 ^= *v2;
    *v2 = rotate_left(*v2, 32);
}

/**
 * Compute SipHash-1-3 over a borrowed slice in CI mode, folding each byte while
 * loading it. An empty slice may have a NULL data pointer. The byte loads avoid
 * alignment and aliasing assumptions.
 */
static uint64_t hash_siphash_ci(const hwire_table_key_t *key, const char *data,
                                size_t len)
{
    uint64_t v0 = UINT64_C(0x736f6d6570736575) ^ key->words[0];
    uint64_t v1 = UINT64_C(0x646f72616e646f6d) ^ key->words[1];
    uint64_t v2 = UINT64_C(0x6c7967656e657261) ^ key->words[0];
    uint64_t v3 = UINT64_C(0x7465646279746573) ^ key->words[1];
    size_t i    = 0;

    while (len - i >= 8u) {
        uint64_t m = 0;

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
        v3 ^= m;
        siphash_round(&v0, &v1, &v2, &v3);
        v0 ^= m;
        i += 8u;
    }

    uint64_t b = (uint64_t)len << 56u;
    for (unsigned j = 0; i < len; ++i, ++j) {
        unsigned char c = fold((unsigned char)data[i]);
        b |= (uint64_t)c << (8u * j);
    }

    v3 ^= b;
    siphash_round(&v0, &v1, &v2, &v3);
    v0 ^= b;
    v2 ^= UINT64_C(0xff);
    siphash_round(&v0, &v1, &v2, &v3);
    siphash_round(&v0, &v1, &v2, &v3);
    siphash_round(&v0, &v1, &v2, &v3);
    return v0 ^ v1 ^ v2 ^ v3;
}

/**
 * Compute SipHash-1-3 over a borrowed slice. In CS mode, load each byte
 * without folding, without allocating a normalized copy. An empty slice may
 * have a NULL data pointer. The byte loads avoid alignment and aliasing
 * assumptions.
 */
static uint64_t hash_siphash_cs(const hwire_table_key_t *key, const char *data,
                                size_t len)
{
    uint64_t v0 = UINT64_C(0x736f6d6570736575) ^ key->words[0];
    uint64_t v1 = UINT64_C(0x646f72616e646f6d) ^ key->words[1];
    uint64_t v2 = UINT64_C(0x6c7967656e657261) ^ key->words[0];
    uint64_t v3 = UINT64_C(0x7465646279746573) ^ key->words[1];
    size_t i    = 0;

    while (len - i >= 8u) {
        uint64_t m = 0;
        for (unsigned j = 0; j < 8u; ++j) {
            m |= (uint64_t)(unsigned char)data[i + j] << (8u * j);
        }
        v3 ^= m;
        siphash_round(&v0, &v1, &v2, &v3);
        v0 ^= m;
        i += 8u;
    }

    uint64_t b = (uint64_t)len << 56u;
    for (unsigned j = 0; i < len; ++i, ++j) {
        unsigned char c = (unsigned char)data[i];
        b |= (uint64_t)c << (8u * j);
    }

    v3 ^= b;
    siphash_round(&v0, &v1, &v2, &v3);
    v0 ^= b;
    v2 ^= UINT64_C(0xff);
    siphash_round(&v0, &v1, &v2, &v3);
    siphash_round(&v0, &v1, &v2, &v3);
    siphash_round(&v0, &v1, &v2, &v3);
    return v0 ^ v1 ^ v2 ^ v3;
}

#endif

/** Compute a case-sensitive hash using the build target backend. */
static uint64_t compute_bytes_hash_cs(const hwire_table_key_t *key,
                                      const char *data, size_t len)
{
#if defined(HWIRE_TABLE_HAVE_AES)
    return hash_aes_cs(key, data, len);
#else
    return hash_siphash_cs(key, data, len);
#endif
}

/** Compute an ASCII case-insensitive hash using the build target backend. */
static uint64_t compute_bytes_hash_ci(const hwire_table_key_t *key,
                                      const char *data, size_t len)
{
#if defined(HWIRE_TABLE_HAVE_AES)
    return hash_aes_ci(key, data, len);
#else
    return hash_siphash_ci(key, data, len);
#endif
}

/** Function pointer type for computing a byte hash. */
typedef uint64_t (*compute_bytes_hash_fn)(const hwire_table_key_t *key,
                                          const char *data, size_t len);

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
 * storage selected by the indexing mode in one call. All references use index +
 * 1, so zero is both the empty-slot and chain-end sentinel. Copy the key first
 * because it may refer to the prior descriptor. Slot capacity is a supported
 * power-of-two multiplier; the bounded pair capacity keeps index sizes and
 * the 32-bit mask representable.
 */
hwire_table_code_t hwire_table_init(hwire_table_t *table,
                                    const hwire_table_key_t *key,
                                    hwire_table_mode_t mode,
                                    hwire_kv_pair_t *entries, size_t capacity,
                                    hwire_table_index_t *index,
                                    hwire_table_slots_capacity_t slots_capacity)
{
    if (!table || !entries || !index || !key ||
        (mode == 0 ||
         (mode & (uint16_t)~(HWIRE_TABLE_CASE_SENSITIVE |
                             HWIRE_TABLE_CASE_INSENSITIVE)) != 0)) {
        return HWIRE_TABLE_EINVAL;
    }
    if (capacity == 0 || capacity > 32768u ||
        (capacity & (capacity - 1u)) != 0) {
        return HWIRE_TABLE_ECAPACITY;
    }
    if (slots_capacity != HWIRE_TABLE_SLOTS_CAP_2N &&
        slots_capacity != HWIRE_TABLE_SLOTS_CAP_4N &&
        slots_capacity != HWIRE_TABLE_SLOTS_CAP_8N) {
        return HWIRE_TABLE_EINVAL;
    }

    /* Copy before modifying the table: key may point into the old table. */
    hwire_table_key_t key_copy = *key;
    size_t count =
        mode == (HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE) ?
            HWIRE_TABLE_INDEX_BOTH_CAPACITY(capacity, slots_capacity) :
            HWIRE_TABLE_INDEX_CAPACITY(capacity, slots_capacity);
    memset(index, 0, count * sizeof(*index));

    table->entries  = entries;
    table->index    = index;
    table->key      = key_copy;
    table->capacity = (uint16_t)capacity;
    table->len      = 0;
    table->mask     = (uint32_t)(capacity * (size_t)slots_capacity - 1u);
    table->mode     = (uint16_t)mode;
    table->next     = NULL;
    table->tail     = table;
    return HWIRE_TABLE_OK;
}

/** Append only to a full tail and reset the new segment before publishing it.
 * Both modes share single-index storage; dual-index storage is a separate
 * class. A candidate already in this chain cannot be linked a second time. */
hwire_table_code_t hwire_table_link(hwire_table_t *table,
                                    hwire_table_t *next_table)
{
    if (!table || !next_table || next_table->next ||
        next_table->tail != next_table ||
        table->tail->len != table->tail->capacity ||
        ((table->mode ==
          (HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE)) !=
         (next_table->mode ==
          (HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE)))) {
        return HWIRE_TABLE_EINVAL;
    }
    if (next_table == table->tail) {
        return HWIRE_TABLE_EINVAL;
    }
    memset(next_table->index, 0,
           get_index_base_count(next_table) * sizeof(*next_table->index));
    next_table->key   = table->key;
    next_table->mode  = table->mode;
    next_table->len   = 0;
    table->tail->next = next_table;
    table->tail       = next_table;
    return HWIRE_TABLE_OK;
}

/** Unhook the first following segment without changing its stored pairs. */
hwire_table_t *hwire_table_unlink(hwire_table_t *table)
{
    if (!table || !table->next) {
        return NULL;
    }
    hwire_table_t *detached = table->next;
    table->next             = detached->next;
    if (table->tail == detached) {
        table->tail = table;
    }
    detached->next = NULL;
    detached->tail = detached;
    return detached;
}

/** Compare binary slices by length and exact bytes. */
static inline int equal_key_cs(hwire_str_t a, const char *data, size_t len)
{
    return a.len == len && (len == 0 || memcmp(a.ptr, data, len) == 0);
}

/** Compare ASCII-CI slices by length and folded bytes. Complete 8-byte words
 * are folded without depending on byte order. memcpy permits unaligned slices;
 * the tail is compared byte by byte. */
static inline int equal_key_ci(hwire_str_t a, const char *data, size_t len)
{
    if (a.len != len) {
        return 0;
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

/** Function pointer type for comparing keys. */
typedef int (*equal_key_fn)(hwire_str_t key, const char *data, size_t len);

/**
 * Probe one index from its hash bucket, wrapping at mask. Return the first
 * empty or equal slot and write its representative to *head_out. An empty slot
 * always exists because each index has at least twice as many slots as pairs.
 */
static inline uint32_t find_slot(const hwire_table_t *table, const char *key,
                                 size_t keylen, uint64_t hash,
                                 uint16_t *head_out,
                                 get_slot_index_fn get_slot_index,
                                 equal_key_fn equal_key)
{
    uint32_t pos                     = (uint32_t)hash & table->mask;
    const hwire_table_index_t *slots = get_slot_index(table);

    for (;;) {
        uint16_t head = slots[pos];
        if (head != EMPTY &&
            equal_key(table->entries[head - 1u].key, key, keylen)) {
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

/** Probe the exact index using byte comparison. */
static inline uint32_t find_slot_cs(const hwire_table_t *table, const char *key,
                                    size_t keylen, uint64_t hash,
                                    uint16_t *head_out)
{
    return find_slot(table, key, keylen, hash, head_out, get_slot_index_cs,
                     equal_key_cs);
}

/** Probe the ASCII-CI index using folded key comparison. */
static inline uint32_t find_slot_ci(const hwire_table_t *table, const char *key,
                                    size_t keylen, uint64_t hash,
                                    uint16_t *head_out)
{
    return find_slot(table, key, keylen, hash, head_out, get_slot_index_ci,
                     equal_key_ci);
}

/** Function pointer type for finding slots. */
typedef uint32_t (*find_slot_fn)(const hwire_table_t *table, const char *key,
                                 size_t keylen, uint64_t hash,
                                 uint16_t *head_out);

/** Publish a slot representative or append to its duplicate chain. */
static inline void update_index(const hwire_table_t *table,
                                hwire_table_index_t *slots, uint32_t pos,
                                uint16_t head, uint16_t index)
{
    hwire_table_index_t *next = slots + get_index_slot_count(table);
    hwire_table_index_t *tail =
        next + (size_t)table->capacity * INDEX_NEXT_COUNT_FACTOR;
    uint16_t ref = (uint16_t)(index + 1u);

    /* Unused next references remain zero from init/link. Only group
     * representatives need a tail reference. */
    if (head == EMPTY) {
        slots[pos]  = ref;
        tail[index] = ref;
    } else {
        uint16_t head_index         = (uint16_t)(head - 1u);
        next[tail[head_index] - 1u] = ref;
        tail[head_index]            = ref;
    }
}

/** Update the selected CI index for a pair already stored by public push. */
static inline hwire_table_code_t push_ci(hwire_table_t *table,
                                         const hwire_kv_pair_t *pair,
                                         uint16_t index,
                                         get_slot_index_fn get_slot_index)
{
    uint64_t hash =
        compute_bytes_hash_ci(&table->key, pair->key.ptr, pair->key.len);
    uint16_t head = EMPTY;
    uint32_t pos  = find_slot(table, pair->key.ptr, pair->key.len, hash, &head,
                              get_slot_index, equal_key_ci);
    update_index(table, get_slot_index(table), pos, head, index);
    return HWIRE_TABLE_OK;
}

/** Update the default index with case-sensitive hashing and equality. */
static inline void insert_index_cs(hwire_table_t *table,
                                   const hwire_kv_pair_t *pair, uint16_t index)
{
    uint64_t hash =
        compute_bytes_hash_cs(&table->key, pair->key.ptr, pair->key.len);
    uint16_t head = EMPTY;
    uint32_t pos =
        find_slot_cs(table, pair->key.ptr, pair->key.len, hash, &head);
    update_index(table, get_slot_index_cs(table), pos, head, index);
}

/** Store one pair, update its enabled indexes, and select the CI slot region.
 */
hwire_table_code_t hwire_table_push(hwire_table_t *table,
                                    const hwire_kv_pair_t *kv)
{
    if (!table || !kv || (kv->key.len && !kv->key.ptr) ||
        (kv->value.len && !kv->value.ptr)) {
        return HWIRE_TABLE_EINVAL;
    }

    table = table->tail;
    if (table->len >= table->capacity) {
        return HWIRE_TABLE_EFULL;
    }

    hwire_kv_pair_t pair = *kv;
    uint16_t index       = table->len;

/** Store the pair in the table. */
#define STORE_PAIR(table, index, pair)                                         \
    do {                                                                       \
        (table)->entries[index] = pair;                                        \
        (table)->len            = (uint16_t)((index) + 1u);                      \
    } while (0)

    switch (table->mode) {
    default:
        return HWIRE_TABLE_EINVAL;
    case HWIRE_TABLE_CASE_SENSITIVE:
        STORE_PAIR(table, index, pair);
        insert_index_cs(table, &pair, index);
        return HWIRE_TABLE_OK;
    case HWIRE_TABLE_CASE_INSENSITIVE:
        STORE_PAIR(table, index, pair);
        return push_ci(table, &pair, index, get_slot_index_cs);
    case HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE:
        STORE_PAIR(table, index, pair);
        insert_index_cs(table, &pair, index);
        return push_ci(table, &pair, index, get_secondary_slot_index);
    }

#undef STORE_PAIR
}

/** Update the iterator for a case-sensitive entry. */
static inline void update_iter_cs(hwire_table_iter_t *iter,
                                  const hwire_table_t *table, uint16_t index,
                                  uint64_t hash)
{
    iter->table     = table;
    iter->index     = index;
    iter->hashes[0] = hash;
    iter->hashes[1] = 0;
}

/** Update the iterator for a case-insensitive entry. */
static inline void update_iter_ci(hwire_table_iter_t *iter,
                                  const hwire_table_t *table, uint16_t index,
                                  uint64_t hash)
{
    iter->table     = table;
    iter->index     = index;
    iter->hashes[0] = 0;
    iter->hashes[1] = hash;
}

/** Function pointer type for updating the iterator. */
typedef void (*update_iter_fn)(hwire_table_iter_t *iter,
                               const hwire_table_t *table, uint16_t index,
                               uint64_t hash);

/** Probe each segment with one hash and update the cursor on a match. */
static inline const hwire_kv_pair_t *
select_by_index(const hwire_table_t *table, const char *key, size_t keylen,
                uint64_t hash, find_slot_fn lookup_slot,
                hwire_table_iter_t *iter, update_iter_fn update_iter)
{
    for (; table; table = table->next) {
        uint16_t head;

        (void)lookup_slot(table, key, keylen, hash, &head);
        if (head != EMPTY) {
            uint16_t index = (uint16_t)(head - 1u);
            if (iter) {
                update_iter(iter, table, index, hash);
            }
            return &table->entries[index];
        }
    }
    return NULL;
}

/** Hash and search a key when its comparison mode is enabled. */
static inline const hwire_kv_pair_t *
get_key(const hwire_table_t *table, const char *key, size_t keylen,
        hwire_table_mode_t mode, compute_bytes_hash_fn compute_bytes_hash,
        find_slot_fn lookup_slot, hwire_table_iter_t *iter,
        update_iter_fn update_iter)
{
    if (table->len && table->mode & mode) {
        uint64_t hash = compute_bytes_hash(&table->key, key, keylen);
        return select_by_index(table, key, keylen, hash, lookup_slot, iter,
                               update_iter);
    }
    return NULL;
}

/** Prepare and select an exact key with the CS lookup functions. */
static inline const hwire_kv_pair_t *get_key_cs(const hwire_table_t *table,
                                                const char *key, size_t keylen,
                                                hwire_table_iter_t *iter)
{
    return get_key(table, key, keylen, HWIRE_TABLE_CASE_SENSITIVE,
                   compute_bytes_hash_cs, find_slot_cs, iter, update_iter_cs);
}

/** Prepare and select an ASCII-CI key with the CI lookup functions. */
static inline const hwire_kv_pair_t *get_key_ci(const hwire_table_t *table,
                                                const char *key, size_t keylen,
                                                hwire_table_iter_t *iter)
{
    return get_key(table, key, keylen, HWIRE_TABLE_CASE_INSENSITIVE,
                   compute_bytes_hash_ci, find_slot_ci, iter, update_iter_ci);
}

/** Find the first exact key match. */
const hwire_kv_pair_t *hwire_table_get(const hwire_table_t *table,
                                       const char *key, size_t keylen,
                                       hwire_table_iter_t *iter)
{
    return get_key_cs(table, key, keylen, iter);
}

/** Find the first ASCII-CI key match. */
const hwire_kv_pair_t *hwire_table_get_ci(const hwire_table_t *table,
                                          const char *key, size_t keylen,
                                          hwire_table_iter_t *iter)
{
    return get_key_ci(table, key, keylen, iter);
}

/** Advance an exact duplicate while preserving the CI hash cache. */
static inline void update_next_iter_cs(hwire_table_iter_t *iter,
                                       const hwire_table_t *table,
                                       uint16_t index, uint64_t hash)
{
    *iter = (hwire_table_iter_t){
        .table  = table,
        .index  = index,
        .hashes = {[0] = hash, [1] = iter->hashes[1]},
    };
}

/** Advance a CI duplicate and invalidate the case-sensitive hash cache. */
static inline void update_next_iter_ci(hwire_table_iter_t *iter,
                                       const hwire_table_t *table,
                                       uint16_t index, uint64_t hash)
{
    *iter = (hwire_table_iter_t){
        .table  = table,
        .index  = index,
        .hashes = {[0] = 0, [1] = hash},
    };
}

/** Follow the duplicate index selected by the API, independently of the
 * lookup that initialized the cursor. */
static inline const hwire_kv_pair_t *find_next_duplicate(
    hwire_table_iter_t *iter, uint64_t hash, hwire_table_mode_t mode,
    compute_bytes_hash_fn compute_bytes_hash, get_next_index_fn get_next_index,
    find_slot_fn lookup_slot, update_iter_fn update_iter)
{
    if (iter->table && iter->table->mode & mode) {
        const hwire_table_t *table = iter->table;
        uint16_t ref               = get_next_index(iter->table)[iter->index];

        if (ref != EMPTY) {
            uint16_t index = (uint16_t)(ref - 1u);
            update_iter(iter, table, index, hash);
            return &table->entries[index];
        }

        if (table->next) {
            hwire_str_t key = table->entries[iter->index].key;

            if (!hash) {
                // Compute the hash if it hasn't been computed yet.
                hash = compute_bytes_hash(&table->key, key.ptr, key.len);
            }

            for (table = table->next; table; table = table->next) {
                uint16_t head;
                (void)lookup_slot(table, key.ptr, key.len, hash, &head);
                if (head != EMPTY) {
                    uint16_t index = (uint16_t)(head - 1u);
                    update_iter(iter, table, index, hash);
                    return &table->entries[index];
                }
            }
        }
    }
    return NULL;
}

/** Advance to the next exact duplicate at the cursor position. */
const hwire_kv_pair_t *hwire_table_next(hwire_table_iter_t *iter)
{
    return find_next_duplicate(iter, iter->hashes[0],
                               HWIRE_TABLE_CASE_SENSITIVE,
                               compute_bytes_hash_cs, get_next_index_cs,
                               find_slot_cs, update_next_iter_cs);
}

/** Advance to the next ASCII-CI duplicate at the cursor position. */
const hwire_kv_pair_t *hwire_table_next_ci(hwire_table_iter_t *iter)
{
    return find_next_duplicate(iter, iter->hashes[1],
                               HWIRE_TABLE_CASE_INSENSITIVE,
                               compute_bytes_hash_ci, get_next_index_ci,
                               find_slot_ci, update_next_iter_ci);
}

/** Advance after the cursor's current pair; full iteration does not hash keys.
 */
const hwire_kv_pair_t *hwire_table_iterate(const hwire_table_t *table,
                                           hwire_table_iter_t *iter)
{
    const hwire_table_t *segment = iter->table ? iter->table : table;
    size_t index                 = iter->table ? (size_t)iter->index + 1u : 0;
    for (; segment; segment = segment->next) {
        if (index < segment->len) {
            *iter = (hwire_table_iter_t){
                .table = segment,
                .index = (uint16_t)index,
            };
            return &segment->entries[index];
        }
        index = 0;
    }
    return NULL;
}
