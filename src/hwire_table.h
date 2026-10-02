#ifndef HWIRE_TABLE_H
#define HWIRE_TABLE_H

#include "hwire.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Round a requested pair count up to a supported table capacity.
 *
 * The result is an integer constant expression when n is one. Valid requests
 * are 1 through 32768; zero and out-of-range requests produce zero. Evaluate
 * n without side effects because this macro may evaluate it more than once.
 */
#define HWIRE_TABLE_CAPACITY(n)                                                \
    ((n) <= 0 || (n) > 32768 ? 0 :                                             \
     (n) <= 1                ? 1 :                                             \
     (n) <= 2                ? 2 :                                             \
     (n) <= 4                ? 4 :                                             \
     (n) <= 8                ? 8 :                                             \
     (n) <= 16               ? 16 :                                            \
     (n) <= 32               ? 32 :                                            \
     (n) <= 64               ? 64 :                                            \
     (n) <= 128              ? 128 :                                           \
     (n) <= 256              ? 256 :                                           \
     (n) <= 512              ? 512 :                                           \
     (n) <= 1024             ? 1024 :                                          \
     (n) <= 2048             ? 2048 :                                          \
     (n) <= 4096             ? 4096 :                                          \
     (n) <= 8192             ? 8192 :                                          \
     (n) <= 16384            ? 16384 :                                         \
                               32768)

/**
 * Key for AES byte hashing or SipHash-1-3. Supply 128 secret bits when
 * hash-flood resistance matters. Hash values are internal and not persistent.
 */
typedef struct {
    uint64_t words[2];
} hwire_table_key_t;

/**
 * @brief Element type for caller-owned table index storage.
 *
 * References encode entry index + 1; zero means an empty slot or the end of a
 * duplicate chain. Applications allocate the required number of elements with
 * HWIRE_TABLE_INDEX_CAPACITY or HWIRE_TABLE_INDEX_CI_CAPACITY.
 */
typedef uint16_t hwire_table_index_t;

/**
 * Exact indexing needs 4N elements: 2N hash slots, N next references, and N
 * tail references. Twice that storage holds the same three regions for both
 * the exact and ASCII case-insensitive indexes.
 */
#define HWIRE_TABLE_INDEX_FACTOR    4u
#define HWIRE_TABLE_INDEX_CI_FACTOR (HWIRE_TABLE_INDEX_FACTOR * 2u)

/** Number of hwire_table_index_t elements required for exact lookup. */
#define HWIRE_TABLE_INDEX_CAPACITY(capacity)                                   \
    ((size_t)(capacity) * HWIRE_TABLE_INDEX_FACTOR)

/** Number of hwire_table_index_t elements required for exact and CI lookup. */
#define HWIRE_TABLE_INDEX_CI_CAPACITY(capacity)                                \
    ((size_t)(capacity) * HWIRE_TABLE_INDEX_CI_FACTOR)

/** Table descriptor; initialize it before using any lookup or iteration API. */
typedef struct {
    hwire_kv_pair_t *entries;
    hwire_table_index_t *index;
    hwire_table_key_t key;
    uint16_t mask;
    uint16_t capacity;
    uint16_t len;
    uint16_t enabled_ci;
} hwire_table_t;

/** Iterator position; includes the end position after 32768 entries. */
typedef uint32_t hwire_table_iter_t;

/** Results of operations that can reject an input or exhaust pair capacity. */
typedef enum {
    HWIRE_TABLE_OK        = 0,
    HWIRE_TABLE_EINVAL    = -1, /**< NULL argument or malformed pair slice */
    HWIRE_TABLE_ECAPACITY = -2, /**< Unsupported initialization capacity */
    HWIRE_TABLE_EFULL     = -3  /**< Pair capacity already exhausted */
} hwire_table_code_t;

/**
 * @brief Expand a 64-bit seed into a deterministic 128-bit hash key.
 * @param key Non-NULL destination.
 * @param seed Caller-selected seed, possibly derived from a pointer or counter.
 *
 * This helper adds no entropy. A predictable seed does not provide deliberate
 * hash-flood resistance; supply a separately generated secret key if needed.
 */
void hwire_table_key_init(hwire_table_key_t *key, uint64_t seed);

