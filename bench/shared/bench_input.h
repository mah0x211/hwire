/* The driver selects a seeded permutation of case-insensitively unique keys.
 * Preparation runs before timing; adapters receive valid borrowed slices. */
#ifndef BENCH_INPUT_H
#define BENCH_INPUT_H
#include "../data/headers/headers_512.h"
#include <stdint.h>
#include <string.h>
enum { MAX_KEYS = 256 };
static hwire_kv_pair_t selected_pairs[MAX_KEYS];
static hwire_str_t selected_hits[MAX_KEYS];
static hwire_str_t selected_ci_hits[MAX_KEYS];
static hwire_str_t selected_misses[MAX_KEYS];

static int input_equal_ci(hwire_str_t a, hwire_str_t b)
{
    if (a.len != b.len) {
        return 0;
    }
    for (size_t i = 0; i < a.len; i++) {
        unsigned char x = (unsigned char)a.ptr[i], y = (unsigned char)b.ptr[i];
        x = x >= 'A' && x <= 'Z' ? x + ('a' - 'A') : x;
        y = y >= 'A' && y <= 'Z' ? y + ('a' - 'A') : y;
        if (x != y) {
            return 0;
        }
    }
    return 1;
}
static void prepare_input(void)
{
    size_t unique[HEADER_COUNT];
    size_t n = 0;
    for (size_t i = 0; i < HEADER_COUNT; i++) {
        size_t j = 0;
        while (j < n && !input_equal_ci(HEADER_PAIRS[i].key, HEADER_PAIRS[unique[j]].key)) {
            j++;
        }
        if (j == n) {
            unique[n++] = i;
        }
    }
    /* Fixed xorshift seed makes every implementation/sample use the same set.
     * Fisher-Yates selects without replacement; nested prefixes isolate size. */
    uint32_t seed = 42;
    for (size_t i = n; i > 1; i--) {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        size_t j = seed % i;
        size_t tmp = unique[i - 1];
        unique[i - 1] = unique[j];
        unique[j] = tmp;
    }
    for (size_t i = 0; i < MAX_KEYS; i++) {
        size_t k = unique[i];
        selected_pairs[i] = HEADER_PAIRS[k];
        selected_hits[i] = hits[k];
        selected_ci_hits[i] = ci_hits[k];
        selected_misses[i] = misses[k];
    }
}
#endif
