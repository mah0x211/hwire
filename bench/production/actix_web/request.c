/* The Rust adapter exports the registration ABI directly. */
#include <stddef.h>
int actix_web_request_with_store(void **context, const unsigned char *data,
                                 size_t len, size_t header_capacity);
void actix_web_context_free(void *context);
size_t actix_web_header_lookup(const void *context, const char *key, size_t len);
void actix_web_store_init(void);
static void __attribute__((constructor)) actix_web_storage_init(void)
{
    actix_web_store_init();
}
