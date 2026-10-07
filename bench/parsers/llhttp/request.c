/* llhttp request adapter: parse through headers with stack state. */

#include "llhttp.h"

#include <stddef.h>

/* Match header parsers by stopping before message-body processing. */
static int headers_complete(llhttp_t *parser)
{
    (void)parser;
    return HPE_PAUSED;
}

int llhttp_request(void **context, const unsigned char *data, size_t len)
{
    (void)context;
    llhttp_t parser;
    llhttp_settings_t settings;

    llhttp_settings_init(&settings);
    settings.on_headers_complete = headers_complete;
    llhttp_init(&parser, HTTP_REQUEST, &settings);
    return llhttp_execute(&parser, (const char *)data, len) != HPE_PAUSED;
}

/* Plain parsing retains no context. */
void llhttp_context_free(void *context)
{
    (void)context;
}