/**
 * @brief Initialize or reset a table using caller-owned storage.
 * @param table Non-NULL table descriptor.
 * @param entries Non-NULL mutable array of at least capacity pairs.
 * @param index Non-NULL mutable array. Supply at least
 *              HWIRE_TABLE_INDEX_CAPACITY(capacity) elements for exact-only
 *              indexing or HWIRE_TABLE_INDEX_CI_CAPACITY(capacity) elements
 *              when enabled_ci is nonzero.
 * @param capacity Maximum number of pairs, including duplicates; a power of
 *                 two in [1, 32768].
 * @param key Non-NULL hash key, copied into the table.
 * @param enabled_ci Zero builds only the exact index. A nonzero value also
 *                   builds the ASCII case-insensitive index.
 * @return HWIRE_TABLE_OK, HWIRE_TABLE_EINVAL for NULL arguments, or
 *         HWIRE_TABLE_ECAPACITY for an unsupported capacity.
 *
 * No allocation occurs. Initialization zeroes the complete index array for
 * the selected mode and leaves the pair array unchanged. The build uses AES
 * hashing when the compiler target enables ARM NEON/AES or x86
 * AES/SSE2/SSSE3, otherwise SipHash-1-3. Use target flags such as -mcpu=native
 * on ARM or -march=native (or -maes -mssse3) on x86. Define HWIRE_NO_AES or
 * HWIRE_NO_SIMD to select SipHash-1-3. There is no runtime CPU check. On
 * invalid arguments the table, entries, and index are unchanged. A successful
 * reset invalidates prior pair and iterator results.
 */
hwire_table_code_t hwire_table_init(hwire_table_t *table,
                                    hwire_kv_pair_t *entries,
                                    hwire_table_index_t *index, size_t capacity,
                                    const hwire_table_key_t *key,
                                    int enabled_ci);

/**
 * @brief Append a borrowed key/value pair to every enabled index.
 * @param table Initialized table.
 * @param kv Non-NULL pair to copy by value. A nonempty slice needs a pointer.
 * @return HWIRE_TABLE_OK, HWIRE_TABLE_EINVAL for invalid arguments, or
 *         HWIRE_TABLE_EFULL when capacity has been exhausted.
 *
 * A failed push leaves the table unchanged. The caller retains ownership of
 * key and value bytes. Values are not normalized or combined.
 */
hwire_table_code_t hwire_table_push(hwire_table_t *table,
                                    const hwire_kv_pair_t *kv);

/**
 * @brief Find the first pair whose key matches exactly, byte for byte.
 * @param table Initialized table.
 * @param key Query bytes; may be NULL only when keylen is zero.
 * @param keylen Number of query bytes; embedded NUL bytes are significant.
 * @return First matching pair in push order, or NULL if absent.
 *
 * The table must be initialized; key must be non-NULL if keylen is nonzero.
 * Absence is an ordinary lookup result, not an error code.
 */
const hwire_kv_pair_t *hwire_table_get(const hwire_table_t *table,
                                       const char *key, size_t keylen);

/**
 * @brief Find the first pair after folding only ASCII A-Z to a-z.
 * @param table Initialized table.
 * @param key Query bytes; may be NULL only when keylen is zero.
 * @param keylen Number of query bytes; embedded NUL bytes are significant.
 * @return First matching pair in push order, or NULL if absent or CI indexing
 *         was disabled during initialization.
 *
 * Non-ASCII bytes are compared unchanged; this is not Unicode case folding.
 * The table must be initialized; key must be non-NULL if keylen is nonzero.
 */
const hwire_kv_pair_t *hwire_table_get_ci(const hwire_table_t *table,
                                          const char *key, size_t keylen);

/**
 * @brief Return the next pair with the same exact key.
 * @param table Initialized table that produced pair.
 * @param pair Live pair from table, or NULL to return NULL.
 * @return Next exact duplicate in push order, or NULL at the end.
 */
const hwire_kv_pair_t *hwire_table_next(const hwire_table_t *table,
                                        const hwire_kv_pair_t *pair);

/**
 * @brief Return the next pair with the same ASCII-CI key.
 * @param table Initialized table that produced pair.
 * @param pair Live pair from table, or NULL to return NULL.
 * @return Next CI duplicate in push order, or NULL at the end or when CI
 *         indexing was disabled during initialization.
 */
const hwire_kv_pair_t *hwire_table_next_ci(const hwire_table_t *table,
                                           const hwire_kv_pair_t *pair);

/**
 * @brief Visit every pair in push order, independently of key equality.
 * @param table Initialized table.
 * @param iter Non-NULL position; set *iter to zero before the first call.
 * @return Current pair and advances *iter, or NULL at the end without
 *         changing *iter.
 *
 * Appends during iteration become visible on later calls. A reset invalidates
 * the iterator position.
 */
const hwire_kv_pair_t *hwire_table_iterate(const hwire_table_t *table,
                                           hwire_table_iter_t *iter);

#ifdef __cplusplus
}
#endif
#endif
