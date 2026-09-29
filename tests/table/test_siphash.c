/* Exercise the internal hash with the SipHash-2-4 reference vectors. */
#include "../../src/hwire_table.c"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    hwire_table_key_t key = {
        {UINT64_C(0x0706050403020100), UINT64_C(0x0f0e0d0c0b0a0908)}
    };
    char input[16];
    for (unsigned i = 0; i < 16u; ++i) {
        input[i] = (char)i;
    }
    const uint64_t expected[] = {
        UINT64_C(0x726fdb47dd0e0e31), UINT64_C(0x74f839c593dc67fd),
        UINT64_C(0x0d6c8009d9a94f5a), UINT64_C(0x85676696d7fb7e2d)};
    for (size_t i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
        assert(hash_key(&key, input, i, 0) == expected[i]);
    }
    assert(hash_key(&key, input, 15, 0) == UINT64_C(0xa129ca6149be45e5));
    assert(hash_key(&key, "AbCdEfGhI", 9, 1) ==
           hash_key(&key, "abcdefghi", 9, 0));
    assert(hash_key(&key, "AbCdEfGhI", 9, 0) !=
           hash_key(&key, "abcdefghi", 9, 0));

    for (unsigned seed = 0; seed < 256u; ++seed) {
        char raw[32];
        char normalized[32];
        char different[32];
        char unaligned[33];
        uint64_t word          = 0;
        uint64_t expected_word = 0;
        for (unsigned i = 0; i < 32u; ++i) {
            unsigned char byte = (unsigned char)(seed + i * 37u);
            raw[i]             = (char)byte;
            normalized[i]      = (char)fold(byte);
            different[i]       = normalized[i];
            if (i < 8u) {
                word |= (uint64_t)byte << (8u * i);
                expected_word |= (uint64_t)fold(byte) << (8u * i);
            }
        }
        assert(fold_word(word) == expected_word);
        memcpy(unaligned + 1, normalized, sizeof normalized);
        for (size_t len = 0; len <= sizeof raw; ++len) {
            assert(hash_key(&key, raw, len, 1) ==
                   hash_key(&key, normalized, len, 0));
            hwire_str_t slice = {len, raw};
            assert(equal_key(slice, normalized, len, 1));
            assert(equal_key(slice, unaligned + 1, len, 1));
            assert(!equal_key(slice, normalized, len + 1u, 1));
            for (size_t pos = 0; pos < len; ++pos) {
                different[pos] = (char)((unsigned char)normalized[pos] ^ 0x80u);
                assert(!equal_key(slice, different, len, 1));
                different[pos] = normalized[pos];
            }
        }
    }
    puts("SipHash vectors passed");
    return 0;
}
