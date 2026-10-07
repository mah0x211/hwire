#ifndef HWIRE_TABLE_AES_H
#define HWIRE_TABLE_AES_H

/* AES byte hashing adapted from aHash's aes_hash.rs and operations.rs:
 * https://github.com/tkaitchuck/aHash/tree/a9d649d18d6aefeef106a48252dc6708ee7f9e47
 * The two 64-bit key words are zero-extended new_with_keys inputs. The shuffle
 * uses the upstream byte-reverse variant, consistently across architectures.
 * This is a keyed map hash, not a cryptographic PRF or MAC.
 *
 * Copyright (c) 2018 Tom Kaitchuck
 *
 * Permission is hereby granted, free of charge, to any
 * person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the
 * Software without restriction, including without
 * limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice
 * shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
 * ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#if !defined(HWIRE_NO_AES) && !defined(HWIRE_NO_SIMD)
# if defined(__aarch64__) && !defined(__AARCH64EB__) && defined(__ARM_NEON) && \
     (defined(__ARM_FEATURE_AES) || defined(__ARM_FEATURE_CRYPTO))
#  define HWIRE_TABLE_HAVE_AES 1
#  include <arm_neon.h>
typedef uint8x16_t table_aes_block_t;

/** Construct a block with little-endian low and high 64-bit lanes. */
static inline table_aes_block_t table_aes_pack(uint64_t low, uint64_t high)
{
    const uint64_t lanes[2] = {low, high};
    return vreinterpretq_u8_u64(vld1q_u64(lanes));
}

/** XOR blocks without changing byte order. */
static inline table_aes_block_t table_aes_xor(table_aes_block_t a,
                                              table_aes_block_t b)
{
    return veorq_u8(a, b);
}

/** Match AESENC: transform with a zero key, then XOR the round key. */
static inline table_aes_block_t table_aes_enc(table_aes_block_t a,
                                              table_aes_block_t key)
{
    return veorq_u8(vaesmcq_u8(vaeseq_u8(a, vdupq_n_u8(0))), key);
}

/** Match AESDEC: inverse transform, then XOR the round key. */
static inline table_aes_block_t table_aes_dec(table_aes_block_t a,
                                              table_aes_block_t key)
{
    return veorq_u8(vaesimcq_u8(vaesdq_u8(a, vdupq_n_u8(0))), key);
}

/** Add modulo 2^64 in two independent lanes. */
static inline table_aes_block_t table_aes_add(table_aes_block_t a,
                                              table_aes_block_t b)
{
    return vreinterpretq_u8_u64(
        vaddq_u64(vreinterpretq_u64_u8(a), vreinterpretq_u64_u8(b)));
}

/** Reverse all sixteen bytes, matching aHash's portable shuffle. */
static inline table_aes_block_t table_aes_reverse(table_aes_block_t a)
{
    table_aes_block_t reversed = vrev64q_u8(a);
    return vextq_u8(reversed, reversed, 8);
}

/** Load exactly sixteen bytes without ASCII folding. */
static inline table_aes_block_t table_aes_load_cs(const char *data)
{
    table_aes_block_t a = vld1q_u8((const unsigned char *)data);
    return a;
}

/** Load exactly sixteen bytes and fold ASCII A-Z. */
static inline table_aes_block_t table_aes_load_ci(const char *data)
{
    table_aes_block_t a = vld1q_u8((const unsigned char *)data);
    table_aes_block_t upper =
        vandq_u8(vcgeq_u8(a, vdupq_n_u8('A')), vcleq_u8(a, vdupq_n_u8('Z')));
    a = vorrq_u8(a, vandq_u8(upper, vdupq_n_u8(0x20)));
    return a;
}

/** Return the low 64-bit lane. */
static inline uint64_t table_aes_low(table_aes_block_t a)
{
    return vgetq_lane_u64(vreinterpretq_u64_u8(a), 0);
}
# elif (defined(__x86_64__) || defined(__i386__)) && defined(__AES__) &&       \
     defined(__SSE2__) && defined(__SSSE3__)
