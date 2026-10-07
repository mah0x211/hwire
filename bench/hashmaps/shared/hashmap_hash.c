/* This translation unit supplies the hwire_table symbols too; do not also
 * link a separate hwire_table.c object. */
#include "hashmap_hash.h"
#include "../../../src/hwire_table.c"
static hwire_table_t hash_state;
void hashmap_hash_seed(uint64_t seed)
{
    hwire_table_key_init(&hash_state.key, seed);
}
uint64_t hashmap_hash_bytes(const char *key, size_t len)
{
    return compute_bytes_hash_cs(&hash_state.key, key, len);
}
uint64_t hashmap_hash_bytes_ci(const char *key, size_t len)
{
    return compute_bytes_hash_ci(&hash_state.key, key, len);
}
const char *hashmap_hash_backend(void)
{
#if defined(HWIRE_TABLE_HAVE_AES)
    return "AES";
#else
    return "SipHash-1-3";
#endif
}
