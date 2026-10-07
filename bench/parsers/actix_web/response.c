/* The Rust adapter exports the registration ABI directly. */
#include <stddef.h>
int actix_web_response(void **context, const unsigned char *data, size_t len);