#  define HWIRE_TABLE_HAVE_AES 1
#  include <immintrin.h>
typedef __m128i table_aes_block_t;

/** Construct a block with little-endian low and high 64-bit lanes. */
static inline table_aes_block_t table_aes_pack(uint64_t low, uint64_t high)
{
    return _mm_set_epi64x((int64_t)high, (int64_t)low);
}

/** XOR blocks without changing byte order. */
static inline table_aes_block_t table_aes_xor(table_aes_block_t a,
                                              table_aes_block_t b)
{
    return _mm_xor_si128(a, b);
}

/** Execute one AESENC round including the round-key XOR. */
static inline table_aes_block_t table_aes_enc(table_aes_block_t a,
                                              table_aes_block_t key)
{
    return _mm_aesenc_si128(a, key);
}

/** Execute one AESDEC round including the round-key XOR. */
static inline table_aes_block_t table_aes_dec(table_aes_block_t a,
                                              table_aes_block_t key)
{
    return _mm_aesdec_si128(a, key);
}

/** Add modulo 2^64 in two independent lanes. */
static inline table_aes_block_t table_aes_add(table_aes_block_t a,
                                              table_aes_block_t b)
{
    return _mm_add_epi64(a, b);
}

/** Reverse all sixteen bytes, matching aHash's portable shuffle. */
static inline table_aes_block_t table_aes_reverse(table_aes_block_t a)
{
    const table_aes_block_t order =
        _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
    return _mm_shuffle_epi8(a, order);
}

/** Load exactly sixteen bytes without ASCII folding. */
static inline table_aes_block_t table_aes_load_cs(const char *data)
{
    table_aes_block_t a;
    memcpy(&a, data, sizeof a);
    return a;
}

/** Load exactly sixteen bytes and fold ASCII A-Z. */
static inline table_aes_block_t table_aes_load_ci(const char *data)
{
    table_aes_block_t a;
    memcpy(&a, data, sizeof a);
    table_aes_block_t upper =
        _mm_and_si128(_mm_cmpgt_epi8(a, _mm_set1_epi8('A' - 1)),
                      _mm_cmpgt_epi8(_mm_set1_epi8('Z' + 1), a));
    a = _mm_or_si128(a, _mm_and_si128(upper, _mm_set1_epi8(0x20)));
    return a;
}

/** Return the low 64-bit lane without assuming the ABI of a vector extract. */
static inline uint64_t table_aes_low(table_aes_block_t a)
{
    uint64_t lanes[2];
    memcpy(lanes, &a, sizeof lanes);
    return lanes[0];
}
# endif

#endif /* AES enabled for the build target. */

#if defined(HWIRE_TABLE_HAVE_AES)
/** Read at most eight little-endian bytes without reading outside the slice. */
static inline uint64_t table_aes_read_cs(const char *data, size_t len)
{
    uint64_t value = 0;
    memcpy(&value, data, len);
    return value;
}

static inline uint64_t table_aes_read_ci(const char *data, size_t len)
{
    uint64_t value = 0;
    memcpy(&value, data, len);
    return fold_word(value);
}

/** Mix a block into the independent AES and shuffled-add states. */
static inline void table_aes_mix(table_aes_block_t *enc, table_aes_block_t *sum,
                                 table_aes_block_t value)
{
    *enc = table_aes_dec(*enc, value);
    *sum = table_aes_add(table_aes_reverse(*sum), value);
}

/**
 * Hash bytes using aHash's AES write/finish algorithm and byte-reverse shuffle.
 * The build target must support the required instructions. All loads
 * stay inside the slice; CI folds ASCII only while loading, without allocation.
 */
