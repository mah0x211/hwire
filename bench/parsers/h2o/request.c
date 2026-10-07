/*
 * picohttpparser adapter (requests) — uniform benchmark entry point
 * (0 = success). phr_parse_request returns the consumed length on
 * success, -1 on error, -2 when the request is incomplete.
 */

#include "picohttpparser.h"

#include <stddef.h>

int h2o_request(void **context, const unsigned char *data, size_t len)
{
    (void)context;
    const char *method, *path;
    size_t method_len, path_len;
    int minor_version;
    struct phr_header headers[100];
    size_t num_headers = sizeof(headers) / sizeof(headers[0]);

    return phr_parse_request((const char *)data, len, &method, &method_len,
                             &path, &path_len, &minor_version, headers,
                             &num_headers, 0) <= 0;
}

void h2o_context_free(void *context)
{
    (void)context;
}
