/* The native adapter exports the registration ABI directly. */
#include <stddef.h>
int hparse_request(void **context, const unsigned char *data, size_t len);
void hparse_context_free(void *context);
