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
 * HWIRE_TABLE_INDEX_CAPACITY or HWIRE_TABLE_INDEX_BOTH_CAPACITY.
 */
typedef uint16_t hwire_table_index_t;

/**
 * Slot count relative to pair capacity. Each index keeps at least twice as
 * many slots as pairs, so a full pair array still leaves empty probe slots.
 */
typedef enum {
    HWIRE_TABLE_SLOTS_CAP_2N = 2,
    HWIRE_TABLE_SLOTS_CAP_4N = 4,
    HWIRE_TABLE_SLOTS_CAP_8N = 8
} hwire_table_slots_capacity_t;

/**
 * Elements per pair for one index: the selected slot multiplier plus 2u
 * for N duplicate-next references and N duplicate-tail references.
 */
#define HWIRE_TABLE_INDEX_FACTOR(slots_capacity) ((size_t)(slots_capacity) + 2u)

/** Two indexes each need their own slots, duplicate-next and duplicate-tail. */
#define HWIRE_TABLE_INDEX_BOTH_FACTOR(slots_capacity)                          \
    (HWIRE_TABLE_INDEX_FACTOR(slots_capacity) * 2u)

/** Number of index elements for exact-only or CI-only lookup.
 * Both arguments must be supported values; these macros do not validate them.
 */
#define HWIRE_TABLE_INDEX_CAPACITY(capacity, slots_capacity)                   \
    ((size_t)(capacity) * HWIRE_TABLE_INDEX_FACTOR(slots_capacity))

/** Number of index elements for both exact and CI lookup. */
#define HWIRE_TABLE_INDEX_BOTH_CAPACITY(capacity, slots_capacity)              \
    ((size_t)(capacity) * HWIRE_TABLE_INDEX_BOTH_FACTOR(slots_capacity))

/** Bit set selecting key comparisons; combine the two flags with bitwise OR. */
typedef uint16_t hwire_table_mode_t;

/** Enable exact indexing. */
#define HWIRE_TABLE_CASE_SENSITIVE   UINT16_C(0x01)
/** Enable ASCII-CI indexing. */
#define HWIRE_TABLE_CASE_INSENSITIVE UINT16_C(0x02)

/** Table descriptor; initialize it before using any lookup or iteration API. */
typedef struct hwire_table {
    hwire_kv_pair_t *entries;
    hwire_table_index_t *index;
    hwire_table_key_t key;
    uint32_t mask;
    uint16_t capacity;
    uint16_t len;
    hwire_table_mode_t mode;  /**< Validated indexing flags */
    struct hwire_table *next; /**< Next caller-owned segment, or NULL */
    struct hwire_table *tail; /**< Last segment; authoritative at chain root */
} hwire_table_t;

/** Current pair position. Initialize with {0} for iteration.
 * Fields are maintained by the APIs; do not modify a live cursor. */
typedef struct {
    const hwire_table_t *table;
    uint16_t index;
    hwire_table_mode_t cached; /**< Comparison used for hash; zero if absent */
    uint64_t hash; /**< Cached hash for crossing segments, selected by APIs */
} hwire_table_iter_t;

