/**
 *  Copyright (C) 2018-present Masatoshi Teruya
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 *
 *  src/hwire.c
 *  lua-net-http
 *  Zero-Allocation HTTP Parser
 */

#include "hwire.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>

/**
 * @name Character Validation Tables
 * @{
 */

/**
 * @brief TCHAR lookup table for token validation
 *
 * RFC 7230:
 * token = 1*tchar
 * tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*"
 *       / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
 *       / DIGIT / ALPHA
 */
static const unsigned char TCHAR[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    //   "                            (  )            ,            /
    '!', 0, '#', '$', '%', '&', '\'', 0, 0, '*', '+', 0, '-', '.', 0,
    //                                                :  ;  <  =  >  ?  @
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 0, 0, 0, 0, 0, 0, 0,
    // upper case
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
    //   [  \  ]
    'z', 0, 0, 0, '^', '_', '`',
    // lower case
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
    //   {       }
    'z', 0, '|', 0, '~'};

/**
 * @brief VCHAR lookup table for field-content validation
 *
 * RFC 7230:
 * field-content = *VCHAR / obs-text
 * VCHAR          = %x21-7E
 * obs-text       = %x80-FF
 */
static const unsigned char VCHAR[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    // VCHAR 0x21 - 0x7E
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    // except DEL 0x7F
    0,
    // all obs-text 0x80 - 0xFF
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1};

/**
 * @brief QDTEXT lookup table for quoted-string validation
 *
 * RFC 7230:
 * quoted-string = DQUOTE *( qdtext / quoted-pair ) DQUOTE
 * qdtext        = HTAB / SP / %x21 / %x23-5B / %x5D-7E / obs-text
 * quoted-pair   = "\" ( HTAB / SP / VCHAR / obs-text )
 * obs-text      = %x80-FF
 */
static const unsigned char QDTEXT[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, // HTAB
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, // 0x20 - 0x21 SP and exclamation mark
    0, // 0x22 double-quote
    // 0x23 - 0x5B
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1,
    0, // 0x5C backslash
    // 0x5D - 0x7E
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, // DEL 0x7F
    // all obs-text 0x80 - 0xFF
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1};

/**
 * https://tools.ietf.org/html/rfc7230#section-4.1
 * 4.1.  Chunked Transfer Coding
 *
 * chunked-body   = *chunk
 *                  last-chunk
 *                  trailer-part
 *                  CRLF
 *
 * chunk          = chunk-size [ chunk-ext ] CRLF
 *                  chunk-data CRLF
 * chunk-size     = 1*HEXDIG
 * last-chunk     = 1*("0") [ chunk-ext ] CRLF
 *
 * chunk-data     = 1*OCTET ; a sequence of chunk-size octets
 */
