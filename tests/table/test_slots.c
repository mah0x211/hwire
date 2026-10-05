#include "../../src/hwire_table.c"
#include <assert.h>
#include <stdio.h>

/* Include the implementation to construct collisions at the final hash slot. */
enum {
    PAIRS = 8
};
static const hwire_table_slots_capacity_t slot_capacities[] = {
    HWIRE_TABLE_SLOTS_CAP_2N,
    HWIRE_TABLE_SLOTS_CAP_4N,
    HWIRE_TABLE_SLOTS_CAP_8N,
};
static const hwire_table_mode_t modes[] = {
    HWIRE_TABLE_CASE_SENSITIVE,
    HWIRE_TABLE_CASE_INSENSITIVE,
    HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE,
};

static void test_layout(hwire_table_slots_capacity_t slots_capacity,
                        hwire_table_mode_t mode)
{
    hwire_table_t table;
    hwire_table_key_t key;
    hwire_kv_pair_t entries[PAIRS];
    hwire_kv_pair_t before[PAIRS];
    hwire_table_index_t storage[HWIRE_TABLE_INDEX_BOTH_CAPACITY(
                                    PAIRS, HWIRE_TABLE_SLOTS_CAP_8N) +
                                2u];
    char names[PAIRS + 1][32];
    const hwire_table_index_t marker = UINT16_C(0x5a5a);
    size_t count =
        mode == (HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE) ?
            HWIRE_TABLE_INDEX_BOTH_CAPACITY(PAIRS, slots_capacity) :
            HWIRE_TABLE_INDEX_CAPACITY(PAIRS, slots_capacity);
    size_t expected_slots = PAIRS * (size_t)slots_capacity;
    assert(HWIRE_TABLE_INDEX_FACTOR(slots_capacity) ==
           (size_t)slots_capacity + 2u);
    assert(HWIRE_TABLE_INDEX_BOTH_FACTOR(slots_capacity) ==
           2u * ((size_t)slots_capacity + 2u));
    for (size_t i = 0; i < sizeof(storage) / sizeof(storage[0]); ++i) {
        storage[i] = marker;
    }
    for (size_t i = 0; i < PAIRS; ++i) {
        entries[i] = (hwire_kv_pair_t){
            .key = {.ptr = "preserved", .len = 9}
        };
    }
    memcpy(before, entries, sizeof(before));
    hwire_table_key_init(&key, 42);
    assert(hwire_table_init(&table, &key, mode, entries, PAIRS, storage + 1,
                            slots_capacity) == HWIRE_TABLE_OK);
    assert(table.mask == expected_slots - 1u);
    assert(index_slot_count(&table) == expected_slots);
    assert(index_count(&table) == count);
    assert(memcmp(before, entries, sizeof(before)) == 0);
    for (size_t i = 0; i < count; ++i) {
        assert(storage[i + 1u] == 0);
    }
    assert(storage[0] == marker && storage[count + 1u] == marker);

    unsigned candidate = 0;
    for (size_t i = 0; i <= PAIRS; ++i) {
        do {
            int length = snprintf(names[i], sizeof(names[i]), "collision-%u",
                                  candidate++);
            assert(length > 0 && (size_t)length < sizeof(names[i]));
        } while ((hash_key(&table, names[i], strlen(names[i]), 0) &
                  table.mask) != table.mask);
        if (i == PAIRS) {
            break;
        }
        hwire_kv_pair_t pair = {
            .key   = {.ptr = names[i], .len = strlen(names[i])},
            .value = {.ptr = names[i], .len = strlen(names[i])},
        };
        assert(hwire_table_push(&table, &pair) == HWIRE_TABLE_OK);
        assert(table.entries[i].key.ptr == names[i]);
        size_t pos = i == 0 ? table.mask : i - 1u;
        assert(table.index[pos] == i + 1u);
        if (mode ==
            (HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE)) {
            assert(index_ci_slot_region(&table)[pos] == i + 1u);
        }
    }
    hwire_table_iter_t iter = {.table = NULL};
    for (size_t i = 0; i < PAIRS; ++i) {
        assert(hwire_table_iterate(&table, &iter) == &entries[i]);
        assert(hwire_table_get(&table, names[i], strlen(names[i]), NULL) ==
               ((mode & HWIRE_TABLE_CASE_SENSITIVE) ? &entries[i] : NULL));
        assert(hwire_table_get_ci(&table, names[i], strlen(names[i]), NULL) ==
               ((mode & HWIRE_TABLE_CASE_INSENSITIVE) ? &entries[i] : NULL));
    }
    assert(hwire_table_iterate(&table, &iter) == NULL);
    assert(hwire_table_get(&table, names[PAIRS], strlen(names[PAIRS]), NULL) ==
           NULL);
    assert(hwire_table_get_ci(&table, names[PAIRS], strlen(names[PAIRS]),
                              NULL) == NULL);
    hwire_kv_pair_t extra = {
        .key = {.ptr = "full", .len = 4}
    };
    assert(hwire_table_push(&table, &extra) == HWIRE_TABLE_EFULL);
    assert(storage[0] == marker && storage[count + 1u] == marker);

    assert(hwire_table_init(&table, &table.key, mode, entries, PAIRS,
                            storage + 1, slots_capacity) == HWIRE_TABLE_OK);
    assert(memcmp(&table.key, &key, sizeof(key)) == 0);
    const char *duplicates[] = {"foo", "Foo", "foo"};
    for (size_t i = 0; i < 3u; ++i) {
        hwire_kv_pair_t pair = {
            .key = {.ptr = duplicates[i], .len = 3}
        };
        assert(hwire_table_push(&table, &pair) == HWIRE_TABLE_OK);
    }
    if (mode & HWIRE_TABLE_CASE_SENSITIVE) {
        assert(hwire_table_get(&table, "foo", 3, &iter) == &entries[0]);
        assert(hwire_table_next(&iter) == &entries[2]);
        assert(hwire_table_next(&iter) == NULL);
    }
    if (mode & HWIRE_TABLE_CASE_INSENSITIVE) {
        assert(hwire_table_get_ci(&table, "FOO", 3, &iter) == &entries[0]);
        assert(hwire_table_next_ci(&iter) == &entries[1]);
        assert(hwire_table_next_ci(&iter) == &entries[2]);
        assert(hwire_table_next_ci(&iter) == NULL);
    }
    assert(storage[0] == marker && storage[count + 1u] == marker);
}

