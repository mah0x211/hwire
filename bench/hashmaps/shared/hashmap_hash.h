#ifndef HASHMAP_HASH_H
#define HASHMAP_HASH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void hashmap_hash_seed(uint64_t seed);
uint64_t hashmap_hash_bytes(const char *key, size_t len);
uint64_t hashmap_hash_bytes_ci(const char *key, size_t len);
const char *hashmap_hash_backend(void);

#ifdef __cplusplus
}
#endif

#endif
