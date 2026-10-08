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
 *  src/hwire.h
 *  lua-net-http
 *  Zero-Allocation HTTP Parser
 */

#ifndef HWIRE_H
#define HWIRE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup hwire Zero-Allocation HTTP Parser
 * @{
 */

/**
 * @name Error Codes
 * @{
 */

/**
 * @brief HWIRE error codes
 *
 * Using enum allows compiler to detect missing cases in switch statements.
 */
typedef enum {
    HWIRE_OK        = 0,   /**< Success */
    HWIRE_EAGAIN    = -1,  /**< More data needed */
    HWIRE_ELEN      = -2,  /**< Length exceeded */
    HWIRE_EMETHOD   = -3,  /**< Unimplemented method */
    HWIRE_EVERSION  = -4,  /**< Unsupported HTTP version */
    HWIRE_EEOL      = -5,  /**< Invalid end-of-line */
    HWIRE_EHDRNAME  = -6,  /**< Invalid header name */
    HWIRE_EHDRVALUE = -7,  /**< Invalid header value */
    HWIRE_EHDRLEN   = -8,  /**< Header length exceeded */
    HWIRE_ESTATUS   = -9,  /**< Invalid status code */
    HWIRE_EILSEQ    = -10, /**< Invalid byte sequence */
    HWIRE_ERANGE    = -11, /**< Value out of range */
    HWIRE_EEXTNAME  = -12, /**< Invalid extension name */
    HWIRE_EEXTVAL   = -13, /**< Invalid extension value or missing EOL */
    HWIRE_ENOBUFS   = -14, /**< Insufficient output buffer space */
    HWIRE_EKEYLEN   = -15, /**< Key length exceeds buffer size */
    HWIRE_ECALLBACK = -16, /**< Callback returned non-zero */
    HWIRE_EURI      = -17  /**< Invalid request-target */
} hwire_code_t;

/** @} */ /* end of Error Codes */

/**
 * @name Maximum Values
 * @{
 */

#define HWIRE_MAX_CHUNKSIZE UINT32_MAX

/** @} */ /* end of Maximum Values */

/**
 * @name Data Structures
 * @{
 */

/**
 * @brief String slice (length + pointer)
 *
 * Points to the input buffer unless an API explicitly supplies a separate
 * output buffer. The caller must keep the referenced buffer valid during use.
 */
typedef struct {
    size_t len;      /**< String length */
    const char *ptr; /**< String pointer (references caller-owned storage) */
} hwire_str_t;

/**
 * @brief Caller-owned output buffer
 *
 * Used for lowercase-converted keys or decoded query parameters.
 */
typedef struct {
    size_t size; /**< Buffer capacity */
    size_t len;  /**< Used length (set by function) */
    char *buf;   /**< User-allocated buffer */
} hwire_buf_t;

/**
 * @brief Key-value pair
 */
typedef struct {
    hwire_str_t key;   /**< Key */
    hwire_str_t value; /**< Value */
} hwire_kv_pair_t;

/**
 * @brief Generic key-value array
 *
 * Can be used for parameters, headers, extensions, etc.
 */
typedef struct {
    hwire_kv_pair_t *items; /**< Array (allocated by caller) */
    uint8_t count;          /**< Number of items in the array */
} hwire_kv_array_t;

/**
 * @brief Parameter (key-value pair alias)
 */
typedef hwire_kv_pair_t hwire_param_t;

/**
 * @brief Query parameter (key-value pair alias)
 */
typedef hwire_kv_pair_t hwire_query_param_t;

/**
 * @brief Header field (key-value pair alias)
 */
typedef hwire_kv_pair_t hwire_header_t;

/**
 * @brief Chunk extension (key-value pair alias)
 */
typedef hwire_kv_pair_t hwire_chunksize_ext_t;

/**
 * @brief HTTP version enumeration
 */
typedef enum {
    HWIRE_HTTP_V10 = 0x0100, /**< HTTP/1.0 */
    HWIRE_HTTP_V11 = 0x0101  /**< HTTP/1.1 */
} hwire_http_version_t;

/**
 * @brief Syntax type of the URI field in an HTTP request line
 */
typedef enum {
    HWIRE_ORIGIN_URI = 0, /**< origin-form request-target */
    HWIRE_ABSOLUTE_URI,   /**< absolute-form request-target */
    HWIRE_AUTHORITY_URI,  /**< authority-form request-target */
    HWIRE_ASTERISK_URI    /**< asterisk-form request-target */
} hwire_uri_type_t;

/**
 * @brief HTTP request structure
 */
typedef struct {
    hwire_str_t method;           /**< Method (references input buffer) */
    hwire_str_t uri;              /**< Complete request-target */
    hwire_http_version_t version; /**< HTTP version */
    hwire_uri_type_t uri_type;    /**< Request-line URI syntax type */
    hwire_str_t scheme;   /**< Scheme without ':'; absent unless absolute */
    hwire_str_t userinfo; /**< Userinfo without '@'; absent if ptr is NULL */
    hwire_str_t host;     /**< Host; IP-literal brackets are retained */
    hwire_str_t port;     /**< Port without ':'; absent if ptr is NULL */
    hwire_str_t path;     /**< Path; absent for authority/asterisk forms */
    hwire_str_t query;    /**< Query without '?'; absent if ptr is NULL */
} hwire_request_t;

/**
 * @brief HTTP response
 */
typedef struct {
    hwire_http_version_t version; /**< HTTP version */
    uint16_t status;              /**< Status code (100-599) */
    hwire_str_t reason; /**< Reason phrase (references input buffer) */
} hwire_response_t;

/**
 * @brief Parser context
 *
 * Holds the user-context pointer, caller-owned output buffers, and callbacks.
 * Allocate on the stack or heap, zero-initialize, then set the required
 * callbacks before passing to parse functions. Set key_lc.buf/size when a
 * parser needs lowercase field names; hwire_parse_query uses qrybuf instead.
 */
typedef struct hwire_ctx_st {
    void *uctx;         /**< User context pointer (not used by the library) */
    hwire_buf_t key_lc; /**< Lowercase key buffer; caller must allocate
                           key_lc.buf and set key_lc.size before parsing */
    hwire_buf_t qrybuf; /**< Decoded query buffer; caller must allocate
                           qrybuf.buf and set qrybuf.size before
                           hwire_parse_query */

    /**
     * Called for each nonempty segment parsed by hwire_parse_query.
     * Key and value reference qrybuf. A missing '=' has value.ptr == NULL;
     * an empty value after '=' has value.ptr != NULL and value.len == 0.
     * @return 0 to continue, non-zero to stop (HWIRE_ECALLBACK)
     */
    int (*query_cb)(struct hwire_ctx_st *ctx, hwire_query_param_t *param);

    /**
     * Called for each parameter parsed by hwire_parse_parameters.
     * @param ctx  Parser context
     * @param param Parsed parameter (key.ptr references input buffer)
     * @return 0 to continue, non-zero to stop (HWIRE_ECALLBACK)
     */
    int (*param_cb)(struct hwire_ctx_st *ctx, hwire_param_t *param);

    /**
     * Called after parsing the chunk-size value.
     * @param ctx  Parser context
     * @param size Parsed chunk size (0 = last chunk)
     * @return 0 to continue, non-zero to stop (HWIRE_ECALLBACK)
     */
    int (*chunksize_cb)(struct hwire_ctx_st *ctx, uint32_t size);

    /**
     * Optional callback called for each chunk extension parsed by
     * hwire_parse_chunksize. When NULL, extensions are still syntax-checked
     * but are not delivered. Set this callback to enforce an extension limit.
     * @param ctx Parser context
     * @param ext Parsed extension (key and value reference input buffer)
     * @return 0 to continue, non-zero to stop (HWIRE_ECALLBACK)
     */
    int (*chunksize_ext_cb)(struct hwire_ctx_st *ctx,
                            hwire_chunksize_ext_t *ext);

    /**
     * Called for each header field parsed by hwire_parse_headers.
     * @param ctx    Parser context (key_lc.buf contains lowercase field name)
     * @param header Parsed header (key.ptr references input buffer)
     * @return 0 to continue, non-zero to stop (HWIRE_ECALLBACK)
     */
    int (*header_cb)(struct hwire_ctx_st *ctx, hwire_header_t *header);

    /**
     * Called after parsing the request line.
     * @param ctx Parser context
     * @param req Parsed request; method, uri, and present request-target
     * components reference the input buffer
     * @return 0 to continue, non-zero to stop (HWIRE_ECALLBACK)
     */
    int (*request_cb)(struct hwire_ctx_st *ctx, hwire_request_t *req);

    /**
     * Called after parsing the status line.
     * @param ctx Parser context
     * @param rsp Parsed response (version, status, reason reference input
     * buffer)
     * @return 0 to continue, non-zero to stop (HWIRE_ECALLBACK)
     */
    int (*response_cb)(struct hwire_ctx_st *ctx, hwire_response_t *rsp);
} hwire_ctx_t;

/** @} */ /* end of Data Structures */

/**
 * @name Character Validation Functions
 * @{
 */

/**
 * @brief Check if a single character is tchar (token character)
 *
 * @param c Character to check
 * @return 1 if character is tchar, 0 otherwise
 *
 * @note This function does NOT modify the character (unlike the internal
 *       TCHAR table which returns lowercase for tchar). Returns 1 if tchar,
 *       0 otherwise.
 */
int hwire_is_tchar(unsigned char c);

/**
 * @brief Check if a single character is vchar (field-content character)
 *
 * @param c Character to check
 * @return 1 if character is vchar (VCHAR or obs-text), 0 otherwise
 */
int hwire_is_vchar(unsigned char c);

/**
 * @brief Check if a single character is fcchar (field-content character
 *        including optional whitespace)
 *
 * Returns 1 if `c` is a field-content character: VCHAR (`0x21–0x7E`),
 * obs-text (`0x80–0xFF`), SP (`0x20`), or HTAB (`0x09`).
 *
 * @param c Character to check
 * @return 1 if character is fcchar, 0 otherwise
 */
int hwire_is_fcchar(unsigned char c);

/**
 * @brief Count consecutive tchar characters
 *
 * Counts the number of consecutive tchar (token) characters from the
 * beginning of str, starting at offset *pos. Updates *pos to the position
 * after the matched characters.
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: end offset (must not be NULL). If the
 * initial offset is greater than or equal to len, returns 0 and leaves it
 * unchanged.
 * @return Number of consecutive tchar characters matched (0 if first char is
 * not tchar)
 */
size_t hwire_parse_tchar(const char *str, size_t len, size_t *pos);

/**
 * @brief Count consecutive vchar characters
 *
 * Counts the number of consecutive vchar (field-content) characters from the
 * beginning of str, starting at offset *pos. Updates *pos to the position
 * after the matched characters.
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: end offset (must not be NULL). If the
 * initial offset is greater than or equal to len, returns 0 and leaves it
 * unchanged.
 * @return Number of consecutive vchar characters matched (0 if first char is
 * not vchar)
 */
size_t hwire_parse_vchar(const char *str, size_t len, size_t *pos);

/**
 * @brief Count consecutive fcchar characters
 *
 * Advances `*pos` past consecutive field-content characters (VCHAR, obs-text,
 * SP, HTAB) starting at `str[*pos]`. Returns the number of characters
 * consumed (`0` if `str[*pos]` is not fcchar).
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: end offset (must not be NULL). If the
 * initial offset is greater than or equal to len, returns 0 and leaves it
 * unchanged.
 * @return Number of consecutive fcchar characters matched (0 if first char is
 * not fcchar)
 */
size_t hwire_parse_fcchar(const char *str, size_t len, size_t *pos);

/** @} */ /* end of Character Validation Functions */

/**
 * @name String Parsing Functions
 * @{
 */

/**
 * @brief Parse a quoted-string
 *
 * @param str Input string (str[*pos] must be DQUOTE, must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: end offset (must not be NULL)
 * @param maxlen Maximum number of bytes examined from the initial *pos; both
 * DQUOTE delimiters count toward the budget
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if available input ends before the maxlen budget is
 * exhausted
 * @return HWIRE_EILSEQ for invalid byte sequence
 * @return HWIRE_ELEN if the quoted-string is incomplete when the maxlen budget
 * is exhausted, including zero available bytes with maxlen equal to zero
 */
int hwire_parse_quoted_string(const char *str, size_t len, size_t *pos,
                              size_t maxlen);

/**
 * @brief Parse parameters from a semicolon-separated list
 *
 * Parses parameters according to RFC 9110 Section 5.6.6:
 *   parameters = *( OWS ";" OWS [ parameter ] )
 *   parameter = parameter-name "=" parameter-value
 *
 * Empty parameters do not invoke param_cb. A trailing semicolon is
 * complete at input end. CR or LF after an empty parameter is left unconsumed.
 * After HWIRE_OK, the caller must check *pos against len and, if input
 * remains, validate the terminator at str[*pos].
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: end offset (must not be NULL). An
 * initial offset greater than len returns HWIRE_EILSEQ unchanged.
 * @param maxlen Maximum number of bytes examined from the initial *pos
 * @param skip_leading_semicolon Non-zero to skip semicolon check for first
 * parameter (0: require leading semicolon, 1: allow first param without
 * semicolon)
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
                           int skip_leading_semicolon);

/**
 * @brief Parse URI query parameters (without the leading '?')
 *
 * Split on literal '&' and the first literal '=' in each nonempty segment.
 * Empty segments are skipped; empty keys and values are preserved. ';' and
 * further '=' are ordinary data. Duplicate keys are delivered in input order.
 * Percent escapes must contain two hex digits. Percent escapes and '+' are
 * decoded into caller-owned storage ('+' becomes SP); the input is unchanged.
 * Encoded delimiters are data, not separators. Decoding is byte-oriented,
 * including %00, with no NUL terminators or Unicode normalization. The
 * caller must keep qrybuf.buf alive while using callback slices. The buffer
 * must not overlap the input. A buffer of at least the raw query length
 * suffices. ctx->qrybuf.len is reset to zero on valid entry and tracks
 * accepted output.
 *
 * @param ctx Context with query_cb and qrybuf.buf/size set (key_lc is unused)
 * @param str Input containing the query (must not be NULL)
 * @param len Number of available bytes in str
 * @param pos Input: query start offset; output: parser position. An initial
 * offset greater than len returns HWIRE_EILSEQ unchanged.
 * @param maxlen Maximum number of query bytes examined from the initial *pos
 *
 * ctx->qrybuf.buf must be non-NULL even for an empty query. Its size may be
 * zero if no decoded bytes are needed.
 * @return HWIRE_OK on success, with *pos == len
 * @return HWIRE_EAGAIN if a percent escape needs more available input before
 * the maxlen budget is exhausted
 * @return HWIRE_EURI for invalid query characters or percent hex digits
 * @return HWIRE_ELEN if the byte budget ends with more input or during a
 * percent escape
 * @return HWIRE_ENOBUFS if the decode buffer is exhausted
 * @return HWIRE_ECALLBACK if query_cb returns non-zero
 * @return HWIRE_EILSEQ if the initial *pos is greater than len
 *
 * On failure within a segment, *pos points to its first byte. A segment
 * ending at the maxlen boundary may be delivered before the outer parser
 * returns HWIRE_ELEN; then *pos points at that boundary. Earlier callback
 * deliveries are not rolled back.
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_query(hwire_ctx_t *ctx, const char *str, size_t len,
                      size_t *pos, size_t maxlen);

/** @} */ /* end of String Parsing Functions */

/**
 * @name HTTP Parsing Functions
 * @{
 */

/**
 * @brief Parse chunk size
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: position after CRLF or LF (must not
 * be NULL); unchanged on failure
 * @param maxlen Maximum line length and number of input bytes examined from
 * the initial *pos; the complete line terminator must fit within this budget
 * @param ctx Parser context (must not be NULL)
 * @return HWIRE_OK on success, CRLF or LF consumed
 * @return HWIRE_EAGAIN if more data is needed before maxlen is reached
 * @return HWIRE_ELEN if the line is incomplete upon reaching maxlen, including
 * zero available bytes with maxlen equal to zero
 * @return HWIRE_ERANGE if chunk size exceeds maxsize
 * @return HWIRE_EILSEQ if byte sequence is illegal
 * @return HWIRE_EEOL if end-of-line terminator is invalid
 * @return HWIRE_EEXTNAME for invalid extension key
 * @return HWIRE_EEXTVAL for invalid extension value or missing EOL
 * @return HWIRE_ECALLBACK if callback returned non-zero
 * @note Line terminators match CR?LF: both CRLF and bare LF are accepted;
 * bare CR is invalid.
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_chunksize(hwire_ctx_t *ctx, const char *str, size_t len,
                          size_t *pos, size_t maxlen);

/**
 * @brief Parse HTTP headers
 *
 * Parses headers until an empty CRLF or LF line is encountered. Calls
 * header_cb for each parsed header.
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: position after the empty CRLF or LF
 * line (must not be NULL); unchanged on failure
 * @param maxlen Maximum total length of the header block from the initial
 * *pos, including the terminating empty line
 * @param ctx Parser context (key_lc must be allocated, header_cb must not be
 * NULL)
 * @return HWIRE_OK on success, empty line consumed
 * @return HWIRE_EAGAIN if input is absent at the start or ends before the
 * header block budget
 * @return HWIRE_EHDRNAME for invalid header name
 * @return HWIRE_EHDRVALUE for invalid header value
 * @return HWIRE_EHDRLEN if the header block is incomplete when maxlen is
 * exhausted
 * @return HWIRE_EEOL if a line terminator is invalid (CR without LF)
 * @return HWIRE_EKEYLEN if key length exceeds ctx->key_lc.size
 * @return HWIRE_ECALLBACK if callback returned non-zero
 * @note Line terminators match CR?LF: both CRLF and bare LF are accepted;
 * bare CR is invalid.
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_headers(hwire_ctx_t *ctx, const char *str, size_t len,
                        size_t *pos, size_t maxlen);

/**
 * @brief Parse HTTP request
 *
 * Parses request line and headers, calling request_cb after request line
 * and header_cb for each header. The request-target form and RFC 3986
 * components are validated and returned as slices into str; uri retains the
 * complete request-target.
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: position after the request (must not
 * be NULL); unchanged on failure
 * @param maxlen Maximum total byte length from the initial *pos (leading empty
 * lines + request-line + header fields and the terminating empty line, all
 * delimiters included)
 * @param ctx Parser context (request_cb and header_cb must not be NULL)
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if input ends before the applicable budget
 * @return HWIRE_EMETHOD for invalid method (not tchar or missing SP)
 * @return HWIRE_EVERSION for invalid HTTP version
 * @return HWIRE_EEOL for invalid end-of-line
 * @return HWIRE_ELEN if leading empty lines or the request-line are incomplete
 * when maxlen is exhausted
 * @return HWIRE_EURI for an invalid request-target form, component, or byte
 * @return HWIRE_EHDRNAME for invalid header field name
 * @return HWIRE_EHDRVALUE for invalid header field value
 * @return HWIRE_EHDRLEN if the header section is incomplete when the remaining
 * maxlen budget is exhausted
 * @return HWIRE_EKEYLEN if key length exceeds ctx->key_lc.size
 * @return HWIRE_ECALLBACK if callback returned non-zero
 * @note Line terminators and leading empty lines match CR?LF: both CRLF and
 * bare LF are accepted; bare CR is invalid.
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_request(hwire_ctx_t *ctx, const char *str, size_t len,
                        size_t *pos, size_t maxlen);

/**
 * @brief Parse HTTP response
 *
 * Parses status line and headers, calling response_cb after status line
 * and header_cb for each header.
 *
 * @param str String to parse (must not be NULL)
 * @param len Number of available input bytes from str[0]
 * @param pos Input: start offset, Output: position after the response (must not
 * be NULL); unchanged on failure
 * @param maxlen Maximum total byte length from the initial *pos (leading empty
 * lines + status-line + header fields and the terminating empty line, all
 * delimiters included)
 * @param ctx Parser context (response_cb and header_cb must not be NULL)
 * @return HWIRE_OK on success
 * @return HWIRE_EAGAIN if input ends before the applicable budget
 * @return HWIRE_ESTATUS for invalid status code
 * @return HWIRE_EVERSION for invalid HTTP version
 * @return HWIRE_EEOL for invalid end-of-line
 * @return HWIRE_EILSEQ for invalid character in reason phrase
 * @return HWIRE_ELEN if leading empty lines or the status-line are incomplete
 * when maxlen is exhausted
 * @return HWIRE_EHDRNAME for invalid header field name
 * @return HWIRE_EHDRVALUE for invalid header field value
 * @return HWIRE_EHDRLEN if the header section is incomplete when the remaining
 * maxlen budget is exhausted
 * @return HWIRE_EKEYLEN if key length exceeds ctx->key_lc.size
 * @return HWIRE_ECALLBACK if callback returned non-zero
 * @note Line terminators and leading empty lines match CR?LF: both CRLF and
 * bare LF are accepted; bare CR is invalid.
 * @note Item limits are enforced by callbacks using caller-owned state.
 * Nonzero callback returns stop parsing with HWIRE_ECALLBACK.
 */
int hwire_parse_response(hwire_ctx_t *ctx, const char *str, size_t len,
                         size_t *pos, size_t maxlen);

/** @} */ /* end of HTTP Parsing Functions */

/** @} */ /* end of hwire */

#ifdef __cplusplus
}
#endif

#endif /* HWIRE_H */
