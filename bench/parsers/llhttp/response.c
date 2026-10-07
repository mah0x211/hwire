/*
 * llhttp adapter (responses) — uniform benchmark entry point (0 = success).
 */

#include "llhttp.h"

#include <stddef.h>

/* Match header parsers by stopping before message-body processing. */
static int headers_complete(llhttp_t *parser)
{
    (void)parser;
    return HPE_PAUSED;
}

int llhttp_response(void **context, const unsigned char *data, size_t len)
{
    (void)context;
    llhttp_t parser;
    llhttp_settings_t settings;

    llhttp_settings_init(&settings);
    settings.on_headers_complete = headers_complete;
    llhttp_init(&parser, HTTP_RESPONSE, &settings);
    return llhttp_execute(&parser, (const char *)data, len) != HPE_PAUSED;
}