static const unsigned char HEXDIGIT[256] = {
    //  ctrl-code: 0-32
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,
    //  SP !  "  #  $  %  &  '  (  )  *  +  ,  -  .  /,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    //  0  1  2  3  4  5  6  7  8  9
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    //  :  ;  <  =  >  ?  @
    0, 0, 0, 0, 0, 0, 0,
    //  A   B   C   D   E   F
    11, 12, 13, 14, 15, 16,
    //  G  H  I  J  K  L  M  N  O  P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]
    //  ^  _  `
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,
    //  a   b   c   d   e   f
    11, 12, 13, 14, 15, 16,
    //  g  h  i  j  k  l  m  n  o  p  q  r  s  t  u  v  w  x  y  z  {  |  }
    //  ~
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/** @} */ /* end of Character Validation Tables */

/**
 * @name Internal Macros
 * @{
 */

#define HT        '\t'
#define SP        ' '
#define CR        '\r'
#define LF        '\n'
#define EQ        '='
#define COLON     ':'
#define SEMICOLON ';'
#define DQUOTE    '"'
#define BACKSLASH '\\'

#define STRTCHAR_NOOP ((void)0)

/**
 * @brief Internal macro: check one character in strtchar_ex loop
 *
 * @param str String being parsed (unsigned char*)
 * @param pos Current position (will be incremented if tchar)
 * @param c Variable to store lowercase tchar (or 0 if not tchar)
 * @param udf User-defined action to execute for each tchar
 */
#define STRTCHAR_EX_CHECK(str, pos, c, udf)                                    \
    {                                                                          \
        c = TCHAR[(unsigned char)(str)[pos]];                                  \
        if (!c)                                                                \
            break;                                                             \
        udf;                                                                   \
        pos++;                                                                 \
    }

/**
 * @brief Internal macro: count consecutive tchar characters with loop unrolling
 *
 * Counts consecutive tchar (token) characters starting at position `pos`,
 * with 8x loop unrolling for performance. Executes `udf` for each matched
 * character (typically lowercase conversion).
 *
 * @param str String being parsed (unsigned char*)
 * @param len Maximum length of string
 * @param pos Starting position (will be modified in-place)
 * @param c Variable to store lowercase tchar (or 0 if not tchar)
 * @param udf User-defined action to execute for each tchar (e.g., ustr[pos] =
 * c)
 * @return Updated position after counting (points to first non-tchar or end)
 *
 * @note This is a statement expression macro that returns the final `pos`
 * value.
 * @note Uses 8x loop unrolling: processes 8 characters per iteration when
 * possible.
 */
#define strtchar_ex(str, len, pos, c, udf)                                     \
    ({                                                                         \
        while (pos + 8 <= (len)) {                                             \
            STRTCHAR_EX_CHECK((str), pos, c, udf);                             \
            STRTCHAR_EX_CHECK((str), pos, c, udf);                             \
            STRTCHAR_EX_CHECK((str), pos, c, udf);                             \
            STRTCHAR_EX_CHECK((str), pos, c, udf);                             \
            STRTCHAR_EX_CHECK((str), pos, c, udf);                             \
            STRTCHAR_EX_CHECK((str), pos, c, udf);                             \
            STRTCHAR_EX_CHECK((str), pos, c, udf);                             \
            STRTCHAR_EX_CHECK((str), pos, c, udf);                             \
        }                                                                      \
        while (pos < (len)) {                                                  \
            STRTCHAR_EX_CHECK((str), pos, c, udf);                             \
        }                                                                      \
        pos;                                                                   \
    })

/** @} */ /* end of Internal Macros */

/**
 * @name Internal Character Validation Functions
 * @{
 */

/**
 * @brief Convert hexadecimal string to size_t
 *
 * Converts a hexadecimal string to a size_t value. Updates `cur` to point to
 * the first non-hexadecimal character.
 *
 * @param str String containing hexadecimal digits
 * @param len Maximum length of string
 * @param cur Updated to point to the first non-hex character
 * @param maxsize Maximum allowed ssize_t value to prevent overflow
 * @return Converted value on success
 * @return HWIRE_ERANGE if value exceeds SSIZE_MAX
 *
 * @note This function is used by hwire_parse_chunksize to parse the chunk-size
 * field.
 * @note HEXDIGIT table is used for fast digit lookup (1-16 for valid hex
 * digits).
 */
static int64_t hex2size(unsigned char *str, size_t len, size_t *cur,
                        uint32_t maxsize)
{
    assert(str != NULL);
    assert(cur != NULL);
    int64_t dec = 0;

    // hex to decimal
    for (size_t pos = 0; pos < len; pos++) {
        unsigned char c = HEXDIGIT[str[pos]];
        if (!c) {
            // found non hexdigit
            *cur = pos;
            return dec;
        }
        // accumulate digit
        dec = (dec << 4) | (c - 1);

        if (dec > (int64_t)maxsize) {
            // result too large
            // limit to max value of 32bit (0x7FFFFFFF)
            return HWIRE_ERANGE;
        }
    }

    *cur = len;
    return dec;
}

/**
 * @brief Check if character is a valid tchar (token character)
 *
 * @param c Character to check
 * @return 1 if TCHAR[c] != 0 (valid tchar), otherwise 0
 *
 * @note The internal TCHAR table returns lowercase for valid tchar,
 *       but this function only returns a boolean.
 */
static inline int is_tchar(unsigned char c)
{
    return TCHAR[c] != 0;
}

/**
 * @brief Check if character is valid field-content (VCHAR or obs-text)
 *
 * @param c Character to check
 * @return 1 if VCHAR[c] == 1, otherwise 0
 */
static inline int is_vchar(unsigned char c)
{
    return VCHAR[c] == 1;
}

/**
 * @brief Count consecutive tchar characters
 *
 * Counts the number of consecutive tchar (token) characters from the
 * beginning of str. This is a simple wrapper around strtchar_ex with
 * STRTCHAR_NOOP (no modification).
 *
 * @param str String to parse (must not be NULL)
 * @param len Maximum length of string
 * @return Number of consecutive tchar characters (0 if first char is not tchar)
 */
static inline size_t strtchar(const unsigned char *str, size_t len)
{
    size_t pos      = 0;
    unsigned char c = 0;
    return strtchar_ex(str, len, pos, c, STRTCHAR_NOOP);
}

/**
 * @brief Skip whitespace characters (SP and HT)
 *
 * Skips spaces (SP) and horizontal tabs (HT) in the input string, up to the
 * specified maximum length.
 *
 * @param str Input string
 * @param len Total length of input string
 * @param pos Input: current position; Output: updated position after skipping
 * @param maxpos Maximum allowed position to skip
 * @return HWIRE_OK on success
 * @return HWIRE_ELEN if position exceeds maxpos
 *
 * @note This function is used to skip BWS (Bad Whitespace) in chunk-size
 * parsing.
 * @note Only SP and HT are considered whitespace (per RFC 7230).
 */
static inline int skip_ws(unsigned char *str, size_t len, size_t *pos,
                          size_t maxpos)
{
    assert(str != NULL);
    assert(pos != NULL);
    size_t cur  = *pos;
    size_t tail = maxpos;

    if (tail > len) {
        tail = len;
    }

    // skip SP and HT
    for (; cur < tail; cur++) {
        switch (str[cur]) {
        case SP:
        case HT:
            continue;

        default:
            // stopped at non-whitespace
            *pos = cur;
            return HWIRE_OK;
        }
    }
    // update position
    *pos = cur;

    if (len > maxpos) {
        // exceeded maxpos
        return HWIRE_ELEN;
    }
    // all whitespace
    return HWIRE_OK;
}

// strvchar_lut: LUT-based implementation (fallback)
static inline size_t strvchar_lut(const unsigned char *str, size_t len)
{
    size_t pos = 0;

#define CHECK_VCHAR()                                                          \
    do {                                                                       \
        if (!is_vchar(str[pos])) {                                             \
            return pos;                                                        \
        }                                                                      \
        pos++;                                                                 \
    } while (0)

    // Process 8 bytes at a time (manual unrolling)
    while (pos + 8 <= len) {
        CHECK_VCHAR();
        CHECK_VCHAR();
        CHECK_VCHAR();
        CHECK_VCHAR();
        CHECK_VCHAR();
        CHECK_VCHAR();
        CHECK_VCHAR();
        CHECK_VCHAR();
    }

#undef CHECK_VCHAR

    // Handle remaining bytes (< 8)
    while (pos < len && is_vchar(str[pos])) {
        pos++;
    }
    return pos;
}

#if defined(__aarch64__) || (defined(__arm__) && defined(__ARM_NEON))
# include <arm_neon.h>

// strvchar_neon: NEON-optimized implementation (16 bytes)
//
// Algorithm: Blacklist approach - detect invalid characters
// - Invalid: 0x00-0x20 (control chars + SP) or 0x7F (DEL)
// - Valid:   0x21-0x7E (VCHAR) or 0x80-0xFF (obs-text)
//
// Each byte in 'invalid' vector is either 0x00 (valid) or 0xFF (invalid).
// We extract the first 8 bytes as a 64-bit mask and find the first invalid
// byte.
//
// Safety analysis for __builtin_ctzll(mask) >> 3:
// - mask is 64 bits (8 bytes), so __builtin_ctzll(mask) max value is 63
// - 63 >> 3 = 7, which is within the 8-byte range (0-7)
// - Combined with pos, the result is always within the 16-byte chunk
static inline size_t strvchar_neon(const unsigned char *str, size_t len)
{
    size_t pos = 0;

    // Pre-compute constants (compile-time)
    const uint8x16_t threshold = vdupq_n_u8(0x21);
    const uint8x16_t del_byte  = vdupq_n_u8(0x7F);

    while (pos + 16 <= len) {
        // Load 16 bytes
        uint8x16_t data = vld1q_u8(str + pos);

        // Detect invalid characters using blacklist approach:
        // - lt_21:  bytes where data < 0x21 (invalid: 0x00-0x20)
        // - eq_7f:  bytes where data == 0x7F (invalid: DEL)
        // - invalid: OR of both conditions (0x00=valid, 0xFF=invalid)
        uint8x16_t lt_21   = vcltq_u8(data, threshold);
        uint8x16_t eq_7f   = vceqq_u8(data, del_byte);
        uint8x16_t invalid = vorrq_u8(lt_21, eq_7f);

        // Interpret the 16-byte vector as two 64-bit integers
        uint64x2_t qdata = vreinterpretq_u64_u8(invalid);

        // Check first 8 bytes (lower 64 bits)
        uint64_t mask1 = vgetq_lane_u64(qdata, 0);
        if (mask1) {
            // Find first set bit and convert to byte position
            // Each byte in mask1 is 0x00 or 0xFF, so ctzll gives the bit
            // position of the first invalid byte. Dividing by 8 (>> 3) converts
            // to byte position. Max value is 63 >> 3 = 7, which is safe.
            return pos + (size_t)(__builtin_ctzll(mask1) >> 3);
        }

        // Check second 8 bytes (upper 64 bits)
        uint64_t mask2 = vgetq_lane_u64(qdata, 1);
        if (mask2) {
            // Same logic as mask1, but add 8 to account for offset
            return pos + 8 + (size_t)(__builtin_ctzll(mask2) >> 3);
        }

        // All 16 bytes are valid, continue to next chunk
        pos += 16;
    }

    // Fall back to LUT for remaining bytes (< 16 bytes)
    return pos + strvchar_lut(str + pos, len - pos);
}

#endif

#if defined(__SSE2__)
# include <emmintrin.h>

// SIMD constants for threshold comparison (as int8_t for signed SIMD ops)
// Used by SSE2/AVX2 implementations to detect invalid characters using
// sign-flip technique: (data ^ 0x80) < (0x21 ^ 0x80) is equivalent to data <
// 0x21 Defined in SSE2 block since AVX2 implies SSE2 (superset)
# define SIMD_SIGN_FLIP         ((int8_t)0x80) // XOR mask to toggle sign bit
# define SIMD_THRESHOLD_SHIFTED ((int8_t)(0x21 ^ 0x80)) // = -95 (0xA1)

// strvchar_sse2: SSE2 optimized implementation (16 bytes)
//
// Algorithm: Blacklist approach using comparison and movemask
// - Invalid: 0x00-0x20 (control chars + SP) or 0x7F (DEL)
// - Valid:   0x21-0x7E (VCHAR) or 0x80-0xFF (obs-text)
//
// Technique: Toggle sign bit (XOR with 0x80) to enable unsigned comparison
// with signed comparison instructions (_mm_cmpgt_epi8).
// - Original:        0x00-0xFF unsigned
// - After XOR 0x80:  0x80-0x7F (now in signed range)
// - Compare with (0x21 ^ 0x80) to detect < 0x21
//
// _mm_movemask_epi8 creates a 16-bit mask where each bit represents
// whether the corresponding byte is invalid (1) or valid (0).
static inline size_t strvchar_sse2(const unsigned char *str, size_t len)
{
    size_t pos = 0;

    // Pre-compute constants (compile-time)
    const __m128i sign_flip = _mm_set1_epi8(SIMD_SIGN_FLIP);
    const __m128i threshold = _mm_set1_epi8(SIMD_THRESHOLD_SHIFTED);
    const __m128i del_byte  = _mm_set1_epi8(0x7F);

    while (pos + 16 <= len) {
        // Load 16 bytes (unaligned load)
        __m128i data =
            _mm_loadu_si128((const __m128i *)(const void *)(str + pos));

        // Toggle sign bit to enable unsigned comparison with signed
        // instructions This transforms the unsigned comparison (data < 0x21)
        // into a signed one
        __m128i data_shifted = _mm_xor_si128(data, sign_flip);

        // Detect invalid characters:
        // - lt_21: bytes where (data ^ 0x80) < (0x21 ^ 0x80), i.e., data < 0x21
        // - eq_7f: bytes where data == 0x7F (DEL)
        __m128i lt_21 = _mm_cmpgt_epi8(threshold, data_shifted);
        __m128i eq_7f = _mm_cmpeq_epi8(data, del_byte);

        // Combine: invalid if lt_21 OR eq_7f
        __m128i invalid = _mm_or_si128(lt_21, eq_7f);

        // Create 16-bit mask: bit i is 1 if byte i is invalid
        int mask = _mm_movemask_epi8(invalid);
        if (mask) {
            // __builtin_ctz(mask) returns the position of the first set bit
            // which corresponds to the first invalid byte position (0-15)
            return pos + (size_t)__builtin_ctz((unsigned int)mask);
        }

        // All 16 bytes are valid, continue to next chunk
        pos += 16;
    }

    // Fall back to LUT for remaining bytes (< 16 bytes)
    return pos + strvchar_lut(str + pos, len - pos);
}

#endif

#if defined(__AVX2__)
# include <immintrin.h>

// strvchar_avx2: AVX2 optimized implementation (32 bytes)
//
// Algorithm: Blacklist approach using 256-bit SIMD
// - Invalid: 0x00-0x20 (control chars + SP) or 0x7F (DEL)
// - Valid:   0x21-0x7E (VCHAR) or 0x80-0xFF (obs-text)
static inline size_t strvchar_avx2(const unsigned char *str, size_t len)
{
    size_t pos = 0;

    // Pre-compute constants (compile-time)
    const __m256i sign_flip = _mm256_set1_epi8(SIMD_SIGN_FLIP);
    const __m256i threshold = _mm256_set1_epi8(SIMD_THRESHOLD_SHIFTED);
    const __m256i del_byte  = _mm256_set1_epi8(0x7F);

    // Process 32 bytes at a time
    while (pos + 32 <= len) {
        __m256i data =
            _mm256_loadu_si256((const __m256i *)(const void *)(str + pos));
        __m256i data_shifted = _mm256_xor_si256(data, sign_flip);
        __m256i lt_21        = _mm256_cmpgt_epi8(threshold, data_shifted);
        __m256i eq_7f        = _mm256_cmpeq_epi8(data, del_byte);
        __m256i invalid      = _mm256_or_si256(lt_21, eq_7f);
        int mask             = _mm256_movemask_epi8(invalid);
        if (mask) {
            return pos + (size_t)__builtin_ctz((unsigned int)mask);
        }
        pos += 32;
    }

    // Fall back to SSE2 for remaining bytes (< 32 bytes)
    return pos + strvchar_sse2(str + pos, len - pos);
}

#endif

// strvchar: count consecutive field-content characters (VCHAR or obs-text)
// Returns the number of consecutive characters from the beginning of str
// that are field-content (VCHAR or obs-text)
static inline size_t strvchar(const unsigned char *str, size_t len)
{
#if defined(__AVX2__)
    return strvchar_avx2(str, len);
#elif defined(__SSE2__)
    return strvchar_sse2(str, len);
#elif defined(__aarch64__) || (defined(__arm__) && defined(__ARM_NEON))
    return strvchar_neon(str, len);
#else
    return strvchar_lut(str, len);
#endif
}

/** @} */ /* end of Internal Character Validation Functions */

/**
 * @name Character Validation Functions
 * @{
 */

int hwire_is_tchar(unsigned char c)
{
    return is_tchar(c);
}

int hwire_is_vchar(unsigned char c)
{
    return is_vchar(c);
}

/**
 * @brief Count consecutive tchar characters
 *
 * Counts the number of consecutive tchar (token) characters from the
 * beginning of str, starting at offset *pos. Updates *pos to the position
 * after the matched characters.
 *
 * @param str String to parse (must not be NULL)
 * @param len Maximum length of string
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @return Number of consecutive tchar characters matched (0 if first char is
 * not tchar)
 */
size_t hwire_parse_tchar(const char *str, size_t len, size_t *pos)
{
    assert(str != NULL);
    assert(pos != NULL);
    size_t cur                = *pos;
    const unsigned char *ustr = (const unsigned char *)str + cur;
    size_t n                  = strtchar(ustr, len - cur);
    *pos += n;
    return n;
}

/**
 * @brief Count consecutive vchar characters
 *
 * Counts the number of consecutive vchar (field-content) characters from the
 * beginning of str, starting at offset *pos. Updates *pos to the position
 * after the matched characters.
 *
 * @param str String to parse (must not be NULL)
 * @param len Maximum length of string
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @return Number of consecutive vchar characters matched (0 if first char is
 * not vchar)
 */
size_t hwire_parse_vchar(const char *str, size_t len, size_t *pos)
{
    assert(str != NULL);
    assert(pos != NULL);
    size_t cur                = *pos;
    const unsigned char *ustr = (const unsigned char *)str + cur;
    size_t n                  = strvchar(ustr, len - cur);
    *pos += n;
    return n;
}

int hwire_parse_quoted_string(const char *str, size_t len, size_t *pos,
                              size_t maxlen)
{
    assert(str != NULL);
    assert(pos != NULL);
    const unsigned char *ustr = (const unsigned char *)str;
    size_t cur                = *pos;
    size_t tail               = cur + maxlen;

    if (cur >= len) {
        // cur exceeds length
        return HWIRE_EAGAIN;
    } else if (ustr[cur] != DQUOTE) {
        // not starting with DQUOTE
        return HWIRE_EILSEQ;
    } else if (tail > len) {
        // adjust tail if exceeds length
        tail = len;
    }
    // Skip opening quote
    cur++;

    // parse quoted-string
    for (; cur < tail; cur++) {
        unsigned char c = ustr[cur];
        if (!QDTEXT[c]) {
            switch (c) {
            case DQUOTE:
                // Found closing quote
                *pos = cur + 1; // Skip closing quote
                return HWIRE_OK;

            case BACKSLASH:
                // quoted-pair = "\" ( HTAB / SP / VCHAR / obs-text )
                if (cur + 1 >= len) {
                    // reach to the end of string, need more bytes
                    *pos = cur;
                    return HWIRE_EAGAIN;
                }
                c = ustr[cur + 1];
                if (is_vchar(c) || c == HT || c == SP) {
                    // valid quoted-pair
                    cur++;
                    continue;
                }
                // fallthrough

            default:
                // found illegal byte sequence
                *pos = cur;
                return HWIRE_EILSEQ;
            }
        }
    }

    if (len > tail) {
        // length exceeds maxlen
        *pos = cur;
        return HWIRE_ELEN;
    }
    // need more bytes
    *pos = cur;
    return HWIRE_EAGAIN;
}

/** @} */ /* end of Character Validation Functions */

/**
 * @name String Parsing Functions
 * @{
 */

/**
 * @brief Parse one parameter
 *
 * Parses a single parameter from a semicolon-separated list.
 *
 *  parameters = *( OWS ";" OWS [ parameter ] )
 *  parameter = parameter-name "=" parameter-value
 *  parameter-name = token
 *  parameter-value = ( token / quoted-string )
 *
 * @param str String to parse (must not be NULL)
 * @param len Maximum length of string
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @param maxpos Maximum position
 * @param cb Callback context (must not be NULL)
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data needed
 * @return HWIRE_EILSEQ for invalid byte sequence
 * @return HWIRE_ELEN if length exceeds maxlen
 * @return HWIRE_EKEYLEN if key length exceeds cb->key_lc.size
 * @return HWIRE_ECALLBACK if callback returned non-zero
 */
static int parse_parameter(char *str, size_t len, size_t *pos, size_t maxpos,
                           hwire_callbacks_t *cb)
{
    hwire_param_t param = {0};
    unsigned char *ustr = (unsigned char *)str;
    size_t cur          = *pos;
    size_t head         = cur;
    size_t tail         = maxpos;

    if (tail > len) {
        // adjust tail if exceeds length
        tail = len;
    }

#define CHECK_POSITON()                                                        \
    *pos = cur;                                                                \
    if (cur == len) {                                                          \
        /* reached end of string, need more bytes */                           \
        return HWIRE_EAGAIN;                                                   \
    } else if (cur >= maxpos) {                                                \
        /* exceeded maxlen */                                                  \
        return HWIRE_ELEN;                                                     \
    }

    // parse parameter-name (token)
    if (cb->key_lc.size > 0) {
        unsigned char c        = 0;
        unsigned char *key_buf = (unsigned char *)cb->key_lc.buf;
        size_t key_len         = cb->key_lc.len;
        size_t key_size        = cb->key_lc.size;
        cur                    = strtchar_ex(ustr, tail, cur, c, {
            // convert to lowercase and store in key_buf
            if (key_len >= key_size) {
                *pos           = cur;
                cb->key_lc.len = key_len;
                return HWIRE_EKEYLEN;
            }
            key_buf[key_len++] = c;
        });
        // update key_lc.len after parsing
        cb->key_lc.len         = key_len;
    } else {
        cur += strtchar(ustr + cur, tail - cur);
    }
    CHECK_POSITON();
    param.key.ptr = str + head;
    param.key.len = cur - head;
    // parameter-name must not be empty
    if (param.key.len == 0) {
        *pos = cur;
        return HWIRE_EILSEQ;
    }

    // check for '='
    if (ustr[cur] != '=') {
        *pos = cur;
        return HWIRE_EILSEQ;
    }
    // skip '='
    cur++;
    CHECK_POSITON();

    // parse parameter-value
    head = cur;
    if (ustr[cur] == DQUOTE) {
        // parse as a quoted-string
        int rc = hwire_parse_quoted_string(str, len, &cur, maxpos - cur);
        if (rc == HWIRE_OK) {
            // cur was updated via &cur pointer
            param.value.ptr = str + head + 1; // skip opening quote
            param.value.len = cur - head - 2; // exclude quotes

            // call callback
            if (cb->param_cb(cb, &param)) {
                return HWIRE_ECALLBACK;
            }
        }
        // update position (cur was already updated by
        // hwire_parse_quoted_string)
        *pos = cur;
        return rc;
    }

    // parse as a token
    param.value.ptr = str + head;
    param.value.len = hwire_parse_tchar(str, tail, &cur);
    if (param.value.len == 0) {
        CHECK_POSITON();
        // parameter-value must not be empty
        return HWIRE_EILSEQ;
    }

#undef CHECK_POSITON

    *pos = cur;

    // call callback
    if (cb->param_cb(cb, &param)) {
        return HWIRE_ECALLBACK;
    }
    return HWIRE_OK;
}

/**
 * @brief Parse parameters from a semicolon-separated list
 *
 * Parses parameters from a semicolon-separated list.
 *
 *  parameters = *( OWS ";" OWS [ parameter ] )
 *
 * Caller must check the character after the last parameter for CRLF or
 * end of string (NULL-terminator).
 *
 * @param str String to parse (must not be NULL)
 * @param len Maximum length of string
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @param maxlen Maximum length (default: HWIRE_MAX_STRLEN)
 * @param maxnparams Maximum number of parameters
 * @param skip_leading_semicolon Non-zero to skip semicolon check for first
 * @param cb Callback context (must not be NULL)
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data needed
 * @return HWIRE_EILSEQ for invalid byte sequence
 * @return HWIRE_ELEN if length exceeds maxlen
 * @return HWIRE_EKEYLEN if key length exceeds cb->key_lc.size
 * @return HWIRE_ECALLBACK if callback returned non-zero
 * @return HWIRE_ENOBUFS if number of parameters exceeds maxnparams
 */
int hwire_parse_parameters(char *str, size_t len, size_t *pos, size_t maxlen,
                           uint8_t maxnparams, int skip_leading_semicolon,
                           hwire_callbacks_t *cb)
{
    assert(str != NULL);
    assert(pos != NULL);
    assert(cb != NULL);
    assert(cb->param_cb != NULL);
    unsigned char *ustr = (unsigned char *)str;
    size_t cur          = *pos;
    size_t maxpos       = cur + maxlen;
    uint8_t nparams     = 0;
    int rv              = HWIRE_OK;

    if (skip_leading_semicolon) {
        // skip leading semicolon if present
        goto CHECK_PARAM;
    }

CHECK_NEXT_PARAM:
    if (skip_ws(ustr, len, &cur, maxpos) != HWIRE_OK) {
        // exceeded maxlen
        *pos = cur;
        return HWIRE_ELEN;
    }

    *pos = cur;
    if (ustr[cur] != SEMICOLON) {
        // no more parameters
        // NOTE: user must check a last character after this function.
        // E.g., for CRLF or end of string (NULL-terminator).
        return HWIRE_OK;
    }
    // skip ';'
    cur++;

    // check position
    *pos = cur;
    if (cur == len) {
        // reached end of string, need more bytes
        return HWIRE_EAGAIN;
    } else if (cur >= maxpos) {
        // exceeded maxlen
        return HWIRE_ELEN;
    }

CHECK_PARAM:
    if (nparams >= maxnparams) {
        // exceeded maximum number of parameters
        return HWIRE_ENOBUFS;
    }

    // skip trailing OWS
    if (skip_ws(ustr, len, &cur, maxpos) != HWIRE_OK) {
        return HWIRE_ELEN;
    }

    // reset key_lc.len before parsing each parameter
    cb->key_lc.len = 0;

    // parse one parameter
    rv = parse_parameter(str, len, &cur, maxpos, cb);
    if (rv == HWIRE_OK) {
        // parsed one parameter, continue to next
        nparams++;
        goto CHECK_NEXT_PARAM;
    }

    // propagate error from parse_parameter
    return rv;
}

/** @} */ /* end of String Parsing Functions */

/**
 * @name Chunked Transfer Coding Functions
 * @{
 */

#include <stdio.h>

/**
 * @brief Parse chunk-size and optional chunk-extensions from a chunk-size line
 *
 * Parses a chunk-size line according to RFC 7230 Section 4.1:
 *   chunk-size = 1*HEXDIG
 *   chunk-ext = *( BWS ";" BWS ext-name [ BWS "=" BWS ext-val ] )
 *   ext-name = token
 *   ext-val = token / quoted-string
 *
 * @param str Input string containing the chunk-size line
 * @param len Length of input string
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @param maxlen Maximum allowed length for parsing
 * @param maxexts Maximum number of chunk-extensions to parse
 * @param cb Callback context (must not be NULL)
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data is needed
 * @return HWIRE_ELEN if parsed length exceeds maxlen
 * @return HWIRE_ERANGE if chunk-size exceeds maxsize
 * @return HWIRE_EEXTNAME if extension name is empty
 * @return HWIRE_EEXTVAL if extension value is invalid
 * @return HWIRE_ECALLBACK if callback returned non-zero
 * @return HWIRE_ENOBUFS if number of extensions exceeds maxexts
 * @return HWIRE_EILSEQ if byte sequence is illegal
 * @return HWIRE_EEOL if end-of-line terminator is invalid
 *
 * @note This function requires CRLF (\\r\\n) as the line terminator.
 * @note Extensions with no value have empty string as value (ptr="" len=0).
 */
int hwire_parse_chunksize(char *str, size_t len, size_t *pos, size_t maxlen,
                          uint8_t maxexts, hwire_callbacks_t *cb)
{
    assert(str != NULL);
    assert(pos != NULL);
    assert(cb != NULL);
    assert(cb->chunksize_cb != NULL);
    unsigned char *ustr = (unsigned char *)str;
    size_t cur          = *pos;
    size_t head         = 0;
    unsigned char *key  = NULL;
    size_t klen         = 0;
    unsigned char *val  = NULL;
    size_t vlen         = 0;
    int64_t size        = 0;
    uint8_t nexts       = 0;

    if (!len) {
        // need more bytes
        return HWIRE_EAGAIN;
    }

    // parse chunk-size
    size = hex2size(ustr, len, &cur, HWIRE_MAX_CHUNKSIZE);
    if (size < 0) {
        // chunk size exceeds maximum allowed size or invalid
        return (int)size;
    } else if (cur == *pos) {
        // no hexadecimal digits found
        return HWIRE_EILSEQ;
    }

    // call chunksize callback
    if (cb->chunksize_cb(cb, (uint32_t)size)) {
        return HWIRE_ECALLBACK;
    }

    // 4.1.1.  Chunk Extensions
    //
    // chunk-ext    = *( BWS ";" BWS ext-name [ BWS "=" BWS ext-val ] )
    // ext-name     = token
    // ext-val      = token / quoted-string
    //
    // trailer-part = *( header-field CRLF )
    //
    // OWS (Optional Whitespace)        = *( SP / HTAB )
    // BWS (Must be removed by parser)  = OWS
    //                                  ; "bad" whitespace
    //
    // quoted-string  = DQUOTE *( qdtext / quoted-pair ) DQUOTE
    // qdtext         = HTAB / SP / %x21 / %x23-5B / %x5D-7E / obs-text
    // quoted-pair    = "\" ( HTAB / SP / VCHAR / obs-text )
    // obs-text       = %x80-FF
    //
CHECK_EOL:

#define skip_bws()                                                             \
    do {                                                                       \
        if (skip_ws(ustr, len, &cur, maxlen) != HWIRE_OK) {                    \
            return HWIRE_ELEN;                                                 \
        } else if (ustr[cur] == 0) {                                           \
            /* more bytes need */                                              \
            return HWIRE_EAGAIN;                                               \
        }                                                                      \
    } while (0)

    // skip BWS
    skip_bws();

    switch (ustr[cur]) {
    default:
        // illegal byte sequence
        return HWIRE_EILSEQ;

    // found tail
    case CR:
        cur++;
        if (!ustr[cur]) {
            // more bytes need
            return HWIRE_EAGAIN;
        } else if (ustr[cur] != LF) {
            // invalid end-of-line terminator
            return HWIRE_EEOL;
        }
    case LF:
        // call extension callback for last extension
        if (klen) {
            hwire_chunksize_ext_t ext = {
                .key   = {.len = klen, .ptr = (const char *)key              },
                .value = {.len = vlen, .ptr = (vlen) ? (const char *)val : ""}
            };
            if (cb->chunksize_ext_cb(cb, &ext)) {
                return HWIRE_ECALLBACK;
            }
        }
        // return and number of bytes consumed
        *pos = cur + 1;
        return HWIRE_OK;
        break;

    case SEMICOLON:
        // has chunk-extensions
        cur++;
        skip_bws();
    }

    // parse chunk-extensions
    if (nexts >= maxexts) {
        // exceeded maximum number of extensions
        return HWIRE_ENOBUFS;
    }

    // call extension callback for previous extension
    if (klen) {
        hwire_chunksize_ext_t ext = {
            .key   = {.len = klen, .ptr = (const char *)key              },
            .value = {.len = vlen, .ptr = (vlen) ? (const char *)val : ""}
        };
        if (cb->chunksize_ext_cb(cb, &ext)) {
            return HWIRE_ECALLBACK;
        }
        nexts++;
        klen = 0;
        vlen = 0;
    }

    // parse ext-name
    head = cur;
    hwire_parse_tchar(str, len, &cur);
    if (cur == head) {
        // disallow empty ext-name (invalid extension name)
        return HWIRE_EEXTNAME;
    }
    key  = ustr + head;
    klen = cur - head;

    skip_bws();
    if (ustr[cur] != EQ) {
        // no ext-value
        goto CHECK_EOL;
    }
    // skip '='
    cur++;
    skip_bws();

    // parse ext-value
    if (ustr[cur] == DQUOTE) {
        int rv = 0;
        // parse as a quoted-string
        head   = cur + 1;
        vlen   = maxlen;
        rv     = hwire_parse_quoted_string((const char *)str, len, &cur, vlen);
        if (rv == HWIRE_OK) {
            vlen = cur - head - 1; // exclude closing quote
            val  = ustr + head;    // skip opening quote
            goto CHECK_EOL;
        }
        // treat an illegal byte sequence as extension value error
        return (rv == HWIRE_EILSEQ) ? HWIRE_EEXTVAL : rv;
    }
    // parse as a token
    head = cur;
    hwire_parse_tchar(str, len, &cur);
    val  = ustr + head;
    vlen = cur - head;

    goto CHECK_EOL;

#undef skip_bws
}

/** @} */ /* end of Chunked Transfer Coding Functions */

/**
 * @name HTTP Headers Parsing Functions
 * @{
 */

/**
 * @brief Parse header value
 *
 * Ported from parse.c:parse_hval
 */
static int parse_hval(unsigned char *str, size_t len, size_t *cur,
                      size_t *maxlen)
{
    size_t pos       = 0;
    size_t ows_start = SIZE_MAX;
    size_t max       = (len > *maxlen) ? *maxlen : len;

CHECK_NEXT:
    pos += strvchar(str + pos, max - pos);
    if (pos < max) {
        // stop at first non VCHAR/obs-text
        unsigned char c = str[pos];
        switch (c) {
        case HT:
        case SP:
            // skip OWS
            ows_start = pos;
            pos++;
            while (pos < max && (str[pos] == HT || str[pos] == SP)) {
                pos++;
            }
            if (pos < max && is_vchar(str[pos])) {
                // continue with field-content (vchar)
                ows_start = SIZE_MAX;
            }
            goto CHECK_NEXT;

        case CR:
            if (!str[pos + 1]) {
                // null-terminator
                break;
            }
            if (str[pos + 1] != LF) {
                // invalid end-of-line terminator
                return HWIRE_EEOL;
            }
        case LF:
            // set tail position
            *cur    = pos + 1 + (c == CR);
            *maxlen = (ows_start == SIZE_MAX) ? pos : ows_start;
            return HWIRE_OK;

        // invalid
        default:
            return HWIRE_EHDRVALUE;
        }
    }

    // header-length too large
    if (len > max) {
        return HWIRE_EHDRLEN;
    }

    return HWIRE_EAGAIN;
}

/**
 * @brief Parse header key and store lowercase in key_lc
 *
 * Ported from parse.c:parse_hkey
 */
static int parse_hkey(unsigned char *str, size_t len, size_t *cur,
                      size_t *maxlen, hwire_callbacks_t *cb)
{
    size_t pos       = 0;
    size_t max       = (len > *maxlen) ? *maxlen : len;
    size_t tchar_len = 0;

    if (cb->key_lc.size > 0) {
        unsigned char c        = 0;
        unsigned char *key_buf = (unsigned char *)cb->key_lc.buf;
        size_t key_len         = cb->key_lc.len;
        size_t key_size        = cb->key_lc.size;
        tchar_len              = strtchar_ex(str, max, pos, c, {
            // convert to lowercase and store in key_buf
            if (key_len >= key_size) {
                // Restore before returning error
                cb->key_lc.len = key_len;
                return HWIRE_EKEYLEN;
            }
            key_buf[key_len++] = c;
        });
        // Update the structure
        cb->key_lc.len         = key_len;
    } else {
        tchar_len = strtchar(str, max);
    }

    if (tchar_len == 0) {
        // Empty or first character is invalid
        return HWIRE_EHDRNAME;
    }

    if (tchar_len < max) {
        // strtchar stopped before maxlen - check why
        if (str[tchar_len] == ':') {
            // Found colon - success
            *maxlen = tchar_len;
            *cur    = tchar_len + 1;
            return HWIRE_OK;
        }
        // Non-tchar, non-colon character - error
        return HWIRE_EHDRNAME;
    }

    // All characters up to maxlen were tchar
    if (len > max) {
        // More data available but exceeded maxlen
        return HWIRE_EHDRLEN;
    }

    return HWIRE_EAGAIN;
}

/**
 * @brief Parse HTTP headers
 *
 * Ported from parse.c:parse_header
 */
int hwire_parse_headers(char *str, size_t len, size_t *pos, size_t maxlen,
                        uint8_t maxnhdrs, hwire_callbacks_t *cb)
{
    assert(str != NULL);
    assert(pos != NULL);
    assert(cb != NULL);
    assert(cb->header_cb != NULL);
    unsigned char *ustr = (unsigned char *)str;
    unsigned char *top  = ustr;
    unsigned char *head = 0;
    uint8_t nhdr        = 0;
    size_t cur          = 0;
    int rv              = 0;
    size_t klen         = 0;
    size_t vlen         = 0;
    hwire_header_t header;

RETRY:
    switch (*ustr) {
    // need more bytes
    case 0:
        return HWIRE_EAGAIN;

    // check header-tail
    case CR:
        // null-terminated
        if (!ustr[1]) {
            return HWIRE_EAGAIN;
        } else if (ustr[1] == LF) {
            // skip CR
            ustr++;
        case LF:
            // skip LF
            ustr++;
            *pos = (size_t)(ustr - top);
            return HWIRE_OK;
        }
    }

    // check maximum header number constraint
    if (nhdr >= maxnhdrs) {
        return HWIRE_ENOBUFS;
    }
    nhdr++;

    head           = ustr;
    klen           = maxlen;
    cb->key_lc.len = 0;
    // parse key and store lowercase in key_lc
    rv             = parse_hkey(ustr, len, &cur, &klen, cb);
    if (rv != HWIRE_OK) {
        return rv;
    }

    // skip OWS
    while (ustr[cur] == SP || ustr[cur] == HT) {
        cur++;
    }

    // re-check maximum header length constraint
    if (cur > maxlen) {
        return HWIRE_EHDRLEN;
    }
    ustr += cur;
    len -= cur;

    header.value.ptr = (const char *)ustr;
    vlen             = maxlen - (size_t)(ustr - head);
    // field-value = *field-content
    // RFC 7230 3.2 / RFC 9112 5.5: Field Values
    // Note: Empty field-value is allowed.
    rv               = parse_hval(ustr, len, &cur, &vlen);
    if (rv != HWIRE_OK) {
        return rv;
    }
    ustr += cur;
    len -= cur;

    // set header key and value
    header.key.ptr   = (const char *)head;
    header.key.len   = klen;
    header.value.len = vlen;

    // call callback
    if (cb->header_cb(cb, &header) != 0) {
        return HWIRE_ECALLBACK;
    }

    goto RETRY;
}

/** @} */ /* end of HTTP Headers Parsing Functions */

/**
 * @name HTTP Request Parsing Functions
 * @{
 */

/**
 * @brief Parse HTTP version string
 *
 * @param str String to parse (must not be NULL)
 * @param len Length of string
 * @param pos Output: position after version string (must not be NULL)
 * @param version Output: HTTP version enum
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data needed
 * @return HWIRE_EVERSION for invalid version
 */
static int parse_version(const unsigned char *str, size_t len, size_t *pos,
                         hwire_http_version_t *version)
{
#define VER_LEN 8
    if (len < VER_LEN) {
        return HWIRE_EAGAIN;
    }
    if (memcmp(str, "HTTP/1.1", VER_LEN) == 0) {
        *version = HWIRE_HTTP_V11;
        *pos     = VER_LEN;
        return HWIRE_OK;
    } else if (memcmp(str, "HTTP/1.0", VER_LEN) == 0) {
        *version = HWIRE_HTTP_V10;
        *pos     = VER_LEN;
        return HWIRE_OK;
    }
    return HWIRE_EVERSION;
#undef VER_LEN
}

/**
 * @brief Parse HTTP method
 *
 * Parses method as 1*tchar followed by SP.
 *
 * @param str String to parse (must not be NULL)
 * @param len Length of string
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @param method Output: method string slice
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data needed
 * @return HWIRE_EMETHOD for invalid method (not tchar or no SP)
 */
static int parse_method(unsigned char *str, size_t len, size_t *pos,
                        hwire_str_t *method)
{
    size_t cur  = 0;
    size_t mlen = 0;

    // method = 1*tchar
    mlen = hwire_parse_tchar((const char *)str, len, &cur);
    if (mlen == 0) {
        // first character is not tchar
        return HWIRE_EMETHOD;
    }

    // SP is required
    if (cur >= len) {
        return HWIRE_EAGAIN;
    }
    if (str[cur] != SP) {
        return HWIRE_EMETHOD;
    }

    method->ptr = (const char *)str;
    method->len = mlen;
    *pos        = cur + 1; // skip SP
    return HWIRE_OK;
}

int hwire_parse_request(char *str, size_t len, size_t *pos, size_t maxlen,
                        uint8_t maxnhdrs, hwire_callbacks_t *cb)
{
    assert(str != NULL);
    assert(pos != NULL);
    assert(cb != NULL);
    assert(cb->request_cb != NULL);
    assert(cb->header_cb != NULL);
    unsigned char *ustr = (unsigned char *)str;
    unsigned char *top  = ustr;
    hwire_request_t req;
    size_t cur = 0;
    int rv     = 0;

SKIP_NEXT_CRLF:
    switch (*ustr) {
    // need more bytes
    case 0:
        return HWIRE_EAGAIN;

    case CR:
    case LF:
        ustr++;
        len--;
        goto SKIP_NEXT_CRLF;
    }

    // parse method
    rv = parse_method(ustr, len, &cur, &req.method);
    if (rv != HWIRE_OK) {
        return rv;
    }
    ustr += cur;
    len -= cur;

    // parse-uri (find SP delimiter)
    req.uri.ptr = (const char *)ustr;
    if (len > maxlen) {
        unsigned char *sp = memchr(ustr, SP, maxlen);
        if (!sp) {
            return HWIRE_ELEN;
        }
        ustr = sp;
    } else {
        unsigned char *sp = memchr(ustr, SP, len);
        if (!sp) {
            return HWIRE_EAGAIN;
        }
        ustr = sp;
    }
    req.uri.len = (size_t)(ustr - (unsigned char *)req.uri.ptr);
    ustr++;
    len -= req.uri.len + 1;

    // parse version
    rv = parse_version(ustr, len, &cur, &req.version);
    if (rv != HWIRE_OK) {
        return rv;
    }

    // check end-of-line after version
    switch (ustr[cur]) {
    case 0:
        return HWIRE_EAGAIN;

    case CR:
        // null-terminated
        if (!ustr[cur + 1]) {
            return HWIRE_EAGAIN;
        }
        // invalid end-of-line terminator
        else if (ustr[cur + 1] != LF) {
            return HWIRE_EEOL;
        }
        cur++;

    case LF:
        cur++;
        break;

    default:
        return HWIRE_EVERSION;
    }

    ustr += cur;
    len -= cur;

    // call request callback
    if (cb->request_cb(cb, &req) != 0) {
        return HWIRE_ECALLBACK;
    }

    // parse headers
    cur = 0;
    rv  = hwire_parse_headers((char *)ustr, len, &cur, maxlen, maxnhdrs, cb);
    if (rv != HWIRE_OK) {
        return rv;
    }
    ustr += cur;

    *pos = (size_t)(ustr - top);
    return HWIRE_OK;
}

/** @} */ /* end of HTTP Request Parsing Functions */

/**
 * @name HTTP Response Parsing Functions
 * @{
 */

/**
 * @brief Parse HTTP reason-phrase
 *
 * @param str String to parse (must not be NULL)
 * @param len Length of string
 * @param cur Output: bytes consumed (must not be NULL)
 * @param maxlen Input: max length, Output: reason phrase length
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data needed
 * @return HWIRE_EEOL for invalid end-of-line
 * @return HWIRE_EILSEQ for invalid byte sequence
 */
static int parse_reason_hwire(unsigned char *str, size_t len, size_t *cur,
                              size_t *maxlen)
{
    size_t pos   = 0;
    size_t limit = (len > *maxlen) ? *maxlen : len;

CHECK_NEXT: {
    size_t local_pos = 0;
    size_t n =
        hwire_parse_vchar((const char *)str + pos, limit - pos, &local_pos);
    pos += n;
}
    if (pos < limit) {
        unsigned char c = str[pos];
        switch (c) {
        case HT:
        case SP:
            // skip OWS
            pos++;
            while (pos < limit && (str[pos] == HT || str[pos] == SP)) {
                pos++;
            }
            goto CHECK_NEXT;

        case CR:
            if (!str[pos + 1]) {
                break;
            }
            if (str[pos + 1] != LF) {
                return HWIRE_EEOL;
            }
        case LF:
            *cur    = pos + 1 + (c == CR);
            *maxlen = pos;
            return HWIRE_OK;

        default:
            return HWIRE_EILSEQ;
        }
    }

    // phrase-length too large
    if (len > *maxlen) {
        return HWIRE_ELEN;
    }

    return HWIRE_EAGAIN;
}

/**
 * @brief Parse HTTP status code
 *
 * @param str String to parse (must not be NULL)
 * @param len Length of string
 * @param cur Output: bytes consumed (must not be NULL)
 * @param status Output: status code (must not be NULL)
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data needed
 * @return HWIRE_ESTATUS for invalid status code
 */
static int parse_status_hwire(const unsigned char *str, size_t len, size_t *cur,
                              uint16_t *status)
{
#define STATUS_LEN 3

    if (len <= STATUS_LEN) {
        return HWIRE_EAGAIN;
    } else if (str[STATUS_LEN] != SP) {
        return HWIRE_ESTATUS;
    }
    // HTTP status code: 3*DIGIT (100-599)
    else if (str[0] < '1' || str[0] > '5' || str[1] < '0' || str[1] > '9' ||
             str[2] < '0' || str[2] > '9') {
        return HWIRE_ESTATUS;
    }

    *cur    = STATUS_LEN + 1;
    *status = (str[0] - 0x30) * 100 + (str[1] - 0x30) * 10 + (str[2] - 0x30);
    return HWIRE_OK;

#undef STATUS_LEN
}

/**
 * @brief Parse HTTP response
 *
 * Parses status line and headers, calling response_cb after status line
 * and header_cb for each header.
 */
int hwire_parse_response(char *str, size_t len, size_t *pos, size_t maxlen,
                         uint8_t maxnhdrs, hwire_callbacks_t *cb)
{
    assert(str != NULL);
    assert(pos != NULL);
    assert(cb != NULL);
    assert(cb->response_cb != NULL);
    assert(cb->header_cb != NULL);
    unsigned char *ustr = (unsigned char *)str;
    unsigned char *top  = ustr;
    hwire_response_t rsp;
    size_t cur = 0;
    int rv     = 0;

SKIP_NEXT_CRLF:
    switch (*ustr) {
    // need more bytes
    case 0:
        return HWIRE_EAGAIN;

    case CR:
    case LF:
        ustr++;
        len--;
        goto SKIP_NEXT_CRLF;
    }

    // parse version
    rv = parse_version(ustr, len, &cur, &rsp.version);
    if (rv != HWIRE_OK) {
        return rv;
    } else if (!ustr[cur]) {
        return HWIRE_EAGAIN;
    } else if (ustr[cur] != SP) {
        return HWIRE_EVERSION;
    }
    ustr += cur + 1;
    len -= cur + 1;

    // parse status
    rv = parse_status_hwire(ustr, len, &cur, &rsp.status);
    if (rv != HWIRE_OK) {
        return rv;
    }
    ustr += cur;
    len -= cur;

    // parse reason
    rsp.reason.ptr = (const char *)ustr;
    rsp.reason.len = maxlen;
    rv             = parse_reason_hwire(ustr, len, &cur, &rsp.reason.len);
    if (rv != HWIRE_OK) {
        return rv;
    }
    ustr += cur;
    len -= cur;

    // call response callback
    if (cb->response_cb(cb, &rsp) != 0) {
        return HWIRE_ECALLBACK;
    }

    // parse headers
    cur = 0;
    rv  = hwire_parse_headers((char *)ustr, len, &cur, maxlen, maxnhdrs, cb);
    if (rv != HWIRE_OK) {
        return rv;
    }
    ustr += cur;

    *pos = (size_t)(ustr - top);
    return HWIRE_OK;
}

/** @} */ /* end of HTTP Response Parsing Functions */

// EOF
