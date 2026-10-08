#include "check.h"
#include "hwire_table.h"

enum { SEGMENTS = 4, CAPACITY = 32, KEYS = 16, KEY_BYTES = 256, MAX_OPS = 128 };
typedef struct {
    hwire_table_t table;
    hwire_kv_pair_t entries[CAPACITY];
    hwire_table_index_t index[HWIRE_TABLE_INDEX_BOTH_CAPACITY(CAPACITY, HWIRE_TABLE_SLOTS_CAP_8N)];
    hwire_kv_pair_t model[CAPACITY];
    size_t len;
} segment_t;
typedef struct {
    segment_t segments[SEGMENTS];
    unsigned int chain[SEGMENTS];
    size_t count;
    hwire_table_mode_t mode;
} model_t;

static unsigned char fold(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

static int equal(hwire_str_t a, hwire_str_t b, int ci)
{
    if (a.len != b.len) {
        return 0;
    }
    for (size_t i = 0; i < a.len; i++) {
        unsigned char x = (unsigned char)a.ptr[i];
        unsigned char y = (unsigned char)b.ptr[i];
        if ((ci ? fold(x) : x) != (ci ? fold(y) : y)) {
            return 0;
        }
    }
    return 1;
}

static size_t flatten(model_t *m, const hwire_kv_pair_t **actual,
                      const hwire_kv_pair_t **expected)
{
    size_t count = 0;
    for (size_t c = 0; c < m->count; c++) {
        segment_t *s = &m->segments[m->chain[c]];
        CHECK(s->table.len == s->len);
        for (size_t i = 0; i < s->len; i++) {
            actual[count] = &s->entries[i];
            expected[count++] = &s->model[i];
        }
    }
    return count;
}

static void check_iteration(model_t *m)
{
    const hwire_kv_pair_t *actual[SEGMENTS * CAPACITY];
    const hwire_kv_pair_t *expected[SEGMENTS * CAPACITY];
    size_t n = flatten(m, actual, expected);
    hwire_table_iter_t it = {0};
    for (size_t i = 0; i < n; i++) {
        CHECK(hwire_table_iterate(&m->segments[0].table, &it) == actual[i]);
        CHECK(actual[i]->key.ptr == expected[i]->key.ptr);
        CHECK(actual[i]->key.len == expected[i]->key.len);
        CHECK(actual[i]->value.ptr == expected[i]->value.ptr);
        CHECK(actual[i]->value.len == expected[i]->value.len);
    }
    CHECK(!hwire_table_iterate(&m->segments[0].table, &it));
}

static void check_lookup(model_t *m, hwire_str_t key, unsigned int switches)
{
    const hwire_kv_pair_t *actual[SEGMENTS * CAPACITY];
    const hwire_kv_pair_t *expected[SEGMENTS * CAPACITY];
    size_t n = flatten(m, actual, expected);
    for (unsigned int ci = 0; ci < 2; ci++) {
        unsigned int switch_mode = switches;
        hwire_table_iter_t it = {0};
        hwire_table_mode_t mode = ci ? HWIRE_TABLE_CASE_INSENSITIVE : HWIRE_TABLE_CASE_SENSITIVE;
        size_t first = n;
        if (m->mode & mode) {
            for (size_t i = 0; i < n; i++) {
                if (equal(expected[i]->key, key, (int)ci)) {
                    first = i;
                    break;
                }
            }
        }
        const hwire_kv_pair_t *found = ci ?
            hwire_table_get_ci(&m->segments[0].table, key.ptr, key.len, &it) :
            hwire_table_get(&m->segments[0].table, key.ptr, key.len, &it);
        CHECK(found == (first < n ? actual[first] : NULL));
        /* Change comparison between next calls to exercise both hash caches. */
        size_t current = first;
        unsigned int comparison = ci;
        for (size_t step = 0; current < n && step <= n; step++) {
            if (switch_mode && (step % 2 == 0)) {
                comparison ^= 1;
            }
            mode = comparison ? HWIRE_TABLE_CASE_INSENSITIVE : HWIRE_TABLE_CASE_SENSITIVE;
            size_t next = n;
            if (m->mode & mode) {
                for (size_t i = current + 1; i < n; i++) {
                    if (equal(expected[current]->key, expected[i]->key, (int)comparison)) {
                        next = i;
                        break;
                    }
                }
            }
            found = comparison ? hwire_table_next_ci(&it) : hwire_table_next(&it);
            CHECK(found == (next < n ? actual[next] : NULL));
            if (!found) {
                /* Failure leaves the current cursor available to another mode. */
                if (switch_mode) {
                    switch_mode = 0;
                    comparison ^= 1;
                    continue;
                }
                break;
            }
            current = next;
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 8) {
        return 0;
    }
    model_t m = {.count = 1, .mode = (hwire_table_mode_t)(1 + data[0] % 3)};
    hwire_table_key_t hash_key;
    uint64_t seed = 0;
    memcpy(&seed, data, sizeof(seed));
    hwire_table_key_init(&hash_key, seed);
    for (size_t i = 0; i < SEGMENTS; i++) {
        segment_t *s = &m.segments[i];
        size_t capacity = (size_t)1 << (data[1 + i] % 6);
        hwire_table_slots_capacity_t slots = (hwire_table_slots_capacity_t)(2u << (data[4 + i] % 3));
        hwire_table_key_t segment_key;
        hwire_table_key_init(&segment_key, seed + i);
        hwire_table_mode_t mode = i && m.mode != 3 ? (hwire_table_mode_t)(m.mode ^ 3u) : m.mode;
        CHECK(hwire_table_init(&s->table, &segment_key, mode, s->entries,
                              capacity, s->index, slots) == HWIRE_TABLE_OK);
    }
    char bytes[KEYS][KEY_BYTES] = {{0}};
    hwire_str_t keys[KEYS];
    for (size_t i = 0; i < KEYS; i++) {
        size_t len = data[i % size] % (KEY_BYTES + 1);
        for (size_t j = 0; j < len; j++) {
            bytes[i][j] = (char)data[(8 + i * KEY_BYTES + j) % size];
        }
        keys[i] = (hwire_str_t){.ptr = bytes[i], .len = len};
    }
    keys[0] = (hwire_str_t){.ptr = "Host", .len = 4};
    keys[1] = (hwire_str_t){.ptr = "HOST", .len = 4};
    keys[2] = (hwire_str_t){.ptr = "host", .len = 4};
    keys[3] = (hwire_str_t){.ptr = "", .len = 0};
    keys[4] = (hwire_str_t){.ptr = "a\0B", .len = 3};
    for (size_t offset = 8, op = 0; offset + 1 < size && op < MAX_OPS; offset += 2, op++) {
        unsigned int action = data[offset] % 8;
        hwire_str_t key = keys[data[offset + 1] % KEYS];
        segment_t *tail = &m.segments[m.chain[m.count - 1]];
        if (action <= 1) {
            hwire_kv_pair_t pair = {.key = key, .value = keys[data[offset] % KEYS]};
            int full = tail->len == tail->table.capacity;
            CHECK(hwire_table_push(&m.segments[0].table, &pair) ==
                  (full ? HWIRE_TABLE_EFULL : HWIRE_TABLE_OK));
            if (!full) {
                tail->model[tail->len++] = pair;
            }
        } else if (action <= 3) {
            check_lookup(&m, key, action == 3);
        } else if (action == 4) {
            unsigned int candidate = 1 + data[offset + 1] % (SEGMENTS - 1);
            int linked = 0;
            for (size_t i = 0; i < m.count; i++) {
                linked |= m.chain[i] == candidate;
            }
            if (!linked) {
                int full = tail->len == tail->table.capacity;
                CHECK(hwire_table_link(&m.segments[0].table, &m.segments[candidate].table) ==
                      (full ? HWIRE_TABLE_OK : HWIRE_TABLE_EINVAL));
                if (full) {
                    m.segments[candidate].len = 0;
                    m.chain[m.count++] = candidate;
                }
            }
        } else if (action == 5) {
            hwire_table_t *detached = hwire_table_unlink(&m.segments[0].table);
            CHECK(detached == (m.count > 1 ? &m.segments[m.chain[1]].table : NULL));
            if (m.count > 1) {
                for (size_t i = 1; i + 1 < m.count; i++) {
                    m.chain[i] = m.chain[i + 1];
                }
                m.count--;
            }
        } else if (action == 6 && m.count == 1) {
            segment_t *s = &m.segments[0];
            hwire_table_slots_capacity_t slots = (hwire_table_slots_capacity_t)((s->table.mask + 1u) / s->table.capacity);
            CHECK(hwire_table_init(&s->table, &hash_key, m.mode, s->entries,
                                  s->table.capacity, s->index, slots) == HWIRE_TABLE_OK);
            s->len = 0;
        } else if (action == 7) {
            CHECK(hwire_table_push(&m.segments[0].table, NULL) == HWIRE_TABLE_EINVAL);
        }
        check_iteration(&m);
    }
    for (size_t i = 0; i < KEYS; i++) {
        check_lookup(&m, keys[i], 0);
        check_lookup(&m, keys[i], 1);
    }
    return 0;
}
