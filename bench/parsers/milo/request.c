/* The native adapter exports the registration ABI directly. */
#include <stddef.h>
int milo_request(void **context, const unsigned char *data, size_t len);
void milo_context_free(void *context);