static uint64_t hash_aes_cs(const hwire_table_key_t *key, const char *data,
                            size_t len)
{
    table_aes_block_t enc =
        table_aes_pack(key->words[0] ^ UINT64_C(0x243f6a8885a308d3),
                       UINT64_C(0x13198a2e03707344));
    table_aes_block_t sum =
        table_aes_pack(key->words[1] ^ UINT64_C(0xa4093822299f31d0),
                       UINT64_C(0x082efa98ec4e6c89));
    table_aes_block_t seed = table_aes_xor(enc, sum);
    enc = table_aes_add(enc, table_aes_pack((uint64_t)len, 0));
    if (len <= 8u) {
        uint64_t low  = 0;
        uint64_t high = 0;
        if (len >= 4u) {
            low  = table_aes_read_cs(data, 4);
            high = table_aes_read_cs(data + len - 4u, 4);
        } else if (len >= 2u) {
            low  = table_aes_read_cs(data, 2);
            high = table_aes_read_cs(data + len - 1u, 1);
        } else if (len == 1u) {
            low  = table_aes_read_cs(data, 1);
            high = low;
        }
        table_aes_mix(&enc, &sum, table_aes_pack(low, high));
    } else if (len <= 16u) {
        table_aes_mix(&enc, &sum,
                      table_aes_pack(table_aes_read_cs(data, 8),
                                     table_aes_read_cs(data + len - 8u, 8)));
    } else if (len <= 32u) {
        table_aes_mix(&enc, &sum, table_aes_load_cs(data));
        table_aes_mix(&enc, &sum, table_aes_load_cs(data + len - 16u));
    } else if (len <= 64u) {
        table_aes_mix(&enc, &sum, table_aes_load_cs(data));
        table_aes_mix(&enc, &sum, table_aes_load_cs(data + 16u));
        table_aes_mix(&enc, &sum, table_aes_load_cs(data + len - 32u));
        table_aes_mix(&enc, &sum, table_aes_load_cs(data + len - 16u));
    } else {
        const char *tail     = data + len - 64u;
        table_aes_block_t t0 = table_aes_load_cs(tail);
        table_aes_block_t t1 = table_aes_load_cs(tail + 16u);
        table_aes_block_t t2 = table_aes_load_cs(tail + 32u);
        table_aes_block_t t3 = table_aes_load_cs(tail + 48u);
        table_aes_block_t c0 = table_aes_enc(seed, t0);
        table_aes_block_t c1 = table_aes_dec(seed, t1);
        table_aes_block_t c2 = table_aes_enc(seed, t2);
        table_aes_block_t c3 = table_aes_dec(seed, t3);
        table_aes_block_t s0 = table_aes_add(seed, t0);
        table_aes_block_t s1 = table_aes_add(
            table_aes_xor(seed, table_aes_pack(UINT64_MAX, UINT64_MAX)), t1);
        s0          = table_aes_add(table_aes_reverse(s0), t2);
        s1          = table_aes_add(table_aes_reverse(s1), t3);
        size_t left = len;
        while (left > 64u) {
            t0 = table_aes_load_cs(data);
            t1 = table_aes_load_cs(data + 16u);
            t2 = table_aes_load_cs(data + 32u);
            t3 = table_aes_load_cs(data + 48u);
            c0 = table_aes_dec(c0, t0);
            c1 = table_aes_dec(c1, t1);
            c2 = table_aes_dec(c2, t2);
            c3 = table_aes_dec(c3, t3);
            s0 = table_aes_add(table_aes_reverse(s0), t0);
            s1 = table_aes_add(table_aes_reverse(s1), t1);
            s0 = table_aes_add(table_aes_reverse(s0), t2);
            s1 = table_aes_add(table_aes_reverse(s1), t3);
            data += 64u;
            left -= 64u;
        }
        table_aes_mix(&enc, &sum, c0);
        table_aes_mix(&enc, &sum, c1);
        table_aes_mix(&enc, &sum, c2);
        table_aes_mix(&enc, &sum, c3);
        table_aes_mix(&enc, &sum, s0);
        table_aes_mix(&enc, &sum, s1);
    }
    table_aes_block_t combined = table_aes_enc(sum, enc);
    return table_aes_low(
        table_aes_dec(table_aes_dec(combined, seed), combined));
}

