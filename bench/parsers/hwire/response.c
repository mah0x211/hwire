/*
 * hwire adapter (responses) — uniform benchmark entry point (0 = success).
 */

#include "hwire.h"

#include <stddef.h>

static int header_cb(hwire_ctx_t *ctx, hwire_header_t *header)
{
    (void)ctx;
    (void)header;
    return 0;
}

static int response_cb(hwire_ctx_t *ctx, hwire_response_t *rsp)
{
    (void)ctx;
    (void)rsp;
    return 0;
}

int hwire_response(const unsigned char *data, size_t len)
{
    hwire_ctx_t ctx  = {0};
    size_t pos       = 0;

    ctx.header_cb    = header_cb;
    ctx.response_cb  = response_cb;
    return hwire_parse_response(&ctx, (const char *)data, len, &pos,
                                UINT16_MAX, UINT8_MAX) != HWIRE_OK;
}
