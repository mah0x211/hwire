#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include "../../src/hwire_table.c"
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(HWIRE_TABLE_HAVE_AES)
# include "aes_vectors.h"

/** Compare the implementation with independent upstream Rust outputs. */
static void test_vectors(void)
{
    char input[1025];
    for (size_t i = 0; i < sizeof input; ++i) {
        input[i] = (char)(unsigned char)i;
    }
    for (size_t k = 0; k < sizeof aes_keys / sizeof aes_keys[0]; ++k) {
        for (size_t n = 0; n < sizeof aes_lengths / sizeof aes_lengths[0];
             ++n) {
            assert(hash_aes(&aes_keys[k], input, aes_lengths[n], 0) ==
                   aes_expected[k][n]);
        }
    }
}
#endif

/** Exercise public lookup with binary, unaligned, and ASCII-folded keys. */
static void check_key(const char *raw, const char *normalized, size_t len)
{
    hwire_table_key_t key = {
        {1, UINT64_MAX}
    };
    hwire_kv_pair_t storage[1];
    hwire_table_index_t index[HWIRE_TABLE_INDEX_BOTH_CAPACITY(1)];
    hwire_table_t table;
    assert(hwire_table_init(&table, storage, index, 1, &key,
                            (HWIRE_TABLE_CASE_SENSITIVE |
                             HWIRE_TABLE_CASE_INSENSITIVE)) == HWIRE_TABLE_OK);
#if defined(HWIRE_TABLE_HAVE_AES)
    assert(hash_key(&table, raw, len, 0) == hash_aes(&key, raw, len, 0));
#else
    assert(hash_key(&table, raw, len, 0) == hash_siphash(&key, raw, len, 0));
#endif
    assert(hash_key(&table, raw, len, 1) ==
           hash_key(&table, normalized, len, 0));
    hwire_kv_pair_t pair = {
        {len, raw },
        {0,   NULL}
    };
    assert(hwire_table_push(&table, &pair) == HWIRE_TABLE_OK);
    const hwire_kv_pair_t *entry = &storage[0];
    assert(entry == &storage[0]);
    assert(hwire_table_get(&table, raw, len) == entry);
    assert(hwire_table_get_ci(&table, raw, len) == entry);
    assert(hwire_table_get_ci(&table, normalized, len) == entry);
}

/** Cover every byte value and each small-length branch with an unaligned slice.
 */
static void test_binary_ci(void)
{
    char raw[258];
    char normalized[258];
    for (unsigned seed = 0; seed < 4u; ++seed) {
        for (size_t i = 0; i < sizeof raw; ++i) {
            unsigned char byte = (unsigned char)(i * 37u + seed);
            raw[i]             = (char)byte;
            normalized[i]      = (byte >= 'A' && byte <= 'Z') ?
                                     (char)(byte + ('a' - 'A')) :
                                     (char)byte;
        }
        for (size_t len = 0; len <= 257u; ++len) {
            check_key(raw + 1, normalized + 1, len);
        }
    }
    check_key(NULL, NULL, 0);
}

/** Place each key immediately before an unreadable page to detect overreads. */
static void test_guard_page(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 1025);
    size_t page = (size_t)page_size;
#if defined(MAP_ANONYMOUS)
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#else
    int flags = MAP_PRIVATE | MAP_ANON;
#endif
    char *region = mmap(NULL, 2u * page, PROT_READ | PROT_WRITE, flags, -1, 0);
    assert(region != MAP_FAILED);
    assert(mprotect(region + page, page, PROT_NONE) == 0);
    char normalized[1025] = {0};
    for (size_t len = 0; len <= sizeof normalized; ++len) {
        if (len > 257u && len != 1023u && len != 1024u && len != 1025u) {
            continue;
        }
        char *raw = region + page - len;
        for (size_t i = 0; i < len; ++i) {
            unsigned char byte = (unsigned char)i;
            raw[i]             = (char)byte;
            normalized[i]      = (byte >= 'A' && byte <= 'Z') ?
                                     (char)(byte + ('a' - 'A')) :
                                     (char)byte;
        }
        check_key(raw, normalized, len);
    }
    assert(munmap(region, 2u * page) == 0);
}

int main(void)
{
#if defined(HWIRE_TABLE_HAVE_AES)
    test_vectors();
#endif
    test_binary_ci();
    test_guard_page();
#if defined(HWIRE_TABLE_HAVE_AES)
    puts("AES boundary tests passed");
#else
    puts("SipHash fallback boundary tests passed");
#endif
    return 0;
}