static void test_link_reset(hwire_table_slots_capacity_t root_slots,
                            hwire_table_slots_capacity_t child_slots,
                            hwire_table_mode_t mode)
{
    hwire_table_t root, child;
    hwire_table_key_t key;
    hwire_kv_pair_t root_entries[1], child_entries[2];
    hwire_table_index_t root_index[HWIRE_TABLE_INDEX_BOTH_CAPACITY(
        1, HWIRE_TABLE_SLOTS_CAP_8N)];
    hwire_table_index_t child_index[HWIRE_TABLE_INDEX_BOTH_CAPACITY(
                                        2, HWIRE_TABLE_SLOTS_CAP_8N) +
                                    2u];
    hwire_table_mode_t child_mode =
        mode == HWIRE_TABLE_CASE_SENSITIVE   ? HWIRE_TABLE_CASE_INSENSITIVE :
        mode == HWIRE_TABLE_CASE_INSENSITIVE ? HWIRE_TABLE_CASE_SENSITIVE :
                                               mode;
    size_t count =
        mode == (HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE) ?
            HWIRE_TABLE_INDEX_BOTH_CAPACITY(2, child_slots) :
            HWIRE_TABLE_INDEX_CAPACITY(2, child_slots);
    child_index[0] = child_index[count + 1u] = UINT16_C(0x5a5a);
    hwire_table_key_init(&key, 42);
    assert(hwire_table_init(&root, &key, mode, root_entries, 1, root_index,
                            root_slots) == HWIRE_TABLE_OK);
    hwire_kv_pair_t pair = {
        .key = {.ptr = "foo", .len = 3}
    };
    assert(hwire_table_push(&root, &pair) == HWIRE_TABLE_OK);
    hwire_table_key_init(&key, 99);
    assert(hwire_table_init(&child, &key, child_mode, child_entries, 2,
                            child_index + 1, child_slots) == HWIRE_TABLE_OK);
    pair.key = (hwire_str_t){.ptr = "stale", .len = 5};
    assert(hwire_table_push(&child, &pair) == HWIRE_TABLE_OK);
    pair.key = (hwire_str_t){.ptr = "old", .len = 3};
    assert(hwire_table_push(&child, &pair) == HWIRE_TABLE_OK);
    hwire_kv_pair_t before[2];
    memcpy(before, child_entries, sizeof(before));
    assert(hwire_table_link(&root, &child) == HWIRE_TABLE_OK);
    assert(child.mask == (uint32_t)(2u * (size_t)child_slots - 1u));
    assert(child.mode == mode && child.len == 0);
    assert(memcmp(&root.key, &child.key, sizeof(root.key)) == 0);
    assert(memcmp(before, child_entries, sizeof(before)) == 0);
    for (size_t i = 0; i < count; ++i) {
        assert(child_index[i + 1u] == 0);
    }
    assert(child_index[0] == UINT16_C(0x5a5a));
    assert(child_index[count + 1u] == UINT16_C(0x5a5a));
    pair.key = (hwire_str_t){.ptr = "foo", .len = 3};
    assert(hwire_table_push(&root, &pair) == HWIRE_TABLE_OK);
    hwire_table_iter_t iter = {.table = NULL};
    if (mode & HWIRE_TABLE_CASE_SENSITIVE) {
        assert(hwire_table_get(&root, "foo", 3, &iter) == &root_entries[0]);
        assert(hwire_table_next(&iter) == &child_entries[0]);
        assert(hwire_table_next(&iter) == NULL);
    }
    if (mode & HWIRE_TABLE_CASE_INSENSITIVE) {
        assert(hwire_table_get_ci(&root, "FOO", 3, &iter) == &root_entries[0]);
        assert(hwire_table_next_ci(&iter) == &child_entries[0]);
        assert(hwire_table_next_ci(&iter) == NULL);
    }
    assert(hwire_table_get(&root, "stale", 5, NULL) == NULL);
    assert(hwire_table_get_ci(&root, "old", 3, NULL) == NULL);
    assert(hwire_table_unlink(&root) == &child);
    assert(child.mask == (uint32_t)(2u * (size_t)child_slots - 1u));
    assert(child_index[count + 1u] == UINT16_C(0x5a5a));
}