static uint64_t hash_aes_ci(const hwire_table_key_t *key, const char *data,
                            size_t len)
{
    table_aes_block_t enc =
        table_aes_pack(key->words[0] ^ UINT64_C(0x243f6a8885a308d3),
                       UINT64_C(0x13198a2e03707344));
    table_aes_block_t sum =
        table_aes_pack(key->words[1] ^ UINT64_C(0xa4093822299f31d0),
                       UINT64_C(0x082efa98ec4e6c89));
    table_aes_block_t seed = table_aes_xor(enc, sum);
    enc = table_aes_add(enc, table_aes_pack((uint64_t)len, 0));
    if (len <= 8u) {
        uint64_t low  = 0;
        uint64_t high = 0;
        if (len >= 4u) {
            low  = table_aes_read_ci(data, 4);
            high = table_aes_read_ci(data + len - 4u, 4);
        } else if (len >= 2u) {
            low  = table_aes_read_ci(data, 2);
            high = table_aes_read_ci(data + len - 1u, 1);
        } else if (len == 1u) {
            low  = table_aes_read_ci(data, 1);
            high = low;
        }
        table_aes_mix(&enc, &sum, table_aes_pack(low, high));
    } else if (len <= 16u) {
        table_aes_mix(&enc, &sum,
                      table_aes_pack(table_aes_read_ci(data, 8),
                                     table_aes_read_ci(data + len - 8u, 8)));
    } else if (len <= 32u) {
        table_aes_mix(&enc, &sum, table_aes_load_ci(data));
        table_aes_mix(&enc, &sum, table_aes_load_ci(data + len - 16u));
    } else if (len <= 64u) {
        table_aes_mix(&enc, &sum, table_aes_load_ci(data));
        table_aes_mix(&enc, &sum, table_aes_load_ci(data + 16u));
        table_aes_mix(&enc, &sum, table_aes_load_ci(data + len - 32u));
        table_aes_mix(&enc, &sum, table_aes_load_ci(data + len - 16u));
    } else {
        const char *tail     = data + len - 64u;
        table_aes_block_t t0 = table_aes_load_ci(tail);
        table_aes_block_t t1 = table_aes_load_ci(tail + 16u);
        table_aes_block_t t2 = table_aes_load_ci(tail + 32u);
        table_aes_block_t t3 = table_aes_load_ci(tail + 48u);
        table_aes_block_t c0 = table_aes_enc(seed, t0);
        table_aes_block_t c1 = table_aes_dec(seed, t1);
        table_aes_block_t c2 = table_aes_enc(seed, t2);
        table_aes_block_t c3 = table_aes_dec(seed, t3);
        table_aes_block_t s0 = table_aes_add(seed, t0);
        table_aes_block_t s1 = table_aes_add(
            table_aes_xor(seed, table_aes_pack(UINT64_MAX, UINT64_MAX)), t1);
        s0          = table_aes_add(table_aes_reverse(s0), t2);
        s1          = table_aes_add(table_aes_reverse(s1), t3);
        size_t left = len;
        while (left > 64u) {
            t0 = table_aes_load_ci(data);
            t1 = table_aes_load_ci(data + 16u);
            t2 = table_aes_load_ci(data + 32u);
            t3 = table_aes_load_ci(data + 48u);
            c0 = table_aes_dec(c0, t0);
            c1 = table_aes_dec(c1, t1);
            c2 = table_aes_dec(c2, t2);
            c3 = table_aes_dec(c3, t3);
            s0 = table_aes_add(table_aes_reverse(s0), t0);
            s1 = table_aes_add(table_aes_reverse(s1), t1);
            s0 = table_aes_add(table_aes_reverse(s0), t2);
            s1 = table_aes_add(table_aes_reverse(s1), t3);
            data += 64u;
            left -= 64u;
        }
        table_aes_mix(&enc, &sum, c0);
        table_aes_mix(&enc, &sum, c1);
        table_aes_mix(&enc, &sum, c2);
        table_aes_mix(&enc, &sum, c3);
        table_aes_mix(&enc, &sum, s0);
        table_aes_mix(&enc, &sum, s1);
    }
    table_aes_block_t combined = table_aes_enc(sum, enc);
    return table_aes_low(
        table_aes_dec(table_aes_dec(combined, seed), combined));
}

#endif
#endif
