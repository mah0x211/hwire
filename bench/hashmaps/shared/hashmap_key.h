#ifndef HASHMAP_KEY_H
#define HASHMAP_KEY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** Fold ASCII uppercase only, preserving every other byte. */
static inline unsigned char hashmap_key_fold(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

/** Fold eight independent byte lanes using the same operation as hwire.
 * The high-bit guard prevents subtraction borrowing between byte lanes. */
static inline uint64_t hashmap_key_fold_word(uint64_t word)
{
    const uint64_t high = UINT64_C(0x8080808080808080);
    uint64_t low = (word & ~high) | high;
    uint64_t ge_a = (low - UINT64_C(0x4141414141414141)) & high;
    uint64_t ge_end = (low - UINT64_C(0x5b5b5b5b5b5b5b5b)) & high;
    return word | ((ge_a & ~ge_end & ~word) >> 2u);
}

/** Compare equal-length slices under ASCII case folding. Word loads are
 * alignment-safe and byte-order independent. On a differing word, compare
 * its bytes to preserve the ordering required by CC's comparison callback. */
static inline int hashmap_key_compare_ci(const char *a, const char *b, size_t len)
{
    size_t i = 0;
    while (len - i >= sizeof(uint64_t)) {
        uint64_t left, right;
        memcpy(&left, a + i, sizeof(left));
        memcpy(&right, b + i, sizeof(right));
        if (hashmap_key_fold_word(left) != hashmap_key_fold_word(right)) {
            break;
        }
        i += sizeof(uint64_t);
    }
    for (; i < len; i++) {
        unsigned char left = hashmap_key_fold((unsigned char)a[i]);
        unsigned char right = hashmap_key_fold((unsigned char)b[i]);
        if (left != right) {
            return left < right ? -1 : 1;
        }
    }
    return 0;
}

#endif
