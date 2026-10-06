#include "hwire_table.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    hwire_table_t table;
    size_t limit;
    size_t allocated_capacity;
    hwire_kv_pair_t entries[];
} hwire_hashmap_t;

static void *hwire_hashmap_create(size_t capacity, hwire_table_mode_t mode,
                                   hwire_table_slots_capacity_t slots_capacity)
{
    hwire_hashmap_t *ctx;
    hwire_table_key_t key;
    size_t entries_size;
    size_t index_size;

    entries_size = capacity * sizeof(ctx->entries[0]);
    index_size =
        HWIRE_TABLE_INDEX_CAPACITY(capacity, slots_capacity) * sizeof(hwire_table_index_t);
    ctx = malloc(sizeof(*ctx) + entries_size + index_size);
    if (ctx == NULL) {
        return NULL;
    }
    hwire_table_key_init(&key, UINT64_C(42));
    if (hwire_table_init(&ctx->table, &key, mode, ctx->entries, capacity,
                         (hwire_table_index_t *)(ctx->entries + capacity),
                         slots_capacity) != HWIRE_TABLE_OK) {
        free(ctx);
        return NULL;
    }
    ctx->limit = capacity;
    ctx->allocated_capacity = capacity;
    return ctx;
}

void *hwire_hashmap_new_exact(size_t capacity)
{
    return hwire_hashmap_create(capacity, HWIRE_TABLE_CASE_SENSITIVE,
                                HWIRE_TABLE_SLOTS_CAP_2N);
}

void *hwire_hashmap_new_ci(size_t capacity)
{
    return hwire_hashmap_create(capacity, HWIRE_TABLE_CASE_INSENSITIVE,
                                HWIRE_TABLE_SLOTS_CAP_2N);
}

static void *hwire_hashmap_create_growth(size_t capacity,
                                        hwire_table_mode_t mode,
                                        hwire_table_slots_capacity_t slots_capacity)
{
    size_t initial = capacity < 32u ? capacity : 32u;
    hwire_hashmap_t *ctx = hwire_hashmap_create(initial, mode, slots_capacity);
    if (ctx) {
        ctx->limit = capacity;
    }
    return ctx;
}

void *hwire_hashmap_new_growth_exact(size_t capacity)
{
    return hwire_hashmap_create_growth(capacity, HWIRE_TABLE_CASE_SENSITIVE,
                                       HWIRE_TABLE_SLOTS_CAP_2N);
}

void *hwire_hashmap_new_growth_ci(size_t capacity)
{
    return hwire_hashmap_create_growth(capacity, HWIRE_TABLE_CASE_INSENSITIVE,
                                       HWIRE_TABLE_SLOTS_CAP_2N);
}

void hwire_hashmap_free(void *ctx)
{
    hwire_hashmap_t *map = ctx;
    hwire_table_t *table = map ? &map->table : NULL;
    while (table) {
        hwire_table_t *next = table->next;
        free(table); /* table is the first member of its allocation. */
        table = next;
    }
}

size_t hwire_hashmap_bytes(const void *ctx)
{
    const hwire_hashmap_t *map = ctx;

    size_t bytes = 0;
    for (const hwire_table_t *t = map ? &map->table : NULL; t; t = t->next) {
        bytes += sizeof(*map) +
                 (size_t)t->capacity * sizeof(map->entries[0]) +
                 HWIRE_TABLE_INDEX_CAPACITY(t->capacity,
                     (t->mask + 1u) / t->capacity) *
                     sizeof(hwire_table_index_t);
    }
    return bytes;
}

