/*
 * hwire adapter (requests) — uniform benchmark entry point (0 = success).
 * The hwire library itself is always compiled from ../../src/hwire.c.
 */

#include "hwire.h"

#include <stddef.h>

static int header_cb(hwire_ctx_t *ctx, hwire_header_t *header)
{
    (void)ctx;
    (void)header;
    return 0;
}

static int request_cb(hwire_ctx_t *ctx, hwire_request_t *req)
{
    (void)ctx;
    (void)req;
    return 0;
}

int hwire_request(const unsigned char *data, size_t len)
{
    hwire_ctx_t ctx = {0};
    size_t pos      = 0;

    ctx.header_cb  = header_cb;
    ctx.request_cb = request_cb;
    return hwire_parse_request(&ctx, (const char *)data, len, &pos,
                               UINT16_MAX, UINT8_MAX) != HWIRE_OK;
}