static void test_invalid_slots(void)
{
    const hwire_table_slots_capacity_t invalid[] = {
        (hwire_table_slots_capacity_t)0,  (hwire_table_slots_capacity_t)1,
        (hwire_table_slots_capacity_t)3,  (hwire_table_slots_capacity_t)16,
        (hwire_table_slots_capacity_t)32,
        (hwire_table_slots_capacity_t)-1,
    };
    hwire_table_t table = {.len = 7};
    hwire_table_t before;
    memcpy(&before, &table, sizeof(before));
    hwire_table_key_t key = {
        .words = {1, 2}
    };
    hwire_kv_pair_t entry = {
        .key = {.ptr = "keep", .len = 4}
    };
    hwire_kv_pair_t entry_before = entry;
    hwire_table_index_t index    = UINT16_C(0x5a5a);
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        assert(hwire_table_init(&table, &key, HWIRE_TABLE_CASE_SENSITIVE,
                                &entry, 1, &index,
                                invalid[i]) == HWIRE_TABLE_EINVAL);
        assert(memcmp(&table, &before, sizeof(table)) == 0);
        assert(memcmp(&entry, &entry_before, sizeof(entry)) == 0);
        assert(index == UINT16_C(0x5a5a));
    }
}

static void test_large_masks(void)
{
    enum {
        CAPACITY = 32768
    };
    static hwire_kv_pair_t entries[CAPACITY];
    static hwire_table_index_t storage
        [HWIRE_TABLE_INDEX_CAPACITY(CAPACITY, HWIRE_TABLE_SLOTS_CAP_8N) + 2u];
    static uint16_t keys[CAPACITY];
    hwire_table_t table;
    hwire_table_key_t key;
    hwire_table_key_init(&key, 123);
    for (size_t s = 0; s < sizeof(slot_capacities) / sizeof(slot_capacities[0]);
         ++s) {
        hwire_table_slots_capacity_t slots_capacity = slot_capacities[s];
        size_t count = HWIRE_TABLE_INDEX_CAPACITY(CAPACITY, slots_capacity);
        storage[0] = storage[count + 1u] = UINT16_C(0x5a5a);
        assert(hwire_table_init(&table, &key, HWIRE_TABLE_CASE_SENSITIVE,
                                entries, CAPACITY, storage + 1,
                                slots_capacity) == HWIRE_TABLE_OK);
        assert(table.mask ==
               (uint32_t)(CAPACITY * (size_t)slots_capacity - 1u));
        int used_upper_slots = 0;
        for (uint32_t i = 0; i < CAPACITY; ++i) {
            keys[i]              = (uint16_t)i;
            hwire_kv_pair_t pair = {
                .key = {.ptr = (const char *)&keys[i], .len = sizeof(keys[i])}
            };
            assert(hwire_table_push(&table, &pair) == HWIRE_TABLE_OK);
        }
        for (size_t i = 0; i < CAPACITY; ++i) {
            assert(hwire_table_get(&table, (const char *)&keys[i],
                                   sizeof(keys[i]), NULL) == &entries[i]);
        }
        for (size_t i = (size_t)UINT16_MAX + 1u; i <= (size_t)table.mask; ++i) {
            if (table.index[i] != 0) {
                used_upper_slots = 1;
                break;
            }
        }
        assert(used_upper_slots ==
               (slots_capacity != HWIRE_TABLE_SLOTS_CAP_2N));
        hwire_kv_pair_t extra = {
            .key = {.ptr = "overflow", .len = 8}
        };
        assert(hwire_table_push(&table, &extra) == HWIRE_TABLE_EFULL);
        assert(storage[0] == UINT16_C(0x5a5a));
        assert(storage[count + 1u] == UINT16_C(0x5a5a));
    }
}

int main(void)
{
    for (size_t s = 0; s < sizeof(slot_capacities) / sizeof(slot_capacities[0]);
         ++s) {
        for (size_t m = 0; m < sizeof(modes) / sizeof(modes[0]); ++m) {
            test_layout(slot_capacities[s], modes[m]);
            size_t next = (s + 1u) % (sizeof(slot_capacities) /
                                      sizeof(slot_capacities[0]));
            test_link_reset(slot_capacities[s], slot_capacities[next],
                            modes[m]);
        }
    }
    test_invalid_slots();
    test_large_masks();
    puts("slot capacity tests passed");
    return 0;
}