int hwire_hashmap_push(void *ctx, const hwire_kv_pair_t *data)
{
    hwire_hashmap_t *map = ctx;

    hwire_table_code_t code = hwire_table_push(&map->table, data);
    if (code == HWIRE_TABLE_OK) {
        return 1;
    }
    if (code != HWIRE_TABLE_EFULL ||
        map->allocated_capacity == map->limit) {
        return 0;
    }
    size_t remaining = map->limit - map->allocated_capacity;
    size_t capacity = map->allocated_capacity < remaining ?
                          map->allocated_capacity : remaining;
    hwire_hashmap_t *block = hwire_hashmap_create(
        capacity, map->table.mode,
        (hwire_table_slots_capacity_t)((map->table.mask + 1u) /
                                      map->table.capacity));
    if (!block) {
        return 0;
    }
    if (hwire_table_link(&map->table, &block->table) != HWIRE_TABLE_OK) {
        free(block);
        return 0;
    }
    map->allocated_capacity += capacity;
    return hwire_table_push(&map->table, data) == HWIRE_TABLE_OK;
}

const hwire_kv_pair_t *hwire_hashmap_get(const void *ctx, const char *key,
                                         size_t len)
{
    const hwire_hashmap_t *map = ctx;
    return hwire_table_get(&map->table, key, len, NULL);
}

const hwire_kv_pair_t *hwire_hashmap_get_ci(const void *ctx, const char *key,
                                            size_t len)
{
    const hwire_hashmap_t *map = ctx;
    return hwire_table_get_ci(&map->table, key, len, NULL);
}

const char *hwire_hashmap_name(void) { return "hwire_table (2N)"; }
size_t hwire_hashmap_growths(const void *ctx)
{
    const hwire_hashmap_t *map = ctx;
    size_t count = 0;
    for (const hwire_table_t *t = map->table.next; t; t = t->next) {
        count++;
    }
    return count;
}

const char *hwire_hashmap_name_4n(void)
{
    return "hwire_table (4N)";
}

void *hwire_hashmap_new_exact_4n(size_t capacity)
{
    return hwire_hashmap_create(capacity, HWIRE_TABLE_CASE_SENSITIVE,
                                HWIRE_TABLE_SLOTS_CAP_4N);
}

void *hwire_hashmap_new_growth_exact_4n(size_t capacity)
{
    return hwire_hashmap_create_growth(capacity, HWIRE_TABLE_CASE_SENSITIVE,
                                       HWIRE_TABLE_SLOTS_CAP_4N);
}

void *hwire_hashmap_new_ci_4n(size_t capacity)
{
    return hwire_hashmap_create(capacity, HWIRE_TABLE_CASE_INSENSITIVE,
                                HWIRE_TABLE_SLOTS_CAP_4N);
}

void *hwire_hashmap_new_growth_ci_4n(size_t capacity)
{
    return hwire_hashmap_create_growth(capacity, HWIRE_TABLE_CASE_INSENSITIVE,
                                       HWIRE_TABLE_SLOTS_CAP_4N);
}

const char *hwire_hashmap_name_8n(void)
{
    return "hwire_table (8N)";
}

void *hwire_hashmap_new_exact_8n(size_t capacity)
{
    return hwire_hashmap_create(capacity, HWIRE_TABLE_CASE_SENSITIVE,
                                HWIRE_TABLE_SLOTS_CAP_8N);
}

void *hwire_hashmap_new_growth_exact_8n(size_t capacity)
{
    return hwire_hashmap_create_growth(capacity, HWIRE_TABLE_CASE_SENSITIVE,
                                       HWIRE_TABLE_SLOTS_CAP_8N);
}

void *hwire_hashmap_new_ci_8n(size_t capacity)
{
    return hwire_hashmap_create(capacity, HWIRE_TABLE_CASE_INSENSITIVE,
                                HWIRE_TABLE_SLOTS_CAP_8N);
}

void *hwire_hashmap_new_growth_ci_8n(size_t capacity)
{
    return hwire_hashmap_create_growth(capacity, HWIRE_TABLE_CASE_INSENSITIVE,
                                       HWIRE_TABLE_SLOTS_CAP_8N);
}

double hwire_hashmap_loadfactor(const void *ctx)
{
    const hwire_hashmap_t *map = ctx;
    size_t keys = 0;
    size_t slots = 0;
    for (const hwire_table_t *t = &map->table; t; t = t->next) {
        keys += t->len;
        slots += (size_t)t->mask + 1u;
    }
    return (double)keys / (double)slots;
}
