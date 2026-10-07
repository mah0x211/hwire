/* The Rust adapter exports the registration ABI directly. */
#include <stddef.h>
int actix_web_request(void **context, const unsigned char *data, size_t len);
void actix_web_context_free(void *context);
