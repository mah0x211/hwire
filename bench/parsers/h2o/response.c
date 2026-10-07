/*
 * picohttpparser adapter (responses) — uniform benchmark entry point
 * (0 = success). phr_parse_response returns the consumed length on
 * success, -1 on error, -2 when the response is incomplete.
 */

#include "picohttpparser.h"

#include <stddef.h>

int h2o_response(void **context, const unsigned char *data, size_t len)
{
    (void)context;
    int minor_version, status;
    const char *msg;
    size_t msg_len;
    struct phr_header headers[100];
    size_t num_headers = sizeof(headers) / sizeof(headers[0]);

    return phr_parse_response((const char *)data, len, &minor_version, &status,
                              &msg, &msg_len, headers, &num_headers, 0) <= 0;
}
