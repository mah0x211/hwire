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

// ALIGNED(n): compiler-portable alignment specifier.
// Standard alignas/`_Alignas` is preferred when available (C++11 / C11).
// The #else branch also disables all SIMD paths by undefining the architecture
// macros before the intrinsic headers are included, since an unknown compiler
// is unlikely to support the required intrinsics.
#if defined(__cplusplus) && __cplusplus >= 201103L
# define ALIGNED(n) alignas(n)
#elif __STDC_VERSION__ >= 201112L
# define ALIGNED(n) _Alignas(n)
#elif defined(_MSC_VER)
# define ALIGNED(n) __declspec(align(n))
#elif defined(__GNUC__) || defined(__clang__)
# define ALIGNED(n) __attribute__((aligned(n)))
#else
# define ALIGNED(n)
// Defensive: undefine all SIMD arch macros so that the header block below
// falls through to #else and defines NO_SIMD automatically.
# undef __AVX2__
# undef __SSE4_2__
# undef __SSSE3__
# undef __SSE2__
# undef __aarch64__
# undef __ARM_NEON
#endif

// HWIRE_NO_SIMD: force the portable scalar implementation regardless of the
// target architecture (used by `make test-nosimd` and by callers that want to
// avoid SIMD). Undefine every architecture/feature macro the SIMD paths key
// off so the block below falls through to the scalar (#else -> NO_SIMD) branch.
// System headers were already included above, so clearing these compiler
// builtins here only affects hwire's own SIMD selection.
#if defined(HWIRE_NO_SIMD)
# undef __AVX2__
# undef __SSE4_2__
# undef __SSSE3__
# undef __SSE2__
# undef __aarch64__
# undef __ARM_NEON
# undef __arm__
#endif

// Normalize the x86 feature macros once, up front: MSVC under /arch:AVX2
// defines only __AVX2__ and none of the implied-level macros that GCC and
// clang define. With the levels filled in here, the rest of the file (and
// the include chain below) can key off the standard level macros alone.
// AVX2 builds take the SSE4.2 paths: a 256-bit scanner was found slower
// for typical header traffic than the PCMPESTRI paths.
#if defined(__AVX2__) && !defined(__SSE4_2__)
# define __SSE4_2__ 1
# define __SSSE3__  1
# define __SSE2__   1
#endif

// SIMD intrinsic headers.  Each x86 header transitively includes its
// prerequisites: SSE4.2 ⊃ SSSE3 ⊃ SSE2.
// NO_SIMD is defined when no known SIMD architecture is active, including
// after the defensive undef-s above for unknown compilers.
#if defined(__SSE4_2__)
# include <nmmintrin.h>
#elif defined(__SSSE3__)
# include <tmmintrin.h>
#elif defined(__SSE2__)
# include <emmintrin.h>
#elif defined(__aarch64__) || (defined(__arm__) && defined(__ARM_NEON))
# include <arm_neon.h>
#else
# define NO_SIMD
#endif

// ctz32/ctz64: count trailing zeros.
// Defined only when the SIMD arch that uses each function is active.
// <intrin.h> is guarded by NO_SIMD to avoid including it when SIMD is disabled.
#if defined(_MSC_VER) && !defined(NO_SIMD)
# include <intrin.h>
#endif

#if defined(__SSE4_2__) || defined(__SSSE3__) || defined(__SSE2__)
static inline int ctz32(unsigned int x)
{
# if defined(_MSC_VER)
    unsigned long i;
    _BitScanForward(&i, x);
    return (int)i;
# else
    return __builtin_ctz(x);
# endif
}
#endif

#if defined(__aarch64__) || (defined(__arm__) && defined(__ARM_NEON))
static inline int ctz64(unsigned long long x)
{
# if defined(_MSC_VER)
    unsigned long i;
    _BitScanForward64(&i, x);
    return (int)i;
# else
    return __builtin_ctzll(x);
# endif
}
#endif

// Sign-flip trick: (byte ^ 0x80) maps unsigned bytes to signed, enabling
// _mm_cmplt_epi8 to implement unsigned byte < threshold comparisons.
#if defined(__SSE2__)
# define SIMD_SIGN_FLIP ((int8_t)0x80)
#endif

#if defined(__GNUC__) || defined(__clang__)
# define likely(x)   __builtin_expect(!!(x), 1)
# define unlikely(x) __builtin_expect(!!(x), 0)
#else
# define likely(x)   (x)
# define unlikely(x) (x)
#endif

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
 * @brief Field-content allowed characters (RFC 7230)
 */
