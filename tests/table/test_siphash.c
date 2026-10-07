#ifndef HWIRE_NO_AES
# define HWIRE_NO_AES
#endif
/* Exercise SipHash-1-3 with outputs from the author's reference implementation:
 * https://github.com/veorq/SipHash (cROUNDS=1, dROUNDS=3, 8-byte output).
 * Key bytes are 00..0f; input bytes are 00..3f. */
#include "../../src/hwire_table.c"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    hwire_table_key_t key = {
        {UINT64_C(0x0706050403020100), UINT64_C(0x0f0e0d0c0b0a0908)}
    };
    char input[64];
    for (unsigned i = 0; i < 64u; ++i) {
        input[i] = (char)i;
    }
    const uint64_t expected[] = {
        UINT64_C(0xabac0158050fc4dc), UINT64_C(0xc9f49bf37d57ca93),
        UINT64_C(0x82cb9b024dc7d44d), UINT64_C(0x8bf80ab8e7ddf7fb),
        UINT64_C(0xcf75576088d38328), UINT64_C(0xdef9d52f49533b67),
        UINT64_C(0xc50d2b50c59f22a7), UINT64_C(0xd3927d989bb11140),
        UINT64_C(0x369095118d299a8e), UINT64_C(0x25a48eb36c063de4),
        UINT64_C(0x79de85ee92ff097f), UINT64_C(0x70c118c1f94dc352),
        UINT64_C(0x78a384b157b4d9a2), UINT64_C(0x306f760c1229ffa7),
        UINT64_C(0x605aa111c0f95d34), UINT64_C(0xd320d86d2a519956),
        UINT64_C(0xcc4fdd1a7d908b66), UINT64_C(0x9cf2689063dbd80c),
        UINT64_C(0x8ffc389cb473e63e), UINT64_C(0xf21f9de58d297d1c),
        UINT64_C(0xc0dc2f46a6cce040), UINT64_C(0xb992abfe2b45f844),
        UINT64_C(0x7ffe7b9ba320872e), UINT64_C(0x525a0e7fdae6c123),
        UINT64_C(0xf464aeb267349c8c), UINT64_C(0x45cd5928705b0979),
        UINT64_C(0x3a3e35e3ca9913a5), UINT64_C(0xa91dc74e4ade3b35),
        UINT64_C(0xfb0bed02ef6cd00d), UINT64_C(0x88d93cb44ab1e1f4),
        UINT64_C(0x540f11d643c5e663), UINT64_C(0x2370dd1f8c21d1bc),
        UINT64_C(0x81157b6c16a7b60d), UINT64_C(0x4d54b9e57a8ff9bf),
        UINT64_C(0x759f12781f2a753e), UINT64_C(0xcea1a3bebf186b91),
        UINT64_C(0x2cf508d3ada26206), UINT64_C(0xb6101c2da3c33057),
        UINT64_C(0xb3f47496ae3a36a1), UINT64_C(0x626b57547b108392),
        UINT64_C(0xc1d2363299e41531), UINT64_C(0x667cc1923f1ad944),
        UINT64_C(0x65704ffec8138825), UINT64_C(0x24f280d1c28949a6),
        UINT64_C(0xc2ca1cedfaf8876b), UINT64_C(0xc2164bfc9f042196),
        UINT64_C(0xa16e9c9368b1d623), UINT64_C(0x49fb169c8b5114fd),
        UINT64_C(0x9f3143f8df074c46), UINT64_C(0xc6fdaf2412cc86b3),
        UINT64_C(0x7eaf49d10a52098f), UINT64_C(0x1cf313559d292f9a),
        UINT64_C(0xc44a30dda2f41f12), UINT64_C(0x36fae98943a71ed0),
        UINT64_C(0x318fb34c73f0bce6), UINT64_C(0xa27abf3670a7e980),
        UINT64_C(0xb4bcc0db243c6d75), UINT64_C(0x23f8d852fdb71513),
        UINT64_C(0x8f035f4da67d8a08), UINT64_C(0xd89cd0e5b7e8f148),
        UINT64_C(0xf6f4e6bcf7a644ee), UINT64_C(0xaec59ad80f1837f2),
        UINT64_C(0xc3b2f6154b6694e0), UINT64_C(0x9d199062b7bbb3a8)};
    for (size_t i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
        assert(hash_siphash_cs(&key, input, i) == expected[i]);
    }
    assert(hash_siphash_ci(&key, "AbCdEfGhI", 9) ==
           hash_siphash_cs(&key, "abcdefghi", 9));
    assert(hash_siphash_cs(&key, "AbCdEfGhI", 9) !=
           hash_siphash_cs(&key, "abcdefghi", 9));

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
            assert(hash_siphash_ci(&key, raw, len) ==
                   hash_siphash_cs(&key, normalized, len));
            hwire_str_t slice = {len, raw};
            assert(equal_key_ci(slice, normalized, len));
            assert(equal_key_ci(slice, unaligned + 1, len));
            assert(!equal_key_ci(slice, normalized, len + 1u));
            for (size_t pos = 0; pos < len; ++pos) {
                different[pos] = (char)((unsigned char)normalized[pos] ^ 0x80u);
                assert(!equal_key_ci(slice, different, len));
                different[pos] = normalized[pos];
            }
        }
    }
    puts("SipHash vectors passed");
    return 0;
}