/** Results of operations that can reject an input or exhaust pair capacity. */
typedef enum {
    HWIRE_TABLE_OK        = 0,
    HWIRE_TABLE_EINVAL    = -1, /**< Invalid argument, pair slice or link */
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
 * @param key Non-NULL hash key, copied into the table.
 * @param mode HWIRE_TABLE_CASE_SENSITIVE, HWIRE_TABLE_CASE_INSENSITIVE, or
 *             their bitwise OR. Zero and unknown bits are invalid.
 * @param entries Non-NULL mutable array of at least capacity pairs.
 * @param capacity Maximum number of pairs, including duplicates; a power of
 *                 two in [1, 32768].
 * @param index Non-NULL mutable array. Supply at least
 *              HWIRE_TABLE_INDEX_CAPACITY(capacity, slots_capacity) elements
 *              for a single index or HWIRE_TABLE_INDEX_BOTH_CAPACITY(capacity,
 *              slots_capacity) when both flags are enabled.
 * @param slots_capacity HWIRE_TABLE_SLOTS_CAP_2N, HWIRE_TABLE_SLOTS_CAP_4N,
 *                       or HWIRE_TABLE_SLOTS_CAP_8N.
 *                       Each enabled index has slots_capacity * capacity slots.
 * @return HWIRE_TABLE_OK on success; HWIRE_TABLE_EINVAL for invalid pointers,
 *         mode or slots_capacity; HWIRE_TABLE_ECAPACITY for invalid capacity.
 *
 * No allocation occurs. Initialization zeroes the complete index array for
 * the selected mode and slot capacity, leaving the pair array unchanged. The
 * build uses AES hashing when the compiler target enables ARM NEON/AES or x86
 * AES/SSE2/SSSE3, otherwise SipHash-1-3. Use target flags such as -mcpu=native
 * on ARM or -march=native (or -maes -mssse3) on x86. Define HWIRE_NO_AES or
 * HWIRE_NO_SIMD to select SipHash-1-3. There is no runtime CPU check. On
 * invalid arguments the table, entries, and index are unchanged. A successful
 * reset invalidates prior pair and iterator results. Unlink all following
 * segments before resetting a chain root; do not reset a linked segment.
 */
hwire_table_code_t
hwire_table_init(hwire_table_t *table, const hwire_table_key_t *key,
                 hwire_table_mode_t mode, hwire_kv_pair_t *entries,
                 size_t capacity, hwire_table_index_t *index,
                 hwire_table_slots_capacity_t slots_capacity);

/**
 * @brief Append an exclusively owned, standalone segment to a full chain.
 * @param table Initialized chain root; its final segment must be full.
 * @param next_table Initialized standalone segment with compatible storage.
 * @return HWIRE_TABLE_OK or HWIRE_TABLE_EINVAL for NULL, self-link, nonfull
 *         tail, already linked segment, or incompatible indexing storage.
 *
 * Single-index segments may use either comparison; dual-index segments may
 * link only to dual-index chains. Copies the root key and mode, clears the
 * appended index and resets its length. Pair bytes are left untouched.
 * Each segment retains its own pair capacity and slot count; linked segments
 * may select different slot capacities. No allocation or reindexing occurs.
 * Rejection changes neither descriptor.
 * The caller must not link storage already owned by another chain. Operate on
 * the chain root for push/link/unlink and do not reset linked descriptors.
 * Link invalidates existing iterators.
 */
hwire_table_code_t hwire_table_link(hwire_table_t *table,
                                    hwire_table_t *next_table);

/**
 * @brief Detach the segment immediately following the chain root.
 * @param table Initialized chain root, or NULL.
 * @return Detached standalone segment, or NULL if none follows.
 *
 * Reconnects the remaining suffix in constant time. Detached pairs and indexes
 * remain readable; the caller owns their storage and may reuse or release it.
 * Unlink invalidates existing iterators for the affected chain.
 */
hwire_table_t *hwire_table_unlink(hwire_table_t *table);

/**
 * @brief Append a borrowed key/value pair to every enabled index.
 * @param table Initialized chain root.
 * @param kv Non-NULL pair to copy by value. A nonempty slice needs a pointer.
 * @return HWIRE_TABLE_OK, HWIRE_TABLE_EINVAL for invalid arguments, or
 *         HWIRE_TABLE_EFULL when capacity has been exhausted.
 *
 * A failed push leaves the table unchanged. The caller retains ownership of
 * key and value bytes. Values are not normalized or combined. Inserts into the
 * final segment; EFULL means that segment needs another linked segment.
 */
hwire_table_code_t hwire_table_push(hwire_table_t *table,
                                    const hwire_kv_pair_t *kv);

/**
 * @brief Find the first pair whose key matches exactly, byte for byte.
 * @param table Initialized table.
 * @param key Query bytes; may be NULL only when keylen is zero.
 * @param keylen Number of query bytes; embedded NUL bytes are significant.
 * @param iter Optional position destination; cleared on failure.
 *             Pass NULL for first-match-only lookup.
 * @return First matching pair in push order, or NULL if absent or exact
 *         indexing was disabled during initialization.
 *
 * The table must be initialized; key must be non-NULL if keylen is nonzero.
 * Absence is an ordinary lookup result, not an error code. Hashes once and
 * searches following segments in insertion order.
 */
const hwire_kv_pair_t *hwire_table_get(const hwire_table_t *table,
                                       const char *key, size_t keylen,
                                       hwire_table_iter_t *iter);

/**
 * @brief Find the first pair after folding only ASCII A-Z to a-z.
 * @param table Initialized table.
 * @param key Query bytes; may be NULL only when keylen is zero.
 * @param keylen Number of query bytes; embedded NUL bytes are significant.
 * @param iter Optional position destination; cleared on failure.
 *             Pass NULL for first-match-only lookup.
 * @return First matching pair in push order, or NULL if absent or CI indexing
 *         was disabled during initialization.
 *
 * Non-ASCII bytes are compared unchanged; this is not Unicode case folding.
 * The table must be initialized; key must be non-NULL if keylen is nonzero.
 */
const hwire_kv_pair_t *hwire_table_get_ci(const hwire_table_t *table,
                                          const char *key, size_t keylen,
                                          hwire_table_iter_t *iter);

/**
 * @brief Return the next pair with the same exact key.
 * @param iter Current position from get, get_ci, iterate or next; may be NULL.
 * @return Next exact duplicate in push order, or NULL at the end or when exact
 *         indexing is disabled. Updates iter on success; otherwise unchanged.
 *
 * Uses the local duplicate index; crossing segments reuses a cached hash or
 * computes it once if absent or the comparison changed. Comparison is selected
 * by this function regardless of how iter was obtained. Reset invalidates the
 * cursor.
 */
const hwire_kv_pair_t *hwire_table_next(hwire_table_iter_t *iter);

/**
 * @brief Return the next pair with the same ASCII-CI key.
 * @param iter Current position, or NULL to return NULL.
 * @return Next CI duplicate in push order, or NULL at the end or when CI
 *         indexing is disabled. Updates iter on success; otherwise unchanged.
 *
 * Uses the local duplicate index and a cached hash across segments. Only pairs
 * after the current position are considered; earlier CI matches are not
 * revisited.
 */
const hwire_kv_pair_t *hwire_table_next_ci(hwire_table_iter_t *iter);

/**
 * @brief Visit every pair in push order, independently of key equality.
 * @param table Initialized chain root containing the cursor's current pair.
 * @param iter Non-NULL position; initialize with {0} before the first call.
 * @return Next pair and updates iter, or NULL at the end without changing it.
 *
 * Appends become visible on later calls. Link, unlink or reset invalidates the
 * cursor position. The same cursor may be used for next/next_ci; iterate then
 * continues after its current pair.
 */
const hwire_kv_pair_t *hwire_table_iterate(const hwire_table_t *table,
                                           hwire_table_iter_t *iter);

#ifdef __cplusplus
}
#endif
#endif