static const unsigned char FCVCHAR[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, // HT is allowed for field-vchar, but not for VCHAR
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    // VCHAR 0x20 - 0x7E
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
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
 * @brief URI allowed characters (RFC 3986)
 *
 * Bit 0 marks path characters, bit 1 marks query characters, bit 2 marks
 * reg-name characters, and bit 3 marks ordinary query-pair characters.
 * Percent is a structural stop in every component; question mark is a stop
 * only in paths. Literal '&', '=', and '+' stop query-pair scanning.
 *
 * unreserved / sub-delims / ":" / "@" / "/" / "?"
 * unreserved = ALPHA / DIGIT / "-" / "." / "_" / "~"
 * sub-delims = "!" / "$" / "&" / "'" / "(" / ")" / "*" / "+" / "," / ";" / "="
 */
enum {
    URI_PATH_CHAR       = 1U,
    URI_QUERY_CHAR      = 2U,
    URI_REGNAME_CHAR    = 4U,
    URI_QUERY_PAIR_CHAR = 8U
};

static const unsigned char URI_CHAR[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0-15
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 16-31
    0, 15, 0, 0, 15, 0, 7, 15, 15, 15, 15, 7, 15, 15, 15,
    11, // 32-47 ( ! # $ % & ' ( ) * + , - . / )
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 11, 15, 0, 7, 0,
    10, // 48-63 ( 0-9 : ; < = > ? )
    11, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, // 64-79 ( @ A-O )
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 0, 0, 0, 0,
    15, // 80-95 ( P-Z [ \ ] ^ _ )
    0, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, // 96-111 ( ` a-o )
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 0, 0, 0, 15,
    0, // 112-127 ( p-z { | } ~ DEL )
    // Extended ASCII (128-255) are NOT allowed in URI (must be unreserved)
    // Actually RFC 3986 says characters "not in the allowed set" must be
    // pct-encoded. So raw UTF-8 bytes > 127 are invalid in URI.
    0};

static inline size_t strurichar_cmp(const unsigned char *str, size_t len)
{
    size_t i                = 0;
    const unsigned int mask = URI_PATH_CHAR;

    // Process 8 bytes at a time using bitwise OR (branchless)
    while (i + 8 <= len) {
        int check =
            !(URI_CHAR[str[i + 0]] & mask) | !(URI_CHAR[str[i + 1]] & mask) |
            !(URI_CHAR[str[i + 2]] & mask) | !(URI_CHAR[str[i + 3]] & mask) |
            !(URI_CHAR[str[i + 4]] & mask) | !(URI_CHAR[str[i + 5]] & mask) |
            !(URI_CHAR[str[i + 6]] & mask) | !(URI_CHAR[str[i + 7]] & mask);

        if (check) {
            // Find exact position of invalid character
            for (size_t j = 0; j < 8; j++) {
                if (!(URI_CHAR[str[i + j]] & mask)) {
                    return i + j;
                }
            }
        }
        i += 8;
    }

    // Handle remaining bytes
    while (i < len) {
        if (!(URI_CHAR[str[i]] & mask)) {
            return i;
        }
        i++;
    }
    return i;
}

/**
 * @brief Count consecutive ordinary query-pair bytes (scalar reference)
 *
 * Examine at most len bytes. Stop before '%', '&', '+', '=', or a byte outside
 * the RFC 3986 query grammar; the caller handles delimiters and decoding.
 * Return len if every byte is ordinary.
 */
static inline size_t strqrychar_cmp(const unsigned char *str, size_t len)
{
    size_t i = 0;
    while (i < len) {
        unsigned char c = str[i];
        if (!(URI_CHAR[c] & URI_QUERY_PAIR_CHAR)) {
            break;
        }
        i++;
    }
    return i;
}

/**
 * @brief Count consecutive URI path characters (scalar reference)
 *
 * Returns the number of consecutive bytes from the beginning of str that
 * belong to the path whitelist. Percent and question mark
 * are returned to the structural parser so it can validate pct-encoded and
 * split the query without a second scan. SIMD implementations must return
 * identical stop positions.
 */

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

/** @} */ /* end of Internal Macros */

/**
 * @name Internal Character Validation Functions
 * @{
 */

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

static inline int is_fcchar(unsigned char c)
{
    return FCVCHAR[c] == 1;
}

// TCHAR_NIBBLE_LO / TCHAR_NIBBLE_HI: nibble-split lookup tables for tchar
// validation.
//
// For any byte c: (TCHAR_NIBBLE_LO[c & 0xF] & TCHAR_NIBBLE_HI[c >> 4]) != 0
// iff c is a valid tchar character (RFC 7230).
//
// Bit assignment in TCHAR_NIBBLE_HI: hi=2→bit0, hi=3→bit1, hi=4→bit2,
//   hi=5→bit3, hi=6→bit4, hi=7→bit5. hi=0,1,8-F map to 0 (all invalid).
#if defined(__SSSE3__)
static const int8_t ALIGNED(16) TCHAR_NIBBLE_LO[16] = {
    // lo:   0x0   0x1   0x2   0x3   0x4   0x5   0x6   0x7
    0x3A, 0x3F, 0x3E, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    // lo:   0x8   0x9   0xA   0xB   0xC   0xD   0xE   0xF
    0x3E, 0x3E, 0x3D, 0x15, 0x34, 0x15, 0x3D, 0x1C};
static const int8_t ALIGNED(16) TCHAR_NIBBLE_HI[16] = {
    // hi:   0x0   0x1   0x2   0x3   0x4   0x5   0x6   0x7
    0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20,
    // hi:   0x8   0x9   0xA   0xB   0xC   0xD   0xE   0xF
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif

/**
 * @brief Count consecutive tchar characters with lowercase conversion
 *
 * Counts the number of consecutive tchar (token) characters from the
 * beginning of str, writing the lowercase-converted characters into lc->buf.
 *
 * Uses the SSSE3 (16B/iter) nibble-trick when available.  For typical HTTP
 * header names (4-15 chars) the SIMD path completes in a single iteration.
 * A scalar 4-char-unrolled fallback handles any remaining bytes.  AVX2
 * builds also take the 128-bit path (see strtchar_cmp).
 *
 * @param str   String to parse (must not be NULL)
 * @param len   Maximum length of string
 * @param lc    Lowercase output buffer (must not be NULL)
 * @return Number of consecutive tchar characters written to lc->buf
 * @return SIZE_MAX if lc->buf is full and more tchars remain in str
 */
static inline size_t strtchar_cmp_lc(const unsigned char *str, size_t len,
                                     hwire_buf_t *lc)
{
    size_t pos         = 0;
    unsigned char *buf = (unsigned char *)lc->buf;
    size_t limit       = (len < lc->size) ? len : lc->size;

#if defined(__SSSE3__)
    if (pos + 16 <= limit) {
        const __m128i lo_lut =
            _mm_loadu_si128((const __m128i *)(const void *)TCHAR_NIBBLE_LO);
        const __m128i hi_lut =
            _mm_loadu_si128((const __m128i *)(const void *)TCHAR_NIBBLE_HI);
        const __m128i nibble  = _mm_set1_epi8(0x0F);
        const __m128i at_char = _mm_set1_epi8(0x40); // '@' (0x41-1)
        const __m128i bkt     = _mm_set1_epi8(0x5B); // '[' (0x5A+1)
        const __m128i bit5    = _mm_set1_epi8(0x20);
        do {
            __m128i data =
                _mm_loadu_si128((const __m128i *)(const void *)(str + pos));
            __m128i lo_v =
                _mm_shuffle_epi8(lo_lut, _mm_and_si128(data, nibble));
            __m128i hi_v = _mm_shuffle_epi8(
                hi_lut, _mm_and_si128(_mm_srli_epi16(data, 4), nibble));
            int mask = _mm_movemask_epi8(
                _mm_cmpeq_epi8(_mm_and_si128(lo_v, hi_v), _mm_setzero_si128()));
            // lowercase: A-Z (0x41-0x5A) -> set bit5
            __m128i is_upper =
                _mm_and_si128(_mm_cmpgt_epi8(data, at_char), // c > '@'
                              _mm_cmpgt_epi8(bkt, data));    // '[' > c
            __m128i lc_out = _mm_or_si128(data, _mm_and_si128(is_upper, bit5));
            // store 16 bytes (safe: pos+16 <= limit <= lc->size)
            _mm_storeu_si128((__m128i *)(void *)(buf + pos), lc_out);
            if (mask) {
                pos += (size_t)ctz32((unsigned)mask);
                lc->len = pos;
                return pos;
            }
            pos += 16;
        } while (pos + 16 <= limit);
    }
#endif

    // scalar fallback for remaining bytes (< 16/32) or non-SIMD builds
    while (pos + 4 <= limit) {
        unsigned char c0 = str[pos];
        unsigned char c1 = str[pos + 1];
        unsigned char c2 = str[pos + 2];
        unsigned char c3 = str[pos + 3];
        if (likely(is_tchar(c0) & is_tchar(c1) & is_tchar(c2) & is_tchar(c3))) {
            buf[pos]     = TCHAR[c0];
            buf[pos + 1] = TCHAR[c1];
            buf[pos + 2] = TCHAR[c2];
            buf[pos + 3] = TCHAR[c3];
            pos += 4;
            continue;
        }
        break;
    }
    while (pos < limit && is_tchar(str[pos])) {
        buf[pos] = TCHAR[str[pos]];
        pos++;
    }
    lc->len = pos;

    // buffer is full - check if there are more tchars
    if (pos < len && is_tchar(str[pos])) {
        return SIZE_MAX;
    }
    return pos;
}

/**
 * @brief Count consecutive tchar characters (no lowercase conversion)
 *
 * Uses the SSSE3 (16B/iter) nibble-trick when available, with a scalar
 * fallback for tail bytes. For typical HTTP header names (6-15 chars) the
 * SIMD path completes in a single iteration. AVX2 builds also take this
 * 128-bit path: the 256-bit variant's per-call register construction
 * (vinserti128/broadcast) costs more than it saves for name-sized scans.
 *
 * @param str String to parse (must not be NULL)
 * @param len Maximum length of string
 * @return Index of first non-tchar byte (0 if first char is not tchar)
 */
static inline size_t strtchar_cmp(const unsigned char *str, size_t len)
{
    size_t pos = 0;

#if defined(__SSSE3__)
    if (pos + 16 <= len) {
        const __m128i lo_lut =
            _mm_loadu_si128((const __m128i *)(const void *)TCHAR_NIBBLE_LO);
        const __m128i hi_lut =
            _mm_loadu_si128((const __m128i *)(const void *)TCHAR_NIBBLE_HI);
        const __m128i nibble = _mm_set1_epi8(0x0F);
        do {
            __m128i data =
                _mm_loadu_si128((const __m128i *)(const void *)(str + pos));
            __m128i lo_v =
                _mm_shuffle_epi8(lo_lut, _mm_and_si128(data, nibble));
            __m128i hi_v = _mm_shuffle_epi8(
                hi_lut, _mm_and_si128(_mm_srli_epi16(data, 4), nibble));
            int mask = _mm_movemask_epi8(
                _mm_cmpeq_epi8(_mm_and_si128(lo_v, hi_v), _mm_setzero_si128()));
            if (mask)
                return pos + (size_t)ctz32((unsigned)mask);
            pos += 16;
        } while (pos + 16 <= len);
    }
#endif

    while (pos < len && TCHAR[str[pos]]) {
        pos++;
    }
    return pos;
}

/**
 * @brief Count consecutive tchar characters with optional lowercase conversion
 *
 * Counts the number of consecutive tchar (token) characters from the
 * beginning of str. Uses 8x loop unrolling for performance.
 *
 * If lc is non-NULL, stores the lowercase-converted tchar characters into
 * lc->buf starting at lc->len.
 *
 * @param str String to parse (must not be NULL)
 * @param len Maximum length of string
 * @param lc  Optional lowercase buffer (NULL to skip lowercase conversion)
 * @return Number of consecutive tchar characters (0 if first char is not tchar)
 * @return SIZE_MAX if buffer is full and there are more tchars to process
 */
static inline size_t strtchar(const unsigned char *str, size_t len,
                              hwire_buf_t *lc)
{
    if (lc) {
        return strtchar_cmp_lc(str, len, lc);
    }

    return strtchar_cmp(str, len);
}

// strvchar_cmp: Compare characters in str against VCHAR set, with optional
// field-vchar support. Returns the number of valid characters from the start of
// str.
// is_field_vchar: 1 to allow field-vchar, 0 otherwise
// endc: set to the first invalid byte (non-NULL assumed)
static inline size_t strvchar_cmp(const unsigned char *str, size_t len,
                                  int is_field_vchar, unsigned char *endc)
{
    size_t pos                  = 0;
    // Select lookup table based on is_field_vchar: FCVCHAR for field-vchar,
    // else VCHAR
    const unsigned char *lookup = is_field_vchar ? FCVCHAR : VCHAR;

    // Process 8 bytes at a time using bitwise OR (branchless)
    while (pos + 8 <= len) {
        int check = !lookup[str[pos + 0]] | !lookup[str[pos + 1]] |
                    !lookup[str[pos + 2]] | !lookup[str[pos + 3]] |
                    !lookup[str[pos + 4]] | !lookup[str[pos + 5]] |
                    !lookup[str[pos + 6]] | !lookup[str[pos + 7]];

        if (check) {
            // Find exact position of invalid character
            for (size_t i = 0; i < 8; i++) {
                if (!lookup[str[pos + i]]) {
                    *endc = str[pos + i];
                    return pos + i;
                }
            }
        }
        pos += 8;
    }

    // Handle remaining bytes (< 8)
    while (pos < len && lookup[str[pos]]) {
        pos++;
    }
    if (pos < len) {
        *endc = str[pos];
    }
    return pos;
}

#if defined(__aarch64__) || (defined(__arm__) && defined(__ARM_NEON))

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
// strvchar_neon: NEON-optimized implementation (16 bytes)
// endc: set to the first invalid byte (non-NULL assumed)
static inline size_t strvchar_neon(const unsigned char *str, size_t len,
                                   int is_field_vchar, unsigned char *endc)
{
    size_t pos                 = 0;
    // Pre-compute constants (compile-time inlined)
    // 0x20 for field-vchar, else 0x21 (exclamation mark) for VCHAR
    const uint8x16_t first_cmp = vdupq_n_u8(is_field_vchar ? 0x20 : 0x21);
    const uint8x16_t del_char  = vdupq_n_u8(0x7F);
    const uint8x16_t ht_char   = vdupq_n_u8(0x09);

    while (pos + 16 <= len) {
        // Load 16 bytes
        uint8x16_t data = vld1q_u8(str + pos);

        // Check1: is_before_first: bytes where data < firstc
        uint8x16_t is_before_first = vcltq_u8(data, first_cmp);

        // Check2: SP/HT exception logic - if is_field_vchar, then SP/HT
        // (0x09) is valid even if < firstc
        if (is_field_vchar) {
            // If is_field_vchar is true, we will mark SP/HT bytes as valid
            // (0x00) in the is_before_first mask. We do this by creating
            // a mask of SP/HT bytes and then using vbicq_u8 to clear those bits
            // in is_before_first.
            uint8x16_t is_spht = vceqq_u8(data, ht_char);
            is_before_first    = vbicq_u8(is_before_first, is_spht);
        }

        // Check3: is_del: bytes where data == 0x7F (DEL)
        uint8x16_t is_del = vceqq_u8(data, del_char);

        // Combine: invalid if is_before_first OR is_del
        uint8x16_t is_invalid = vorrq_u8(is_before_first, is_del);

        // Interpret as 64-bit integers
        uint64x2_t qdata = vreinterpretq_u64_u8(is_invalid);

        uint64_t mask1 = vgetq_lane_u64(qdata, 0);
        if (mask1) {
            size_t first = (size_t)(ctz64(mask1) >> 3);
            *endc        = str[pos + first]; // L1 hit: data was just loaded
            return pos + first;
        }

        uint64_t mask2 = vgetq_lane_u64(qdata, 1);
        if (mask2) {
            size_t first = 8 + (size_t)(ctz64(mask2) >> 3);
            *endc        = str[pos + first]; // L1 hit: data was just loaded
            return pos + first;
        }

        pos += 16;
    }

    // Fall back to LUT for remaining bytes (< 16 bytes)
    return pos + strvchar_cmp(str + pos, len - pos, is_field_vchar, endc);
}

// strurichar_neon: NEON optimized implementation (16 bytes)
//
// Algorithm: Whitelist approach mirroring URI_CHAR, with percent returned for
// pct-encoded validation and question mark returned as a path delimiter.
// Each result lane is 0xFF (invalid) or 0x00 (valid); the first invalid
// byte is located via the same two-lane ctz64 extraction as strvchar_neon.
static inline size_t strurichar_neon(const unsigned char *str, size_t len)
{
    size_t pos                = 0;
    const uint8x16_t first_ok = vdupq_n_u8(0x21);
    const uint8x16_t last_ok  = vdupq_n_u8(0x7E);

    while (pos + 16 <= len) {
        uint8x16_t data = vld1q_u8(str + pos);

        // outside the printable whitelist bounds
        uint8x16_t is_out =
            vorrq_u8(vcltq_u8(data, first_ok), vcgtq_u8(data, last_ok));

        // excluded bytes within the bounds
        uint8x16_t in_2223 = vandq_u8(vcgeq_u8(data, vdupq_n_u8(0x22)),
                                      vcleq_u8(data, vdupq_n_u8(0x23)));
        uint8x16_t in_5b5e = vandq_u8(vcgeq_u8(data, vdupq_n_u8(0x5B)),
                                      vcleq_u8(data, vdupq_n_u8(0x5E)));
        uint8x16_t in_7b7d = vandq_u8(vcgeq_u8(data, vdupq_n_u8(0x7B)),
                                      vcleq_u8(data, vdupq_n_u8(0x7D)));
        uint8x16_t is_excl = vorrq_u8(
            vorrq_u8(in_2223, in_5b5e),
            vorrq_u8(in_7b7d,
                     vorrq_u8(vorrq_u8(vceqq_u8(data, vdupq_n_u8(0x3C)),
                                       vceqq_u8(data, vdupq_n_u8(0x3E))),
                              vceqq_u8(data, vdupq_n_u8(0x60)))));

        uint8x16_t is_pct   = vceqq_u8(data, vdupq_n_u8('%'));
        uint8x16_t is_qmark = vceqq_u8(data, vdupq_n_u8('?'));
        uint8x16_t is_invalid =
            vorrq_u8(vorrq_u8(is_out, is_excl), vorrq_u8(is_pct, is_qmark));

        uint64x2_t qdata = vreinterpretq_u64_u8(is_invalid);
        uint64_t mask1   = vgetq_lane_u64(qdata, 0);
        if (mask1) {
            return pos + (size_t)(ctz64(mask1) >> 3);
        }
        uint64_t mask2 = vgetq_lane_u64(qdata, 1);
        if (mask2) {
            return pos + 8 + (size_t)(ctz64(mask2) >> 3);
        }
        pos += 16;
    }

    // Fall back for remaining bytes (< 16 bytes)
    return pos + strurichar_cmp(str + pos, len - pos);
}

/**
 * @brief Count ordinary query-pair bytes using NEON
 *
 * Read only complete 16-byte blocks within len, then use strqrychar_cmp for
 * the remainder. Return the same first-stop offset as the scalar reference.
 */
static inline size_t strqrychar_neon(const unsigned char *str, size_t len)
{
    size_t pos                = 0;
    const uint8x16_t first_ok = vdupq_n_u8(0x21);
    const uint8x16_t last_ok  = vdupq_n_u8(0x7E);

    while (pos + 16 <= len) {
        uint8x16_t data = vld1q_u8(str + pos);
        uint8x16_t is_out =
            vorrq_u8(vcltq_u8(data, first_ok), vcgtq_u8(data, last_ok));
        uint8x16_t in_2223 = vandq_u8(vcgeq_u8(data, vdupq_n_u8(0x22)),
                                      vcleq_u8(data, vdupq_n_u8(0x23)));
        uint8x16_t in_5b5e = vandq_u8(vcgeq_u8(data, vdupq_n_u8(0x5B)),
                                      vcleq_u8(data, vdupq_n_u8(0x5E)));
        uint8x16_t in_7b7d = vandq_u8(vcgeq_u8(data, vdupq_n_u8(0x7B)),
                                      vcleq_u8(data, vdupq_n_u8(0x7D)));
        uint8x16_t is_excl = vorrq_u8(
            vorrq_u8(in_2223, in_5b5e),
            vorrq_u8(in_7b7d,
                     vorrq_u8(vorrq_u8(vceqq_u8(data, vdupq_n_u8(0x3C)),
                                       vceqq_u8(data, vdupq_n_u8(0x3E))),
                              vceqq_u8(data, vdupq_n_u8(0x60)))));
        uint8x16_t is_structural =
            vorrq_u8(vorrq_u8(vceqq_u8(data, vdupq_n_u8('%')),
                              vceqq_u8(data, vdupq_n_u8('&'))),
                     vorrq_u8(vceqq_u8(data, vdupq_n_u8('+')),
                              vceqq_u8(data, vdupq_n_u8('='))));
        uint8x16_t is_invalid =
            vorrq_u8(vorrq_u8(is_out, is_excl), is_structural);

        uint64x2_t qdata = vreinterpretq_u64_u8(is_invalid);
        uint64_t mask1   = vgetq_lane_u64(qdata, 0);
        if (mask1) {
            return pos + (size_t)(ctz64(mask1) >> 3);
        }
        uint64_t mask2 = vgetq_lane_u64(qdata, 1);
        if (mask2) {
            return pos + 8 + (size_t)(ctz64(mask2) >> 3);
        }
        pos += 16;
    }

    return pos + strqrychar_cmp(str + pos, len - pos);
}

#endif

#if defined(__SSE2__)

// The SSE2 16-byte scanners below (strvchar_sse2, in_range_sse2, and
// strurichar_sse2) are referenced only when the PCMPESTRI path is
// unavailable: the dispatchers prefer the SSE4.2 implementation whenever
// __SSE4_2__ is defined. Guard them out otherwise to avoid
// -Wunused-function under -Werror.
//
// strvchar_sse2 algorithm: Blacklist approach using comparison and movemask
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
//
// Parameters:
// - is_field_vchar: non-zero if field-vchar is allowed, 0x00 otherwise
// - endc: set to the first invalid byte (non-NULL assumed)
# if !defined(__SSE4_2__)
static inline size_t strvchar_sse2(const unsigned char *str, size_t len,
                                   int8_t is_field_vchar, unsigned char *endc)
{
    size_t pos              = 0;
    // Pre-compute constants (compile-time)
    const __m128i sign_flip = _mm_set1_epi8(SIMD_SIGN_FLIP);
    // 0x20 (space) for field-vchar, else 0x21 (exclamation mark) for VCHAR
    const __m128i first_cmp =
        _mm_set1_epi8((is_field_vchar ? 0x20 : 0x21) ^ SIMD_SIGN_FLIP);
    const __m128i del_char = _mm_set1_epi8(0x7F);
    const __m128i ht_char  = _mm_set1_epi8(0x09);
    const __m128i allow_ht = _mm_set1_epi8(-(is_field_vchar != 0));

    while (pos + 16 <= len) {
        // Load 16 bytes (unaligned load)
        __m128i data =
            _mm_loadu_si128((const __m128i *)(const void *)(str + pos));

        // Toggle sign bit to enable unsigned comparison with signed
        // instructions This transforms the unsigned comparison (data < 0x21)
        // into a signed one
        __m128i data_shifted = _mm_xor_si128(data, sign_flip);

        // Check1: Detect bytes where (data ^ 0x80) < (firstc ^ 0x80) which is
        // equivalent
        __m128i is_before_first = _mm_cmpgt_epi8(first_cmp, data_shifted);

        // Check2: SP/HT exception logic
        // if is_field_vchar is true, then HT (0x09) is valid even if < firstc.
        __m128i is_ht         = _mm_cmpeq_epi8(data, ht_char);
        __m128i is_allowed_ht = _mm_and_si128(is_ht, allow_ht);
        is_before_first = _mm_andnot_si128(is_allowed_ht, is_before_first);

        // Check3: DEL (0x7F) is always invalid
        __m128i is_del = _mm_cmpeq_epi8(data, del_char);

        // Combine: invalid if is_before_first OR is_del
        __m128i is_invalid = _mm_or_si128(is_before_first, is_del);

        // Create 16-bit mask: bit i is 1 if byte i is invalid
        int mask = _mm_movemask_epi8(is_invalid);
        if (mask) {
            int first = ctz32((unsigned int)mask);
            // L1 hit: data was just loaded from str+pos
            *endc     = str[pos + (size_t)first];
            return pos + (size_t)first;
        }

        // All 16 bytes are valid, continue to next chunk
        pos += 16;
    }

    return pos + strvchar_cmp(str + pos, len - pos, is_field_vchar, endc);
}

// in_range_sse2: lanes set to 0xFF where lo <= data <= hi (unsigned),
// computed on sign-flipped data with signed compares. `lo` and `hi` are
// raw (unflipped) byte values; both are flipped here.
static inline __m128i in_range_sse2(__m128i data_shifted, int lo, int hi)
{
    const __m128i below = _mm_cmpgt_epi8(
        _mm_set1_epi8((char)(lo ^ SIMD_SIGN_FLIP)), data_shifted);
    const __m128i above = _mm_cmpgt_epi8(
        data_shifted, _mm_set1_epi8((char)(hi ^ SIMD_SIGN_FLIP)));
    return _mm_cmpeq_epi8(_mm_or_si128(below, above), _mm_setzero_si128());
}

// strurichar_sse2: SSE2 optimized implementation (16 bytes)
//
// Algorithm: Whitelist approach mirroring URI_CHAR, with percent returned for
// pct-encoded validation and question mark returned as a path delimiter.
// Unsigned range tests use the sign-flip (XOR 0x80) technique documented in
// strvchar_sse2. The first invalid byte is located via movemask + ctz32.
static inline size_t strurichar_sse2(const unsigned char *str, size_t len)
{
    size_t pos              = 0;
    const __m128i sign_flip = _mm_set1_epi8(SIMD_SIGN_FLIP);
    const __m128i first_cmp = _mm_set1_epi8(0x21 ^ SIMD_SIGN_FLIP);
    const __m128i last_cmp  = _mm_set1_epi8(0x7E ^ SIMD_SIGN_FLIP);

    while (pos + 16 <= len) {
        __m128i data =
            _mm_loadu_si128((const __m128i *)(const void *)(str + pos));
        __m128i data_shifted = _mm_xor_si128(data, sign_flip);

        // outside the printable whitelist bounds (unsigned via sign flip)
        __m128i is_before = _mm_cmpgt_epi8(first_cmp, data_shifted);
        __m128i is_after  = _mm_cmpgt_epi8(data_shifted, last_cmp);

        // excluded bytes within the bounds
        __m128i eq_3c = _mm_cmpeq_epi8(data, _mm_set1_epi8(0x3C));
        __m128i eq_3e = _mm_cmpeq_epi8(data, _mm_set1_epi8(0x3E));
        __m128i eq_60 = _mm_cmpeq_epi8(data, _mm_set1_epi8(0x60));

        __m128i is_invalid = _mm_or_si128(
            _mm_or_si128(is_before, is_after),
            _mm_or_si128(
                _mm_or_si128(eq_3c, _mm_or_si128(eq_3e, eq_60)),
                _mm_or_si128(
                    in_range_sse2(data_shifted, 0x22, 0x23),
                    _mm_or_si128(in_range_sse2(data_shifted, 0x5B, 0x5E),
                                 in_range_sse2(data_shifted, 0x7B, 0x7D)))));
        __m128i is_pct   = _mm_cmpeq_epi8(data, _mm_set1_epi8('%'));
        __m128i is_qmark = _mm_cmpeq_epi8(data, _mm_set1_epi8('?'));
        is_invalid = _mm_or_si128(is_invalid, _mm_or_si128(is_pct, is_qmark));

        int mask = _mm_movemask_epi8(is_invalid);
        if (mask) {
            return pos + (size_t)ctz32((unsigned int)mask);
        }
        pos += 16;
    }

    return pos + strurichar_cmp(str + pos, len - pos);
}

/**
 * @brief Count ordinary query-pair bytes using SSE2
 *
 * Read only complete 16-byte blocks within len, then use strqrychar_cmp for
 * the remainder. Return the same first-stop offset as the scalar reference.
 */
static inline size_t strqrychar_sse2(const unsigned char *str, size_t len)
{
    size_t pos              = 0;
    const __m128i sign_flip = _mm_set1_epi8(SIMD_SIGN_FLIP);
    const __m128i first_cmp = _mm_set1_epi8(0x21 ^ SIMD_SIGN_FLIP);
    const __m128i last_cmp  = _mm_set1_epi8(0x7E ^ SIMD_SIGN_FLIP);

    while (pos + 16 <= len) {
        __m128i data =
            _mm_loadu_si128((const __m128i *)(const void *)(str + pos));
        __m128i data_shifted = _mm_xor_si128(data, sign_flip);
        __m128i is_before    = _mm_cmpgt_epi8(first_cmp, data_shifted);
        __m128i is_after     = _mm_cmpgt_epi8(data_shifted, last_cmp);
        __m128i is_excl      = _mm_or_si128(
            _mm_or_si128(_mm_cmpeq_epi8(data, _mm_set1_epi8(0x3C)),
                         _mm_cmpeq_epi8(data, _mm_set1_epi8(0x3E))),
            _mm_or_si128(
                _mm_cmpeq_epi8(data, _mm_set1_epi8(0x60)),
                _mm_or_si128(
                    in_range_sse2(data_shifted, 0x22, 0x23),
                    _mm_or_si128(in_range_sse2(data_shifted, 0x5B, 0x5E),
                                 in_range_sse2(data_shifted, 0x7B, 0x7D)))));
        __m128i is_structural = _mm_or_si128(
            _mm_or_si128(_mm_cmpeq_epi8(data, _mm_set1_epi8('%')),
                         _mm_cmpeq_epi8(data, _mm_set1_epi8('&'))),
            _mm_or_si128(_mm_cmpeq_epi8(data, _mm_set1_epi8('+')),
                         _mm_cmpeq_epi8(data, _mm_set1_epi8('='))));
        __m128i is_invalid = _mm_or_si128(_mm_or_si128(is_before, is_after),
                                          _mm_or_si128(is_excl, is_structural));

        int mask = _mm_movemask_epi8(is_invalid);
        if (mask) {
            return pos + (size_t)ctz32((unsigned int)mask);
        }
        pos += 16;
    }

    return pos + strqrychar_cmp(str + pos, len - pos);
}

# endif /* !defined(__SSE4_2__) */

#endif /* defined(__SSE2__) */

#if defined(__SSE4_2__)

// strvchar_sse42: SSE4.2 optimized implementation using PCMPESTRI
//
// Algorithm: Blacklist approach using PCMPESTRI range matching
// - Invalid ranges for VCHAR: 0x00-0x20, 0x7F-0x7F
// - Invalid ranges for field-vchar: 0x00-0x08, 0x0A-0x1F, 0x7F-0x7F (excludes
// HT)
// - Valid: 0x21-0x7E (VCHAR) or 0x80-0xFF (obs-text)
//
// PCMPESTRI checks up to 8 ranges (16 bytes) against 16 bytes of data.
// When a match is found (invalid char), switch to slow loop.
static inline size_t strvchar_sse42(const unsigned char *str, size_t len,
                                    int is_field_vchar, unsigned char *endc)
{
    size_t pos = 0;
    // Invalid character ranges (padded to 16 bytes to match _mm_loadu_si128):
    // - VCHAR:       0x00-0x20 (control + SP), 0x7F-0x7F (DEL) - 2 ranges
    // - field-vchar: 0x00-0x08, 0x0A-0x1F, 0x7F-0x7F - 3 ranges (excludes HT)
    static const char ALIGNED(16) VCHAR_INVALID_RANGES[16] =
        "\x00\x20\x7f\x7f"; // 2 ranges = 4 bytes; remaining 12 bytes are \0
    static const char ALIGNED(16) FCVCHAR_INVALID_RANGES[16] =
        "\x00\x08\x0a\x1f\x7f\x7f"; // 3 ranges = 6 bytes; remaining 10 bytes
                                    // are \0

    const __m128i ranges = _mm_loadu_si128((
        const __m128i *)(const void *)(is_field_vchar ? FCVCHAR_INVALID_RANGES :
                                                        VCHAR_INVALID_RANGES));
    const int ranges_len = is_field_vchar ? 6 : 4;

    while (pos + 16 <= len) {
        __m128i data =
            _mm_loadu_si128((const __m128i *)(const void *)(str + pos));

        // PCMPESTRI: find first byte in data that falls within any of the
        // invalid ranges. Returns index of first match (0-15), or 16 if no
        // match.
        int idx = _mm_cmpestri(ranges, ranges_len, data, 16,
                               _SIDD_LEAST_SIGNIFICANT | _SIDD_CMP_RANGES |
                                   _SIDD_UBYTE_OPS);

        if (idx != 16) {
            // Use PSHUFB to bring data[idx] to lane 0 — no memory address
            // dependency on 'idx' (unlike str[pos+idx] which needs
            // idx→addr→load)
            *endc = (unsigned char)_mm_cvtsi128_si32(
                _mm_shuffle_epi8(data, _mm_set1_epi8((int8_t)idx)));
            return pos + (size_t)idx;
        }

        pos += 16;
    }

    return pos + strvchar_cmp(str + pos, len - pos, is_field_vchar, endc);
}

// strurichar_sse42: SSE4.2 optimized implementation using PCMPESTRI
//
// Algorithm: Path and query whitelists each decompose into eight ranges.
// Percent is excluded so the structural parser can validate its two hex
// digits; question mark is excluded from the path ranges.
static inline size_t strurichar_sse42(const unsigned char *str, size_t len)
{
    size_t pos = 0;
    static const char ALIGNED(16) PATH_RANGES[16] =
        "\x21\x21\x24\x24\x26\x3b\x3d\x3d"
        "\x40\x5a\x5f\x5f\x61\x7a\x7e\x7e";
    const __m128i ranges =
        _mm_loadu_si128((const __m128i *)(const void *)PATH_RANGES);

    while (pos + 16 <= len) {
        __m128i data =
            _mm_loadu_si128((const __m128i *)(const void *)(str + pos));

        int idx =
            _mm_cmpestri(ranges, 16, data, 16,
                         _SIDD_LEAST_SIGNIFICANT | _SIDD_CMP_RANGES |
                             _SIDD_UBYTE_OPS | _SIDD_MASKED_NEGATIVE_POLARITY);
        if (idx != 16) {
            return pos + (size_t)idx;
        }
        pos += 16;
    }

    return pos + strurichar_cmp(str + pos, len - pos);
}

/**
 * @brief Count ordinary query-pair bytes using SSE4.2 range comparisons
 *
 * Read only complete 16-byte blocks within len, then use strqrychar_cmp for
 * the remainder. Return the same first-stop offset as the scalar reference.
 */
static inline size_t strqrychar_sse42(const unsigned char *str, size_t len)
{
    size_t pos                                                   = 0;
    // RFC 3986 query characters, excluding %, &, +, and = for the caller.
    static const unsigned char ALIGNED(16) QUERY_PAIR_RANGES[16] = {
        0x21, 0x21, 0x24, 0x24, 0x27, 0x2A, 0x2C, 0x3B,
        0x3F, 0x5A, 0x5F, 0x5F, 0x61, 0x7A, 0x7E, 0x7E};
    const __m128i ranges =
        _mm_loadu_si128((const __m128i *)(const void *)QUERY_PAIR_RANGES);

    while (pos + 16 <= len) {
        __m128i data =
            _mm_loadu_si128((const __m128i *)(const void *)(str + pos));
        int idx =
            _mm_cmpestri(ranges, 16, data, 16,
                         _SIDD_LEAST_SIGNIFICANT | _SIDD_CMP_RANGES |
                             _SIDD_UBYTE_OPS | _SIDD_MASKED_NEGATIVE_POLARITY);
        if (idx != 16) {
            return pos + (size_t)idx;
        }
        pos += 16;
    }

    return pos + strqrychar_cmp(str + pos, len - pos);
}

#endif

// strvchar: count consecutive field-content characters (VCHAR or obs-text)
// Returns the number of consecutive characters from the beginning of str
// that are field-content (VCHAR or obs-text)

// strfcchar: count consecutive field-content characters (VCHAR, obs-text,
// SP, HT) Returns the number of consecutive characters from the beginning
// of str that are field-content (VCHAR, obs-text, SP, HT)

static inline size_t strvchar(const unsigned char *str, size_t len)
{
    unsigned char endc = 0; // discarded; compiler optimizes away
#if defined(__SSE4_2__)
    if (likely(len >= 16)) {
        return strvchar_sse42(str, len, 0, &endc);
    }
#elif defined(__SSE2__)
    if (likely(len >= 16)) {
        return strvchar_sse2(str, len, 0, &endc);
    }
#elif defined(__aarch64__) || (defined(__arm__) && defined(__ARM_NEON))
    if (likely(len >= 16)) {
        return strvchar_neon(str, len, 0, &endc);
    }
#endif
    return strvchar_cmp(str, len, 0, &endc);
}

// strurichar: count consecutive RFC 3986 path characters, stopping at
// percent and question mark for the structural parser.
static inline size_t strurichar(const unsigned char *str, size_t len)
{
#if defined(__SSE4_2__)
    if (likely(len >= 16)) {
        return strurichar_sse42(str, len);
    }
#elif defined(__SSE2__)
    if (likely(len >= 16)) {
        return strurichar_sse2(str, len);
    }
#elif defined(__aarch64__) || (defined(__arm__) && defined(__ARM_NEON))
    if (likely(len >= 16)) {
        return strurichar_neon(str, len);
    }
#endif
    return strurichar_cmp(str, len);
}

/**
 * @brief Count consecutive ordinary query-pair bytes within len
 *
 * Select the available SIMD scanner for at least 16 bytes, or use the scalar
 * reference. Return the offset of the first delimiter, escape, plus sign, or
 * invalid byte; return len when none occurs.
 */
static inline size_t strqrychar(const unsigned char *str, size_t len)
{
#if defined(__SSE4_2__)
    if (likely(len >= 16)) {
        return strqrychar_sse42(str, len);
    }
#elif defined(__SSE2__)
    if (likely(len >= 16)) {
        return strqrychar_sse2(str, len);
    }
#elif defined(__aarch64__) || (defined(__arm__) && defined(__ARM_NEON))
    if (likely(len >= 16)) {
        return strqrychar_neon(str, len);
    }
#endif
    return strqrychar_cmp(str, len);
}

static inline size_t strfcchar(const unsigned char *str, size_t len,
                               unsigned char *endc)
{
#if defined(__SSE4_2__)
    if (likely(len >= 16)) {
        return strvchar_sse42(str, len, 1, endc);
    }
#elif defined(__SSE2__)
    if (likely(len >= 16)) {
        return strvchar_sse2(str, len, 1, endc);
    }
#elif defined(__aarch64__) || (defined(__arm__) && defined(__ARM_NEON))
    if (likely(len >= 16)) {
        return strvchar_neon(str, len, 1, endc);
    }
#endif

    return strvchar_cmp(str, len, 1, endc);
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

int hwire_is_fcchar(unsigned char c)
{
    return is_fcchar(c);
}

/**
 * @brief Count consecutive tchar characters
 *
 * Counts the number of consecutive tchar (token) characters from the
 * beginning of str, starting at offset *pos. Updates *pos to the position
 * after the matched characters.
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @return Number of consecutive tchar characters matched (0 if first char is
 * not tchar)
 * @see RFC 7230 Section 3.2.6 Field Value Components
 * @see RFC 9110 Section 5.6.2 Tokens
 */
size_t hwire_parse_tchar(const char *str, size_t len, size_t *pos)
{
    assert(str != NULL);
    assert(pos != NULL);
    size_t cur = *pos;

    if (cur < len) {
        const unsigned char *ustr = (const unsigned char *)str + cur;
        size_t n                  = strtchar(ustr, len - cur, NULL);
        *pos += n;
        return n;
    }
    return 0;
}

/**
 * @brief Count consecutive vchar characters
 *
 * Counts the number of consecutive vchar (field-content) characters from the
 * beginning of str, starting at offset *pos. Updates *pos to the position
 * after the matched characters.
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @return Number of consecutive vchar characters matched (0 if first char is
 * not vchar)
 * @see RFC 7230 Section 3.2.6 Field Value Components
 * @see RFC 9110 Section 5.5 Field Values
 */
size_t hwire_parse_vchar(const char *str, size_t len, size_t *pos)
{
    assert(str != NULL);
    assert(pos != NULL);
    size_t cur = *pos;

    if (cur < len) {
        const unsigned char *ustr = (const unsigned char *)str + cur;
        size_t n                  = strvchar(ustr, len - cur);
        *pos += n;
        return n;
    }
    return 0;
}

/**
 * @brief Count consecutive fcchar characters
 *
 * Counts the number of consecutive field-content characters (VCHAR, obs-text,
 * SP, HTAB) from str, starting at offset *pos. Updates *pos to the position
 * after the matched characters.
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @return Number of consecutive fcchar characters matched (0 if first char is
 * not fcchar)
 * @see RFC 9110 Section 5.5 Field Values
 */
size_t hwire_parse_fcchar(const char *str, size_t len, size_t *pos)
{
    assert(str != NULL);
    assert(pos != NULL);
    size_t cur = *pos;

    if (cur < len) {
        const unsigned char *ustr = (const unsigned char *)str + cur;
        unsigned char endc        = 0; /* discarded; caller uses str[*pos] */
        size_t n                  = strfcchar(ustr, len - cur, &endc);
        *pos += n;
        return n;
    }
    return 0;
}

/**
 * @see RFC 7230 Section 3.2.6 Field Value Components
 * @see RFC 9110 Section 5.6.4 Quoted Strings
 */
static int parse_quoted_string(const unsigned char **ustr,
                               const unsigned char *head,
                               const unsigned char *tail, size_t maxlen)
{
    assert(ustr != NULL && *ustr != NULL);
    const unsigned char *str = *ustr;

    if (str >= tail) {
        // classify the boundary only after reaching the parse tail
        return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN : HWIRE_EAGAIN;
    } else if (*str++ != DQUOTE) {
        // not starting with DQUOTE
        *ustr = str - 1;
        return HWIRE_EILSEQ;
    }

    // parse quoted-string
    // RFC 9110 5.6.4: quoted-string = DQUOTE *( qdtext / quoted-pair ) DQUOTE
    // qdtext = HTAB / SP / %x21 / %x23-5B / %x5D-7E / obs-text
    // obs-text = %x80-FF
    while (str < tail) {
        unsigned char c = *str++;
        if (!QDTEXT[c]) {
            switch (c) {
            case DQUOTE:
                // Found closing quote
                *ustr = str;
                return HWIRE_OK;

            case BACKSLASH:
                // quoted-pair = "\" ( HTAB / SP / VCHAR / obs-text )
                if (str >= tail) {
                    str--;
                    goto TAIL_REACHED;
                }
                c = *str++;
                if (is_vchar(c) || c == HT || c == SP) {
                    // valid quoted-pair
                    continue;
                }
                str--;
                // fallthrough

            default:
                // found illegal byte sequence
                *ustr = str - 1;
                return HWIRE_EILSEQ;
            }
        }
    }

TAIL_REACHED:
    *ustr = str;
    return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN : HWIRE_EAGAIN;
}

int hwire_parse_quoted_string(const char *str, size_t len, size_t *pos,
                              size_t maxlen)
{
    assert(str != NULL);
    assert(pos != NULL);
    const unsigned char *ustr = (const unsigned char *)str;
    const unsigned char *head = ustr;
    const unsigned char *tail = ustr + len;
    int rv                    = 0;

    if (*pos >= len) {
        return (*pos == len && maxlen == 0) ? HWIRE_ELEN : HWIRE_EAGAIN;
    }

    // Adjust the input pointer and scan boundaries based on the current
    // position and maxlen
    ustr += *pos;
    head = ustr;
    if (maxlen < len - *pos) {
        tail = head + maxlen;
    }

    // parse quoted-string
    rv = parse_quoted_string(&ustr, head, tail, maxlen);
    *pos += (size_t)(ustr - head);
    return rv;
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
 * @param ustr Input: start pointer, Output: end pointer
 * @param head Public parser entry pointer
 * @param tail Exclusive scan tail
 * @param maxlen Maximum number of bytes examined from head
 * @param ctx Callback context (must not be NULL)
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if input ends before the budget
 * @return HWIRE_EILSEQ for invalid byte sequence
 * @return HWIRE_ELEN if a required component is incomplete at the budget
 * @return HWIRE_EKEYLEN if key length exceeds ctx->key_lc.size
 * @return HWIRE_ECALLBACK if callback returned non-zero
 * @see RFC 7231 Section 3.1.1.1 Parameter
 * @see RFC 9110 Section 5.6.6 Parameters
 */
static int parse_parameter(const unsigned char **ustr,
                           const unsigned char *head, const unsigned char *tail,
                           size_t maxlen, hwire_ctx_t *ctx)
{
    assert(ustr != NULL && *ustr != NULL);
    const unsigned char *str  = *ustr;
    const unsigned char *pstr = str;
    hwire_param_t param       = {0};

#define CHECK_POSITION()                                                       \
    do {                                                                       \
        if (str >= tail) {                                                     \
            *ustr = str;                                                       \
            return ((size_t)(str - head) >= maxlen) ? HWIRE_ELEN :             \
                                                      HWIRE_EAGAIN;            \
        }                                                                      \
    } while (0)

    // parse parameter-name (token)
    if (ctx->key_lc.size > 0) {
        size_t n = strtchar(str, (size_t)(tail - str), &ctx->key_lc);
        if (n == SIZE_MAX) {
            *ustr = str;
            return HWIRE_EKEYLEN;
        }
        str += n;
    } else {
        str += strtchar(str, (size_t)(tail - str), NULL);
    }
    CHECK_POSITION();
    param.key.ptr = (const char *)pstr;
    param.key.len = (size_t)(str - pstr);
    // parameter-name must not be empty
    if (param.key.len == 0) {
        *ustr = str;
        return HWIRE_EILSEQ;
    }

    // check for '='
    if (*str++ != '=') {
        *ustr = str - 1;
        return HWIRE_EILSEQ;
    }
    CHECK_POSITION();

    // parse parameter-value
    pstr = str;
    if (*str == DQUOTE) {
        // parse as a quoted-string
        int rc = parse_quoted_string(&str, head, tail, maxlen);
        if (rc == HWIRE_OK) {
            param.value.ptr = (const char *)pstr + 1;   // skip opening quote
            param.value.len = (size_t)(str - pstr - 2); // exclude quotes

            // call callback
            if (ctx->param_cb(ctx, &param)) {
                return HWIRE_ECALLBACK;
            }
        }
        *ustr = str;
        return rc;
    }

    // parse as a token
    param.value.ptr = (const char *)str;
    param.value.len = strtchar(str, (size_t)(tail - str), NULL);
    str += param.value.len;
    if (param.value.len == 0) {
        CHECK_POSITION();
        // parameter-value must not be empty
        *ustr = str;
        return HWIRE_EILSEQ;
    }

#undef CHECK_POSITION

    *ustr = str;

    // call callback
    if (ctx->param_cb(ctx, &param)) {
        return HWIRE_ECALLBACK;
    }
    return HWIRE_OK;
}

/**
 * @brief Skip whitespace characters (SP and HT)
 *
 * Skips spaces (SP) and horizontal tabs (HT) in the input string, up to the
 * specified maximum length.
 *
 * @param ustr Input: current pointer; Output: first non-whitespace pointer
 * @param tail Exclusive scan tail
 *
 * @note This function is used to skip BWS (Bad Whitespace) in chunk-size
 * parsing.
 * @note Only SP and HT are considered whitespace (per RFC 7230).
 */
static inline void skip_ws(const unsigned char **ustr,
                           const unsigned char *tail)
{
    assert(ustr != NULL && *ustr != NULL);
    const unsigned char *str = *ustr;

    // skip SP and HT
    while (str < tail) {
        switch (*str++) {
        case SP:
        case HT:
            continue;

        default:
            // stopped at non-whitespace
            *ustr = str - 1;
            return;
        }
    }
    // update position
    *ustr = str;
}

/**
 * @brief Parse parameters from a semicolon-separated list
 *
 * Parses parameters from a semicolon-separated list.
 *
 *  parameters = *( OWS ";" OWS [ parameter ] )
 *
 * Empty parameters do not invoke param_cb. A trailing semicolon is
 * complete at input end. CR or LF after an empty parameter is left unconsumed.
 * After HWIRE_OK, the caller must check *pos against len and, if input
 * remains, validate the terminator at str[*pos].
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @param maxlen Maximum number of bytes examined from the initial *pos
 * @param skip_leading_semicolon Non-zero to skip semicolon check for first
 * @param cb Callback context (must not be NULL)
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if a required component needs more input before maxlen
 * is exhausted
 * @return HWIRE_EILSEQ for invalid byte sequence
 * @return HWIRE_ELEN if a required component is incomplete when maxlen is
 * exhausted
 * @return HWIRE_EKEYLEN if key length exceeds ctx->key_lc.size
 * @return HWIRE_ECALLBACK if callback returned non-zero
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_parameters(hwire_ctx_t *ctx, const char *str, size_t len,
                           size_t *pos, size_t maxlen,
                           int skip_leading_semicolon)
{
    assert(str != NULL);
    assert(pos != NULL);
    assert(ctx != NULL);
    assert(ctx->param_cb != NULL);
    const unsigned char *ustr = (const unsigned char *)str;
    const unsigned char *head = ustr;
    const unsigned char *tail = ustr + len;
    int rv                    = HWIRE_OK;

    if (*pos > len) {
        return HWIRE_EILSEQ;
    }
    ustr += *pos;
    head = ustr;

    if (maxlen < (size_t)(tail - head)) {
        tail = head + maxlen;
    }

    if (skip_leading_semicolon) {
        // skip leading semicolon if present
        goto CHECK_PARAM;
    }

CHECK_NEXT_PARAM:
    skip_ws(&ustr, tail);

    *pos = (size_t)(ustr - (const unsigned char *)str);
    if (ustr >= tail) {
        // The zero-or-more grammar is complete at input end. Reaching the
        // budget while more input exists is a hard length error.
        return (*pos >= len) ? HWIRE_OK : HWIRE_ELEN;
    }
    if (*ustr != SEMICOLON) {
        // no more parameters
        // NOTE: caller must inspect the byte at *pos for CRLF or other
        // terminator after this function returns HWIRE_OK.
        return HWIRE_OK;
    }

SKIP_SEMICOLON:
    // skip ';'
    ustr++;

    *pos = (size_t)(ustr - (const unsigned char *)str);

CHECK_PARAM:
    // skip trailing OWS
    skip_ws(&ustr, tail);

    // reset key_lc.len before parsing each parameter
    ctx->key_lc.len = 0;

    // Checking for end of string is required because we might have
    // consumed a semicolon (empty parameter) and reached EOS.
    // In this case, we have a valid empty parameter at the end, so return OK.
    if (ustr >= tail) {
        *pos = (size_t)(ustr - (const unsigned char *)str);
        return (*pos >= len) ? HWIRE_OK : HWIRE_ELEN;
    }

    if (*ustr == CR || *ustr == LF) {
        // Leave the line terminator for the caller, including after empty
        // parameters.
        *pos = (size_t)(ustr - (const unsigned char *)str);
        return HWIRE_OK;
    }

    // parse one parameter
    // RFC 9110 5.6.6: parameters = *( OWS ";" OWS [ parameter ] )
    if (*ustr == SEMICOLON) {
        // empty parameter, skip
        goto SKIP_SEMICOLON;
    }

    rv = parse_parameter(&ustr, head, tail, maxlen, ctx);
    if (rv == HWIRE_OK) {
        // parsed one parameter, continue to next
        goto CHECK_NEXT_PARAM;
    }

    // propagate error from parse_parameter
    return rv;
}

/**
 * @brief Convert hexadecimal string to size_t
 *
 * Converts hexadecimal digits starting at `*ustr` to a size_t value. Updates
 * `ustr` to point to the first non-hexadecimal character.
 *
 * @param ustr Input: start pointer, Output: first non-hex character
 * @param tail Absolute exclusive scan tail
 * @param maxsize Maximum allowed value; returns HWIRE_ERANGE if exceeded
 * @return Converted value on success (0 if no hex digits found)
 * @return HWIRE_ERANGE if value exceeds maxsize (HWIRE_MAX_CHUNKSIZE =
 * UINT32_MAX)
 *
 * @note This function is used by hwire_parse_chunksize to parse the chunk-size
 * field.
 * @note HEXDIGIT table is used for fast digit lookup (1-16 for valid hex
 * digits).
 */
static int64_t hex2size(const unsigned char **ustr, const unsigned char *tail,
                        uint32_t maxsize)
{
    assert(ustr != NULL && *ustr != NULL);
    const unsigned char *str = *ustr;
    int64_t dec              = 0;

    // hex to decimal
    while (str < tail) {
        unsigned char c = HEXDIGIT[*str++];
        if (!c) {
            // found non hexdigit
            *ustr = str - 1;
            return dec;
        }
        // accumulate digit
        dec = (dec << 4) | (c - 1);

        if (dec > (int64_t)maxsize) {
            // result too large: exceeds maxsize (HWIRE_MAX_CHUNKSIZE =
            // UINT32_MAX)
            return HWIRE_ERANGE;
        }
    }

    *ustr = str;
    return dec;
}

/** @} */ /* end of String Parsing Functions */

/**
 * @name Chunked Transfer Coding Functions
 * @{
 */

/**
 * @brief Parse chunk-size and optional chunk-extensions from a chunk-size line
 *
 * Parses a chunk-size line according to RFC 7230 Section 4.1:
 *   chunk-size = 1*HEXDIG
 *   chunk-ext = *( BWS ";" BWS ext-name [ BWS "=" BWS ext-val ] )
 *   ext-name = token
 *   ext-val = token / quoted-string
 *
 * @param str Input string containing the chunk-size line (must start at
 * chunk-size)
 * @param len Length of input string
 * @param pos Input: start offset, Output: position after CRLF or LF (must not
 * be NULL)
 * @param maxlen Maximum line length and number of input bytes examined from
 * the initial *pos
 * @param cb Callback context (must not be NULL)
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data is needed before maxlen is reached
 * @return HWIRE_ELEN if the line is incomplete upon reaching maxlen
 * @return HWIRE_ERANGE if chunk-size exceeds maxsize
 * @return HWIRE_EEXTNAME if extension name is empty
 * @return HWIRE_EEXTVAL if extension value is invalid
 * @return HWIRE_ECALLBACK if callback returned non-zero
 * @return HWIRE_EILSEQ if byte sequence is illegal
 * @return HWIRE_EEOL if end-of-line terminator is invalid
 *
 * @note This function accepts CRLF or bare LF as the line terminator.
 * @note Extensions with no value have empty string as value (ptr="" len=0).
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_chunksize(hwire_ctx_t *ctx, const char *str, size_t len,
                          size_t *pos, size_t maxlen)
{
    assert(str != NULL);
    assert(pos != NULL);
    assert(ctx != NULL);
    assert(ctx->chunksize_cb != NULL);
    const unsigned char *ustr = (const unsigned char *)str;
    const unsigned char *head = ustr;
    const unsigned char *tail = ustr + len;
    const unsigned char *pstr = NULL;
    const unsigned char *key  = NULL;
    size_t klen               = 0;
    const unsigned char *val  = NULL;
    size_t vlen               = 0;
    int64_t size              = 0;

    if (*pos >= len) {
        return (*pos == len && maxlen == 0) ? HWIRE_ELEN : HWIRE_EAGAIN;
    }
    ustr += *pos;
    head = ustr;

    if (maxlen < (size_t)(tail - head)) {
        tail = head + maxlen;
    }

    if (ustr >= tail) {
        // no input can be examined within the line-length budget
        return HWIRE_ELEN;
    }

    // parse chunk-size
    // chunk-size = 1*HEXDIG
    // RFC 7230 4.1 / RFC 9112 7.1: Chunk Size
    size = hex2size(&ustr, tail, HWIRE_MAX_CHUNKSIZE);
    if (size < 0) {
        // chunk size exceeds maximum allowed size or invalid
        return (int)size;
    } else if (ustr == head) {
        // no hexadecimal digits found
        return HWIRE_EILSEQ;
    }

    // call chunksize callback
    if (ctx->chunksize_cb(ctx, (uint32_t)size)) {
        return HWIRE_ECALLBACK;
    }

    // 4.1.1. Chunk Extensions
    //
    // chunk-ext    = *( BWS ";" BWS ext-name [ BWS "=" BWS ext-val ] )
    // ext-name     = token
    // ext-val      = token / quoted-string
    // RFC 7230 4.1.1 / RFC 9112 7.1.1: Chunk Extensions
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
        skip_ws(&ustr, tail);                                                  \
        if (ustr >= tail) {                                                    \
            return ((size_t)(ustr - head) >= maxlen) ? HWIRE_ELEN :            \
                                                       HWIRE_EAGAIN;           \
        }                                                                      \
    } while (0)

    // skip BWS
    skip_bws();

    switch (*ustr++) {
    default:
        // illegal byte sequence
        return HWIRE_EILSEQ;

    // found tail
    case CR:
        if (ustr >= tail) {
            return ((size_t)(ustr - head) >= maxlen) ? HWIRE_ELEN :
                                                       HWIRE_EAGAIN;
        } else if (*ustr++ != LF) {
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
            if (ctx->chunksize_ext_cb != NULL &&
                ctx->chunksize_ext_cb(ctx, &ext)) {
                return HWIRE_ECALLBACK;
            }
        }
        // return and number of bytes consumed
        *pos += (size_t)(ustr - head);
        return HWIRE_OK;

    case SEMICOLON:
        // has chunk-extensions
        skip_bws();
    }

    // parse chunk-extensions
    // call extension callback for previous extension
    if (klen) {
        hwire_chunksize_ext_t ext = {
            .key   = {.len = klen, .ptr = (const char *)key              },
            .value = {.len = vlen, .ptr = (vlen) ? (const char *)val : ""}
        };
        if (ctx->chunksize_ext_cb != NULL && ctx->chunksize_ext_cb(ctx, &ext)) {
            return HWIRE_ECALLBACK;
        }
        klen = 0;
        vlen = 0;
    }

    // parse ext-name
    pstr = ustr;
    ustr += strtchar(ustr, (size_t)(tail - ustr), NULL);
    if (ustr == pstr) {
        // disallow empty ext-name (invalid extension name)
        return HWIRE_EEXTNAME;
    }
    key  = pstr;
    klen = (size_t)(ustr - pstr);

    skip_bws();
    if (*ustr != EQ) {
        // no ext-value
        goto CHECK_EOL;
    }
    // skip '='
    ustr++;
    skip_bws();

    // parse ext-value
    if (*ustr == DQUOTE) {
        int rv = 0;
        // parse as a quoted-string
        pstr   = ustr + 1;
        rv     = parse_quoted_string(&ustr, head, tail, maxlen);
        if (rv == HWIRE_OK) {
            vlen = (size_t)(ustr - pstr - 1); // exclude closing quote
            val  = pstr;                      // skip opening quote
            goto CHECK_EOL;
        } else if (rv == HWIRE_EILSEQ) {
            // treat an illegal byte sequence as extension value error
            return HWIRE_EEXTVAL;
        }
        return rv;
    }
    // parse as a token
    pstr = ustr;
    ustr += strtchar(ustr, (size_t)(tail - ustr), NULL);
    if (ustr == pstr) {
        // delimiters here mean '=' was followed by no extension value
        if (*ustr == CR || *ustr == LF || *ustr == SEMICOLON) {
            return HWIRE_EEXTVAL;
        }
        return HWIRE_EILSEQ;
    }
    val  = pstr;
    vlen = (size_t)(ustr - pstr);

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
static int parse_hval(const unsigned char **ustr, const unsigned char *tail,
                      size_t *vlen)
{
    assert(ustr != NULL && *ustr != NULL);
    const unsigned char *str = *ustr;
    const unsigned char *val = str;

    // Use strfcchar to scan VCHAR + SP + HT + obs-text in one go
    // This stops at CR, LF, or any invalid character (e.g. CTLs)
    // endc receives the stopped byte from within the SIMD function (pshufb on
    // AVX2/SSE4.2) or via an L1-cached load on SSE2/NEON/scalar — avoids a
    // separate stopped-byte load after strfcchar returns.
    unsigned char endc = 0;
    size_t fcchar_len  = strfcchar(val, (size_t)(tail - str), &endc);
    str += fcchar_len;
    if (str < tail) {
        // Stopped at non-field-content; use endc (already set) instead of
        // loading *str again
        if (likely(endc == CR)) {
            if (unlikely(tail - str < 2)) {
                return HWIRE_EAGAIN;
            } else if (unlikely(str[1] != LF)) {
                return HWIRE_EEOL;
            }
            // valid end of header value, continue to trim OWS and check CRLF
            // set cursor after CRLF and trim trailing OWS
            *ustr = str + 2;

REMOVE_OWS:

#define IS_OWS() (str > val && (str[-1] == SP || str[-1] == HT))

            // Backtrack to trim trailing OWS
            // Enter slow loop only if the last character is OWS
            if (unlikely(IS_OWS())) {
                do {
                    str--;
                } while (IS_OWS());
            }

#undef IS_OWS

            *vlen = (size_t)(str - val);
            return HWIRE_OK;
        } else if (likely(endc == LF)) {
            // only LF found - valid end of header value, continue to trim OWS
            // and check LF
            *ustr = str + 1;
            goto REMOVE_OWS;
        } else {
            return HWIRE_EHDRVALUE;
        }
    }

    return HWIRE_EAGAIN;
}

/**
 * @brief Parse header key and store lowercase in key_lc
 *
 * Ported from parse.c:parse_hkey
 */
static int parse_hkey(const unsigned char **ustr, const unsigned char *tail,
                      size_t *klen, hwire_ctx_t *ctx)
{
    assert(ustr != NULL && *ustr != NULL);
    const unsigned char *str = *ustr;
    size_t tchar_len         = 0;

    if (ctx->key_lc.size > 0) {
        tchar_len = strtchar(str, (size_t)(tail - str), &ctx->key_lc);
        if (tchar_len == SIZE_MAX) {
            return HWIRE_EKEYLEN;
        }
    } else {
        tchar_len = strtchar(str, (size_t)(tail - str), NULL);
    }

    if (unlikely(tchar_len == 0)) {
        // Empty or first character is invalid
        return HWIRE_EHDRNAME;
    }

    str += tchar_len;
    if (likely(str < tail)) {
        // strtchar stopped before tail - check why
        if (likely(*str == ':')) {
            // Found colon - success
            *klen = tchar_len;
            *ustr = str + 1;
            return HWIRE_OK;
        }
        // Non-tchar, non-colon character - error
        return HWIRE_EHDRNAME;
    }

    return HWIRE_EAGAIN;
}

/**
 * @brief Parse HTTP headers (shared implementation)
 *
 * Field names, OWS, values, delimiters, field line endings, and the terminating
 * empty line stay within tail. maxlen is measured from head.
 */
static int parse_headers(hwire_ctx_t *ctx, const unsigned char **ustr,
                         const unsigned char *head, const unsigned char *tail,
                         size_t maxlen)
{
    assert(ustr != NULL && *ustr != NULL);
    assert(ctx != NULL);
    assert(ctx->header_cb != NULL);
    const unsigned char *str        = *ustr;
    const unsigned char *field_head = str;
    size_t klen                     = 0;
    size_t vlen                     = 0;
    int rv                          = 0;
    hwire_header_t header           = {0};

RETRY:
    // End-of-headers (CRLF/LF) or incomplete data — happens once per request,
    // not once per header. Use unlikely to keep the hot header-parsing path
    // as a straight-line fall-through.
    if (unlikely(str >= tail)) {
        return ((size_t)(tail - head) >= maxlen) ? HWIRE_EHDRLEN : HWIRE_EAGAIN;
    }
    if (unlikely(*str <= CR)) {
        if (likely(*str == CR)) {
            if (unlikely(tail - str < 2)) {
                return ((size_t)(tail - head) >= maxlen) ? HWIRE_EHDRLEN :
                                                           HWIRE_EAGAIN;
            } else if (likely(str[1] == LF)) {
                *ustr = str + 2;
                return HWIRE_OK;
            }
            return HWIRE_EEOL;
        } else if (*str++ == LF) {
            *ustr = str;
            return HWIRE_OK;
        }
        str--;
        // Any other control char <= CR falls through to parse_hkey().
    }

    field_head      = str;
    klen            = 0;
    ctx->key_lc.len = 0;
    // parse key and store lowercase in key_lc
    // header-field = field-name ":" OWS field-value OWS
    // field-name = token
    // RFC 7230 3.2 / RFC 9112 5.1: Field Names
    rv              = parse_hkey(&str, tail, &klen, ctx);
    if (unlikely(rv != HWIRE_OK)) {
        if (rv == HWIRE_EAGAIN) {
            return (size_t)(tail - head) >= maxlen ? HWIRE_EHDRLEN :
                                                     HWIRE_EAGAIN;
        }
        return rv;
    }

    // skip OWS
    skip_ws(&str, tail);

    if (unlikely(str >= tail)) {
        return (size_t)(tail - head) >= maxlen ? HWIRE_EHDRLEN : HWIRE_EAGAIN;
    }

    header.value.ptr = (const char *)str;
    vlen             = 0;
    // field-value = *field-content
    // RFC 7230 3.2 / RFC 9112 5.5: Field Values
    // Note: Empty field-value is allowed.
    rv               = parse_hval(&str, tail, &vlen);
    if (unlikely(rv != HWIRE_OK)) {
        if (rv == HWIRE_EAGAIN) {
            return (size_t)(tail - head) >= maxlen ? HWIRE_EHDRLEN :
                                                     HWIRE_EAGAIN;
        }
        return rv;
    }

    // set header key and value
    header.key.ptr   = (const char *)field_head;
    header.key.len   = klen;
    header.value.len = vlen;

    // call callback
    if (unlikely(ctx->header_cb(ctx, &header) != 0)) {
        return HWIRE_ECALLBACK;
    }

    goto RETRY;
}

/**
 * @brief Parse HTTP headers
 *
 * Ported from parse.c:parse_header
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_headers(hwire_ctx_t *ctx, const char *str, size_t len,
                        size_t *pos, size_t maxlen)
{
    assert(str != NULL);
    assert(pos != NULL);
    assert(ctx != NULL);
    assert(ctx->header_cb != NULL);
    const unsigned char *ustr = (const unsigned char *)str;
    const unsigned char *head = ustr;
    const unsigned char *tail = ustr + len;
    int rv                    = HWIRE_OK;

    if (unlikely(*pos >= len)) {
        return HWIRE_EAGAIN;
    }
    // Adjust the input pointers based on the current position
    ustr += *pos;
    head = ustr;
    if (maxlen < len - *pos) {
        tail = head + maxlen;
    }

    rv = parse_headers(ctx, &ustr, head, tail, maxlen);
    if (rv == HWIRE_OK) {
        *pos += (size_t)(ustr - head);
    }
    return rv;
}

/** @} */ /* end of HTTP Headers Parsing Functions */

/**
 * @name HTTP Request Parsing Functions
 * @{
 */

static inline int uri_is_alpha(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static inline int uri_is_digit(unsigned char c)
{
    return c >= '0' && c <= '9';
}

static inline int uri_is_unreserved(unsigned char c)
{
    return uri_is_alpha(c) || uri_is_digit(c) || c == '-' || c == '.' ||
           c == '_' || c == '~';
}

static inline int uri_is_sub_delim(unsigned char c)
{
    return c == '!' || c == '$' || c == '&' || c == '\'' || c == '(' ||
           c == ')' || c == '*' || c == '+' || c == ',' || c == ';' || c == '=';
}

static inline int uri_incomplete(const unsigned char *head,
                                 const unsigned char *tail, size_t maxlen)
{
    return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN : HWIRE_EAGAIN;
}

// Callers map an incomplete triplet to their own input-completeness error.
static inline int parse_pct_triplet(const unsigned char *str,
                                    const unsigned char *tail,
                                    unsigned char *decoded)
{
    if (tail - str < 3) {
        return HWIRE_EAGAIN;
    }
    unsigned char hi = HEXDIGIT[str[1]];
    unsigned char lo = HEXDIGIT[str[2]];
    if (hi == 0 || lo == 0) {
        return HWIRE_EURI;
    }
    if (decoded != NULL) {
        *decoded = (unsigned char)(((hi - 1) << 4) | (lo - 1));
    }
    return HWIRE_OK;
}

static inline int parse_pct_encoded(const unsigned char **ustr,
                                    const unsigned char *head,
                                    const unsigned char *tail, size_t maxlen)
{
    const unsigned char *str = *ustr;
    int rv                   = parse_pct_triplet(str, tail, NULL);
    if (rv == HWIRE_EAGAIN) {
        return uri_incomplete(head, tail, maxlen);
    } else if (rv != HWIRE_OK) {
        return rv;
    }
    *ustr = str + 3;
    return HWIRE_OK;
}

static int parse_path_query(const unsigned char **ustr,
                            const unsigned char *head,
                            const unsigned char *tail, size_t maxlen,
                            hwire_str_t *path, hwire_str_t *query)
{
    const unsigned char *pstr = *ustr;
    const unsigned char *str  = pstr;
    int rv                    = 0;

    *path  = (hwire_str_t){0};
    *query = (hwire_str_t){0};
    while (str < tail) {
        str += strurichar(str, (size_t)(tail - str));
        if (str == tail) {
            return uri_incomplete(head, tail, maxlen);
        } else if (*str == SP) {
            if (path->ptr == NULL) {
                path->ptr = (const char *)pstr;
                path->len = (size_t)(str - pstr);
            } else {
                query->len = (size_t)(str - (const unsigned char *)query->ptr);
            }
            *ustr = str + 1;
            return HWIRE_OK;
        } else if (*str == '%') {
            rv = parse_pct_encoded(&str, head, tail, maxlen);
            if (rv != HWIRE_OK) {
                return rv;
            }
        } else if (*str == '?') {
            // Later question marks are query data, not new delimiters.
            if (query->ptr == NULL) {
                path->ptr  = (const char *)pstr;
                path->len  = (size_t)(str - pstr);
                query->ptr = (const char *)(str + 1);
            }
            str++;
        } else {
            return HWIRE_EURI;
        }
    }

    return uri_incomplete(head, tail, maxlen);
}

// Parse and deliver one nonempty query segment, consuming a trailing '&'.
static int parse_query_parameter(const unsigned char **ustr,
                                 const unsigned char *head,
                                 const unsigned char *tail, size_t maxlen,
                                 hwire_ctx_t *ctx)
{
    const unsigned char *str  = *ustr;
    hwire_buf_t *qrybuf       = &ctx->qrybuf;
    char *key                 = qrybuf->buf + qrybuf->len;
    char *out                 = key;
    char *out_tail            = qrybuf->buf + qrybuf->size;
    hwire_query_param_t param = {0};
    size_t qrylen;
    int rv;

PARSE_QRYCHAR:
    qrylen = strqrychar(str, (size_t)(tail - str));
    if (qrylen != 0) {
        if (qrylen > (size_t)(out_tail - out)) {
            return HWIRE_ENOBUFS;
        }
        memcpy(out, str, qrylen);
        out += qrylen;
    }
    str += qrylen;

    // If we have reached the end of the input, finalize the current parameter.
    if (str >= tail) {
        goto PARAM_END;
    }

    if (*str == '&') {
        // Skip the '&' delimiter to the start of the next parameter.
        str++;
    } else {
        // If the current character is not a query delimiter, decode it.
        unsigned char c = 0;

        switch (*str) {
        case '+':
            c = SP;
            str++;
            break;

        case '%':
            // Percent-encoded triplet, e.g., "%20" for space.
            rv = parse_pct_triplet(str, tail, &c);
            if (rv == HWIRE_EAGAIN) {
                return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN :
                                                           HWIRE_EAGAIN;
            } else if (rv != HWIRE_OK) {
                return rv;
            }
            str += 3;
            break;

        case '=':
            if (!param.key.ptr) {
                param.key.ptr   = key;
                param.key.len   = (size_t)(out - key);
                param.value.ptr = out;
                // skip the '=' character
                str++;
                goto PARSE_QRYCHAR;
            }
            // If we encounter an '=' after the key has already been set, treat
            // it as a literal character.
            c = '=';
            str++;
            break;

        default:
            return HWIRE_EURI;
        }

        // Append the decoded character to the output buffer.
        if (out == out_tail) {
            return HWIRE_ENOBUFS;
        }
        *out++ = (char)c;
        goto PARSE_QRYCHAR;
    }

PARAM_END:
    // Finalize the current query parameter before invoking the callback.
    if (param.value.ptr) {
        param.value.len = (size_t)(out - param.value.ptr);
    } else {
        param.key.ptr = key;
        param.key.len = (size_t)(out - key);
    }

    if (ctx->query_cb(ctx, &param) != 0) {
        return HWIRE_ECALLBACK;
    }

    qrybuf->len = (size_t)(out - qrybuf->buf);
    *ustr       = str;
    return HWIRE_OK;
}

/**
 * @brief Parse query input into decoded key/value pairs
 *
 * Split each nonempty '&'-separated segment at its first literal '=' and
 * deliver it through ctx->query_cb. Decode '%HH' and '+' into the caller-owned
 * ctx->qrybuf; encoded delimiters remain data. Empty keys and values are
 * allowed, while empty segments are skipped.
 *
 * @param ctx Context with query_cb and a non-NULL, non-overlapping qrybuf.buf
 * @param str Query input without the leading '?' (must not be NULL)
 * @param len Number of available bytes in str
 * @param pos Input: start offset; output: parser position. An initial offset
 * greater than len returns HWIRE_EILSEQ without changing *pos.
 * @param maxlen Maximum number of bytes examined from the initial *pos
 * @return HWIRE_OK if all available input was consumed, with *pos == len
 * @return HWIRE_EAGAIN if a percent escape needs more input before maxlen
 * @return HWIRE_EURI for an invalid query byte or percent hex digit
 * @return HWIRE_ELEN if the byte budget ends with more input or during an
 * incomplete percent escape
 * @return HWIRE_ENOBUFS if the decode buffer is exhausted
 * @return HWIRE_ECALLBACK if query_cb returns non-zero
 * @return HWIRE_EILSEQ if the initial *pos exceeds len
 *
 * Earlier callbacks are not rolled back on failure. A pair ending at the
 * byte-budget boundary may be delivered before HWIRE_ELEN is returned.
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_query(hwire_ctx_t *ctx, const char *str, size_t len,
                      size_t *pos, size_t maxlen)
{
    assert(ctx != NULL);
    assert(ctx->query_cb != NULL);
    assert(str != NULL);
    assert(pos != NULL);
    assert(ctx->qrybuf.buf != NULL);
    const unsigned char *ustr = (const unsigned char *)str;
    const unsigned char *head = ustr;
    const unsigned char *tail = ustr + len;

    if (*pos > len) {
        return HWIRE_EILSEQ;
    }
    ustr += *pos;
    head = ustr;

    if (maxlen < (size_t)(tail - head)) {
        tail = head + maxlen;
    }
    ctx->qrybuf.len = 0;

CHECK_NEXT_PARAM:
    *pos = (size_t)(ustr - (unsigned char *)str);
    if (ustr >= tail) {
        return (*pos >= len) ? HWIRE_OK : HWIRE_ELEN;
    } else if (*ustr == '&') {
        ustr++;
    } else {
        int rv = parse_query_parameter(&ustr, head, tail, maxlen, ctx);
        if (rv != HWIRE_OK) {
            return rv;
        }
    }
    goto CHECK_NEXT_PARAM;
}

// Parse IPv4address and leave ustr at the byte after its fourth dec-octet.
static int parse_ip_v4(const unsigned char **ustr, const unsigned char *head,
                       const unsigned char *tail, size_t maxlen)
{
    const unsigned char *str = *ustr;

    for (size_t part = 0; part < 4; part++) {
        const unsigned char *pstr = str;
        size_t digits             = 0;
        unsigned int value        = 0;

        while (str < tail && uri_is_digit(*str)) {
            if (++digits > 3) {
                return HWIRE_EURI;
            }
            value = value * 10U + (unsigned int)(*str - '0');
            str++;
        }

        if (value > 255U || (digits > 1 && *pstr == '0')) {
            return HWIRE_EURI;
        } else if (part == 3 && digits != 0) {
            *ustr = str;
            return HWIRE_OK;
        } else if (str >= tail) {
            return uri_incomplete(head, tail, maxlen);
        } else if (digits == 0 || *str != '.') {
            return HWIRE_EURI;
        }
        str++;
    }
    /* NOTE: The fourth octet always returns from inside the loop. */
    return HWIRE_EURI; /* LCOV_EXCL_LINE */
}

static int parse_ip_v6(const unsigned char **ustr, const unsigned char *head,
                       const unsigned char *tail, size_t maxlen)
{
    const unsigned char *str         = *ustr;
    const unsigned char *group_start = NULL;
    size_t groups                    = 0;
    size_t digits                    = 0;
    unsigned char c                  = 0;
    int compressed                   = 0;
    int rv                           = 0;

#define CHECK_LEN()                                                            \
    do {                                                                       \
        if (str >= tail) {                                                     \
            return uri_incomplete(head, tail, maxlen);                         \
        }                                                                      \
    } while (0)

    c = *str;
    if (c == ':') {
        str++;
        CHECK_LEN();
        if (*str != ':') {
            return HWIRE_EURI;
        }
        str++;
        compressed = 1;
    }

IPV6_GROUP_START:
    CHECK_LEN();
    if (*str == ']') {
        if (!compressed || groups >= 8) {
            return HWIRE_EURI;
        }
        goto IPV6_END;
    }
    group_start = str;
    digits      = 0;

IPV6_GROUP:
    CHECK_LEN();
    c = *str;
    if (HEXDIGIT[c] != 0) {
        if (++digits > 4) {
            return HWIRE_EURI;
        }
        str++;
        goto IPV6_GROUP;
    } else if (c == '.') {
        if (groups > 6) {
            return HWIRE_EURI;
        }
        str = group_start;
        rv  = parse_ip_v4(&str, head, tail, maxlen);
        if (rv != HWIRE_OK) {
            return rv;
        }
        CHECK_LEN();
        if (*str != ']') {
            return HWIRE_EURI;
        }
        groups += 2;
        if ((!compressed && groups != 8) || (compressed && groups >= 8)) {
            return HWIRE_EURI;
        }
        goto IPV6_END;
    } else if (digits == 0) {
        return HWIRE_EURI;
    }
    groups++;
    if (groups > 8) {
        return HWIRE_EURI;
    } else if (c == ']') {
        if ((!compressed && groups != 8) || (compressed && groups >= 8)) {
            return HWIRE_EURI;
        }
        goto IPV6_END;
    } else if (c != ':') {
        return HWIRE_EURI;
    }
    str++;
    CHECK_LEN();
    if (*str == ':') {
        if (compressed) {
            return HWIRE_EURI;
        }
        compressed = 1;
        str++;
    } else if (*str == ']') {
        return HWIRE_EURI;
    }
    goto IPV6_GROUP_START;

IPV6_END:
    *ustr = str;
    return HWIRE_OK;

#undef CHECK_LEN
}

static int parse_ip_vfuture(const unsigned char **ustr,
                            const unsigned char *head,
                            const unsigned char *tail, size_t maxlen)
{
    const unsigned char *str           = *ustr;
    const unsigned char *version_start = str;
    const unsigned char *value_start   = NULL;
    unsigned char c                    = 0;

    while (str < tail && HEXDIGIT[*str] != 0) {
        str++;
    }
    if (str == tail) {
        return uri_incomplete(head, tail, maxlen);
    } else if (str == version_start || *str != '.') {
        return HWIRE_EURI;
    }
    str++;
    value_start = str;

    while (str < tail) {
        c = *str;
        if (c == ']') {
            if (str == value_start) {
                return HWIRE_EURI;
            }
            *ustr = str;
            return HWIRE_OK;
        } else if (!uri_is_unreserved(c) && !uri_is_sub_delim(c) && c != ':') {
            return HWIRE_EURI;
        }
        str++;
    }
    return uri_incomplete(head, tail, maxlen);
}

// Parse IP-literal content starting after '[' and leave ustr after ']'.
static int parse_ip_literal(const unsigned char **ustr,
                            const unsigned char *head,
                            const unsigned char *tail, size_t maxlen)
{
    const unsigned char *str = *ustr;
    int rv                   = 0;

    if (str >= tail) {
        return uri_incomplete(head, tail, maxlen);
    } else if (*str == 'v' || *str == 'V') {
        str++;
        rv = parse_ip_vfuture(&str, head, tail, maxlen);
    } else {
        rv = parse_ip_v6(&str, head, tail, maxlen);
    }

    if (rv != HWIRE_OK) {
        return rv;
    }
    *ustr = str + 1;
    return HWIRE_OK;
}

static int parse_authority_form(const unsigned char **ustr,
                                const unsigned char *head,
                                const unsigned char *tail, size_t maxlen,
                                hwire_request_t *req)
{
    const unsigned char *str        = *ustr;
    const unsigned char *host_start = str;
    const unsigned char *host_end   = NULL;
    const unsigned char *port_start = NULL;
    unsigned char c                 = 0;
    int rv                          = 0;

    /* NOTE: parse_uri checks for available input before dispatching here. */
    if (str >= tail) {                             /* LCOV_EXCL_BR_LINE */
        return uri_incomplete(head, tail, maxlen); /* LCOV_EXCL_LINE */
    } else if (*str == '[') {
        str++;
        rv = parse_ip_literal(&str, head, tail, maxlen);
        if (rv != HWIRE_OK) {
            return rv;
        }
        host_end = str;
        if (str >= tail) {
            return uri_incomplete(head, tail, maxlen);
        } else if (*str != ':') {
            return HWIRE_EURI;
        }
        str++;
        goto PORT_FIRST;
    }

REG_NAME:
    if (str >= tail) {
        return uri_incomplete(head, tail, maxlen);
    }
    c = *str;
    if (likely(URI_CHAR[c] & URI_REGNAME_CHAR)) {
        str++;
        goto REG_NAME;
    } else if (c == '%') {
        rv = parse_pct_encoded(&str, head, tail, maxlen);
        if (rv != HWIRE_OK) {
            return rv;
        }
        goto REG_NAME;
    } else if (c != ':' || str == host_start) {
        return HWIRE_EURI;
    }
    host_end = str;
    str++;
    goto PORT_FIRST;

PORT_FIRST:
    if (str >= tail) {
        return uri_incomplete(head, tail, maxlen);
    } else if (!uri_is_digit(*str)) {
        return HWIRE_EURI;
    }
    port_start = str;

PORT:
    str++;
    if (str >= tail) {
        return uri_incomplete(head, tail, maxlen);
    } else if (uri_is_digit(*str)) {
        goto PORT;
    } else if (*str != SP) {
        return HWIRE_EURI;
    }

    req->uri.ptr  = (const char *)host_start;
    req->uri.len  = (size_t)(str - host_start);
    req->uri_type = HWIRE_AUTHORITY_URI;
    req->host.ptr = (const char *)host_start;
    req->host.len = (size_t)(host_end - host_start);
    req->port.ptr = (const char *)port_start;
    req->port.len = (size_t)(str - port_start);
    req->scheme   = (hwire_str_t){0};
    req->userinfo = (hwire_str_t){0};
    req->path     = (hwire_str_t){0};
    req->query    = (hwire_str_t){0};
    *ustr         = str + 1;
    return HWIRE_OK;
}

static int parse_origin_form(const unsigned char **ustr,
                             const unsigned char *head,
                             const unsigned char *tail, size_t maxlen,
                             hwire_request_t *req)
{
    const unsigned char *pstr = *ustr;
    int rv =
        parse_path_query(ustr, head, tail, maxlen, &req->path, &req->query);

    if (rv != HWIRE_OK) {
        return rv;
    }
    req->uri.ptr  = (const char *)pstr;
    req->uri.len  = (size_t)(*ustr - pstr - 1);
    req->uri_type = HWIRE_ORIGIN_URI;
    req->scheme   = (hwire_str_t){0};
    req->userinfo = (hwire_str_t){0};
    req->host     = (hwire_str_t){0};
    req->port     = (hwire_str_t){0};
    return HWIRE_OK;
}

static int parse_uri_authority(const unsigned char **ustr,
                               const unsigned char *head,
                               const unsigned char *tail, size_t maxlen,
                               hwire_request_t *req)
{
    const unsigned char *pstr       = *ustr;
    const unsigned char *str        = pstr;
    const unsigned char *host_start = pstr;
    const unsigned char *host_end   = NULL;
    const unsigned char *port_start = NULL;
    unsigned char c                 = 0;
    int has_userinfo                = 0;
    int port_valid                  = 1;
    int rv                          = 0;

AUTHORITY_START:
    host_start = str;
    host_end   = NULL;
    port_start = NULL;
    port_valid = 1;
    if (str >= tail) {
        return uri_incomplete(head, tail, maxlen);
    } else if (*str == '[') {
        str++;
        rv = parse_ip_literal(&str, head, tail, maxlen);
        if (rv != HWIRE_OK) {
            return rv;
        }
        host_end = str;
        goto IP_LITERAL_END;
    }

    // Before '@', a colon and its suffix remain a provisional port because
    // colon is also valid in userinfo. A raw '@' restarts this loop at host.
AUTHORITY_COMPONENT:
    if (str >= tail) {
        return uri_incomplete(head, tail, maxlen);
    }
    c = *str;
    if (likely(URI_CHAR[c] & URI_REGNAME_CHAR)) {
        if (port_start != NULL && !uri_is_digit(c)) {
            port_valid = 0;
        }
        str++;
        goto AUTHORITY_COMPONENT;
    } else if (c == '%') {
        if (port_start != NULL) {
            port_valid = 0;
        }
        rv = parse_pct_encoded(&str, head, tail, maxlen);
        if (rv != HWIRE_OK) {
            return rv;
        }
        goto AUTHORITY_COMPONENT;
    } else if (c == ':') {
        if (port_start == NULL) {
            host_end   = str;
            port_start = str + 1;
        } else {
            port_valid = 0;
        }
        str++;
        goto AUTHORITY_COMPONENT;
    } else if (c == '@') {
        if (has_userinfo) {
            return HWIRE_EURI;
        }
        req->userinfo.ptr = (const char *)pstr;
        req->userinfo.len = (size_t)(str - pstr);
        has_userinfo      = 1;
        str++;
        goto AUTHORITY_START;
    } else if (c == '/' || c == '?' || c == SP) {
        if (!port_valid) {
            return HWIRE_EURI;
        } else if (host_end == NULL) {
            host_end = str;
        }
        goto AUTHORITY_END;
    }
    return HWIRE_EURI;

IP_LITERAL_END:
    if (str >= tail) {
        return uri_incomplete(head, tail, maxlen);
    }
    c = *str;
    if (c == ':') {
        port_start = ++str;
        goto IP_LITERAL_PORT;
    } else if (c != '/' && c != '?' && c != SP) {
        return HWIRE_EURI;
    }
    goto AUTHORITY_END;

IP_LITERAL_PORT:
    if (str >= tail) {
        return uri_incomplete(head, tail, maxlen);
    }
    c = *str;
    if (uri_is_digit(c)) {
        str++;
        goto IP_LITERAL_PORT;
    } else if (c != '/' && c != '?' && c != SP) {
        return HWIRE_EURI;
    }

AUTHORITY_END:
    if (port_start != NULL) {
        req->port.ptr = (const char *)port_start;
        req->port.len = (size_t)(str - port_start);
    } else {
        req->port = (hwire_str_t){0};
    }
    if (!has_userinfo) {
        req->userinfo = (hwire_str_t){0};
    }
    req->host.ptr = (const char *)host_start;
    req->host.len = (size_t)(host_end - host_start);
    *ustr         = str;
    return HWIRE_OK;
}

// Parse an RFC 3986 scheme and leave ustr at the byte after its colon.
static int parse_uri_scheme(const unsigned char **ustr,
                            const unsigned char *head,
                            const unsigned char *tail, size_t maxlen,
                            hwire_str_t *scheme)
{
    const unsigned char *pstr = *ustr;
    const unsigned char *str  = pstr;

    if (tail - str < 2) {
        return uri_incomplete(head, tail, maxlen);
    } else if (!uri_is_alpha(*str)) {
        return HWIRE_EURI;
    }
    str++;

    while (*str != ':') {
        if (!uri_is_alpha(*str) && !uri_is_digit(*str) && *str != '+' &&
            *str != '-' && *str != '.') {
            return HWIRE_EURI;
        }
        str++;
        if (str >= tail) {
            return uri_incomplete(head, tail, maxlen);
        }
    }
    scheme->ptr = (const char *)pstr;
    scheme->len = (size_t)(str - pstr);
    *ustr       = str + 1;
    return HWIRE_OK;
}

static int parse_absolute_form(const unsigned char **ustr,
                               const unsigned char *head,
                               const unsigned char *tail, size_t maxlen,
                               hwire_request_t *req)
{
    const unsigned char *pstr = *ustr;
    const unsigned char *str  = pstr;
    int has_authority         = 0;
    int rv = parse_uri_scheme(&str, head, tail, maxlen, &req->scheme);

    if (rv != HWIRE_OK) {
        return rv;
    } else if (str >= tail) {
        return uri_incomplete(head, tail, maxlen);
    } else if (*str == '/') {
        if (tail - str < 2) {
            return uri_incomplete(head, tail, maxlen);
        } else if (str[1] == '/') {
            str += 2;
            rv = parse_uri_authority(&str, head, tail, maxlen, req);
            if (rv != HWIRE_OK) {
                return rv;
            }
            has_authority = 1;
        }
    }

    rv = parse_path_query(&str, head, tail, maxlen, &req->path, &req->query);
    if (rv != HWIRE_OK) {
        return rv;
    }
    req->uri.ptr  = (const char *)pstr;
    req->uri.len  = (size_t)(str - pstr - 1);
    req->uri_type = HWIRE_ABSOLUTE_URI;
    if (!has_authority) {
        req->userinfo = (hwire_str_t){0};
        req->host     = (hwire_str_t){0};
        req->port     = (hwire_str_t){0};
    }
    *ustr = str;
    return HWIRE_OK;
}

static inline int method_is(const hwire_str_t *method, const char *name,
                            size_t namelen)
{
    return method->len == namelen && memcmp(method->ptr, name, namelen) == 0;
}

static int parse_asterisk_form(const unsigned char **ustr, hwire_request_t *req)
{
    const unsigned char *str = *ustr;

    if (!method_is(&req->method, "OPTIONS", 7)) {
        return HWIRE_EURI;
    }
    req->uri.ptr  = (const char *)str;
    req->uri.len  = 1;
    req->uri_type = HWIRE_ASTERISK_URI;
    req->path     = (hwire_str_t){0};
    req->query    = (hwire_str_t){0};
    req->scheme   = (hwire_str_t){0};
    req->userinfo = (hwire_str_t){0};
    req->host     = (hwire_str_t){0};
    req->port     = (hwire_str_t){0};
    *ustr         = str + 2;
    return HWIRE_OK;
}

/**
 * @brief Parse and decompose an RFC 9112 request-target
 */
static int parse_uri(const unsigned char **ustr, const unsigned char *head,
                     const unsigned char *tail, size_t maxlen,
                     hwire_request_t *req)
{
    const unsigned char *str = *ustr;

    if (str >= tail) {
        return uri_incomplete(head, tail, maxlen);
    }

    if (*str == '*') {
        if (tail - str < 2) {
            return uri_incomplete(head, tail, maxlen);
        } else if (str[1] == SP) {
            return parse_asterisk_form(ustr, req);
        } else if (!method_is(&req->method, "CONNECT", 7)) {
            return HWIRE_EURI;
        }
        return parse_authority_form(ustr, head, tail, maxlen, req);
    } else if (method_is(&req->method, "CONNECT", 7)) {
        return parse_authority_form(ustr, head, tail, maxlen, req);
    } else if (*str == '/') {
        return parse_origin_form(ustr, head, tail, maxlen, req);
    }
    return parse_absolute_form(ustr, head, tail, maxlen, req);
}

/**
 * @brief Parse HTTP version string
 *
 * @param ustr Input: start pointer, Output: pointer after version string
 * @param head Parser entry pointer
 * @param tail Exclusive scan tail
 * @param maxlen Maximum number of bytes examined from head
 * @param version Output: HTTP version enum
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data needed
 * @return HWIRE_ELEN if the remaining budget cannot hold the version string
 * @return HWIRE_EVERSION for invalid version
 */
static int parse_version(const unsigned char **ustr, const unsigned char *head,
                         const unsigned char *tail, size_t maxlen,
                         hwire_http_version_t *version)
{
#define VER_LEN 8
    assert(ustr != NULL && *ustr != NULL);
    const unsigned char *str = *ustr;

    if (unlikely(tail - str < VER_LEN)) {
        size_t consumed = (size_t)(str - head);
        return (maxlen - consumed < VER_LEN) ? HWIRE_ELEN : HWIRE_EAGAIN;
    }
    if (memcmp(str, "HTTP/1.1", VER_LEN) == 0) {
        *version = HWIRE_HTTP_V11;
        *ustr    = str + VER_LEN;
        return HWIRE_OK;
    } else if (memcmp(str, "HTTP/1.0", VER_LEN) == 0) {
        *version = HWIRE_HTTP_V10;
        *ustr    = str + VER_LEN;
        return HWIRE_OK;
    }
    return HWIRE_EVERSION;
#undef VER_LEN
}

/**
 * @brief Parse HTTP method
 *
 * Parses method as 1*tchar followed by SP, length-capped by maxlen (mirrors
 * parse_uri so the method participates in the request's cumulative budget).
 *
 * @param ustr Input: start pointer, Output: pointer after method and SP
 * @param head Parser entry pointer
 * @param tail Exclusive scan tail
 * @param maxlen Maximum number of bytes examined from head
 * @param method Output: method string slice
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data needed
 * @return HWIRE_ELEN if method length exceeds maxlen
 * @return HWIRE_EMETHOD for invalid method (not tchar or no SP)
 */
static int parse_method(const unsigned char **ustr, const unsigned char *head,
                        const unsigned char *tail, size_t maxlen,
                        hwire_str_t *method)
{
    assert(ustr != NULL && *ustr != NULL);
    const unsigned char *pstr = *ustr;
    size_t mlen               = strtchar(pstr, (size_t)(tail - pstr), NULL);
    const unsigned char *str  = pstr + mlen;

    // method = 1*tchar terminated by SP
    if (str < tail) {
        if (*str++ == SP) {
            if (mlen == 0) {
                // method must not be empty
                return HWIRE_EMETHOD;
            }
            method->ptr = (const char *)pstr;
            method->len = mlen;
            *ustr       = str;
            return HWIRE_OK;
        }
        return HWIRE_EMETHOD;
    }
    return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN : HWIRE_EAGAIN;
}

/**
 * @brief Parse request line
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_request(hwire_ctx_t *ctx, const char *str, size_t len,
                        size_t *pos, size_t maxlen)
{
    assert(str != NULL);
    assert(pos != NULL);
    assert(ctx != NULL);
    assert(ctx->request_cb != NULL);
    assert(ctx->header_cb != NULL);
    const unsigned char *ustr = (const unsigned char *)str;
    const unsigned char *head = ustr;
    const unsigned char *tail = ustr + len;
    hwire_request_t req;
    int rv = 0;

    if (unlikely(*pos >= len)) {
        return (*pos == len && maxlen == 0) ? HWIRE_ELEN : HWIRE_EAGAIN;
    }
    // Adjust the input pointers based on the current position
    ustr += *pos;
    head = ustr;
    if (maxlen < len - *pos) {
        tail = head + maxlen;
    }

SKIP_NEXT_CRLF:
    if (unlikely(ustr >= tail)) {
        return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN : HWIRE_EAGAIN;
    }
    switch (*ustr) {
    case CR:
        if (unlikely(tail - ustr < 2)) {
            return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN :
                                                       HWIRE_EAGAIN;
        } else if (unlikely(ustr[1] != LF)) {
            return HWIRE_EEOL;
        }
        ustr += 2;
        goto SKIP_NEXT_CRLF;

    case LF:
        ustr++;
        goto SKIP_NEXT_CRLF;
    }

    // parse method
    // method = 1*tchar
    // RFC 7230 3.1.1 / RFC 9112 3.1: Method
    rv = parse_method(&ustr, head, tail, maxlen, &req.method);
    if (rv != HWIRE_OK) {
        return rv;
    }

    // parse-uri (find SP delimiter)
    // request-target = origin-form / absolute-form / authority-form /
    // asterik-form RFC 7230 3.1.1 / RFC 9112 3.2: Request Target
    rv = parse_uri(&ustr, head, tail, maxlen, &req);
    if (rv != HWIRE_OK) {
        return rv;
    }

    // parse version
    // HTTP-version = HTTP-name "/" DIGIT "." DIGIT
    // RFC 7230 2.6 / RFC 9110 2.5: Protocol Versioning
    rv = parse_version(&ustr, head, tail, maxlen, &req.version);
    if (rv != HWIRE_OK) {
        return rv;
    }

    // check end-of-line after version
    if (unlikely(ustr >= tail)) {
        return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN : HWIRE_EAGAIN;
    }
    switch (*ustr++) {
    case CR:
        if (ustr >= tail) {
            return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN :
                                                       HWIRE_EAGAIN;
        } else if (*ustr++ != LF) {
            // invalid end-of-line terminator
            return HWIRE_EEOL;
        }
        break;

    case LF:
        break;

    default:
        return HWIRE_EVERSION;
    }

    // call request callback
    if (ctx->request_cb(ctx, &req) != 0) {
        return HWIRE_ECALLBACK;
    }

    // parse headers within the remaining message budget (cumulative total)
    rv = parse_headers(ctx, &ustr, head, tail, maxlen);
    if (rv != HWIRE_OK) {
        return rv;
    }
    *pos += (size_t)(ustr - head);
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
 * @param ustr Input: start pointer, Output: pointer after line terminator
 * @param head Parser entry pointer
 * @param tail Exclusive scan tail
 * @param maxlen Maximum number of bytes examined from head
 * @param reason_len Output: reason phrase length
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data needed
 * @return HWIRE_ELEN if the remaining budget cannot hold the line terminator
 * @return HWIRE_EEOL for invalid end-of-line
 * @return HWIRE_EILSEQ for invalid byte sequence
 */
static int parse_reason(const unsigned char **ustr, const unsigned char *head,
                        const unsigned char *tail, size_t maxlen,
                        size_t *reason_len)
{
    assert(ustr != NULL && *ustr != NULL);
    unsigned char endc        = 0;
    const unsigned char *pstr = *ustr;
    size_t n                  = strfcchar(pstr, (size_t)(tail - pstr), &endc);
    const unsigned char *str  = pstr + n;

    if (str < tail) {
        if (likely(endc == CR)) {
            if (unlikely(tail - str < 2)) {
                size_t consumed = (size_t)(str - head);
                return (maxlen - consumed < 2) ? HWIRE_ELEN : HWIRE_EAGAIN;
            } else if (unlikely(str[1] != LF)) {
                return HWIRE_EEOL;
            }
            *reason_len = n;
            *ustr       = str + 2;
            return HWIRE_OK;
        } else if (likely(endc == LF)) {
            *ustr       = str + 1;
            *reason_len = n;
            return HWIRE_OK;
        }
        return HWIRE_EILSEQ;
    }
    return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN : HWIRE_EAGAIN;
}

/**
 * @brief Parse HTTP status code
 *
 * @param ustr Input: start pointer, Output: pointer after status and SP
 * @param head Parser entry pointer
 * @param tail Exclusive scan tail
 * @param maxlen Maximum number of bytes examined from head
 * @param status Output: status code (must not be NULL)
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if more data needed
 * @return HWIRE_ELEN if the remaining budget cannot hold status and SP
 * @return HWIRE_ESTATUS for invalid status code
 */
static int parse_status(const unsigned char **ustr, const unsigned char *head,
                        const unsigned char *tail, size_t maxlen,
                        uint16_t *status)
{
#define STATUS_LEN 3
    assert(ustr != NULL && *ustr != NULL);
    const unsigned char *str = *ustr;

    if (unlikely(tail - str <= STATUS_LEN)) {
        size_t consumed = (size_t)(str - head);
        return (maxlen - consumed <= STATUS_LEN) ? HWIRE_ELEN : HWIRE_EAGAIN;
    } else if (str[STATUS_LEN] != SP) {
        return HWIRE_ESTATUS;
    }
    // HTTP status code: 3*DIGIT (100-599)
    else if (str[0] < '1' || str[0] > '5' || str[1] < '0' || str[1] > '9' ||
             str[2] < '0' || str[2] > '9') {
        return HWIRE_ESTATUS;
    }

    *ustr   = str + STATUS_LEN + 1;
    *status = (str[0] - 0x30) * 100 + (str[1] - 0x30) * 10 + (str[2] - 0x30);
    return HWIRE_OK;

#undef STATUS_LEN
}

/**
 * @brief Parse HTTP response
 *
 * Parses status line and headers, calling response_cb after status line
 * and header_cb for each header.
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_response(hwire_ctx_t *ctx, const char *str, size_t len,
                         size_t *pos, size_t maxlen)
{
    assert(str != NULL);
    assert(pos != NULL);
    assert(ctx != NULL);
    assert(ctx->response_cb != NULL);
    assert(ctx->header_cb != NULL);
    const unsigned char *ustr = (const unsigned char *)str;
    const unsigned char *head = ustr;
    const unsigned char *tail = ustr + len;
    hwire_response_t rsp      = {0};
    int rv                    = 0;

    if (unlikely(*pos >= len)) {
        return (*pos == len && maxlen == 0) ? HWIRE_ELEN : HWIRE_EAGAIN;
    }
    // Adjust the input pointers based on the current position and maximum
    // length.
    ustr += *pos;
    head = ustr;
    if (maxlen < len - *pos) {
        tail = head + maxlen;
    }

SKIP_NEXT_CRLF:
    if (unlikely(ustr >= tail)) {
        return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN : HWIRE_EAGAIN;
    }

    switch (*ustr) {
    case CR:
        if (unlikely(tail - ustr < 2)) {
            return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN :
                                                       HWIRE_EAGAIN;
        } else if (unlikely(ustr[1] != LF)) {
            return HWIRE_EEOL;
        }
        ustr += 2;
        goto SKIP_NEXT_CRLF;

    case LF:
        ustr++;
        goto SKIP_NEXT_CRLF;
    }

    // parse version
    // status-line = HTTP-version SP status-code SP reason-phrase CRLF
    // RFC 7230 3.1.2 / RFC 9112 4: Status Line
    rv = parse_version(&ustr, head, tail, maxlen, &rsp.version);
    if (rv != HWIRE_OK) {
        return rv;
    } else if (ustr >= tail) {
        return ((size_t)(tail - head) >= maxlen) ? HWIRE_ELEN : HWIRE_EAGAIN;
    } else if (*ustr++ != SP) {
        return HWIRE_EVERSION;
    }

    // parse status
    // status-code = 3DIGIT
    // RFC 7230 3.1.2 / RFC 9112 4: Status Code
    rv = parse_status(&ustr, head, tail, maxlen, &rsp.status);
    if (rv != HWIRE_OK) {
        return rv;
    }

    // parse reason
    // reason-phrase = *( HTAB / SP / VCHAR / obs-text )
    // RFC 7230 3.1.2 / RFC 9112 4: Reason Phrase
    rsp.reason.ptr = (const char *)ustr;
    rsp.reason.len = 0;
    rv             = parse_reason(&ustr, head, tail, maxlen, &rsp.reason.len);
    if (rv != HWIRE_OK) {
        return rv;
    }

    // call response callback
    if (ctx->response_cb(ctx, &rsp) != 0) {
        return HWIRE_ECALLBACK;
    }

    // parse headers within the remaining message budget (cumulative total)
    rv = parse_headers(ctx, &ustr, head, tail, maxlen);
    if (rv != HWIRE_OK) {
        return rv;
    }
    *pos += (size_t)(ustr - head);
    return HWIRE_OK;
}

/** @} */ /* end of HTTP Response Parsing Functions */

// EOF
