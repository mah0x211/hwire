/* hwire request adapter: zero is success. */

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

static int parse_request(hwire_ctx_t *ctx, hwire_request_t *request)
{
    (void)ctx;
    (void)request;
    return 0;
}

int hwire_request(void **context, const unsigned char *data, size_t len)
{
    hwire_ctx_t parser = {0};
    size_t pos         = 0;

    (void)context;
    parser.header_cb  = parse_header;
    parser.request_cb = parse_request;
    return hwire_parse_request(&parser, (const char *)data, len, &pos,
                               UINT16_MAX) != HWIRE_OK;
}

void hwire_context_free(void *context)
{
    (void)context;
}
