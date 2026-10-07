/* hwire response adapter: zero is success. */

#include "hwire.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static int parse_header(hwire_ctx_t *ctx, hwire_header_t *header)
{
    (void)ctx;
    (void)header;
    return 0;
}

static int parse_response(hwire_ctx_t *ctx, hwire_response_t *response)
{
    (void)ctx;
    (void)response;
    return 0;
}

int hwire_response(void **context, const unsigned char *data, size_t len)
{
    hwire_ctx_t parser = {0};
    size_t pos         = 0;

    (void)context;
    parser.header_cb   = parse_header;
    parser.response_cb = parse_response;
    return hwire_parse_response(&parser, (const char *)data, len, &pos,
                                UINT16_MAX) != HWIRE_OK;
}
