#include "hwire_table.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    hwire_table_t table;
    hwire_kv_pair_t pairs[4];
    hwire_table_index_t
        index[HWIRE_TABLE_INDEX_BOTH_CAPACITY(4, HWIRE_TABLE_SLOTS_CAP_8N)];
} segment_t;

static void init_slots(segment_t *segment, size_t capacity,
                       hwire_table_mode_t mode,
                       hwire_table_slots_capacity_t slots_capacity)
{
    hwire_table_key_t key;
    hwire_table_key_init(&key, capacity);
    assert(hwire_table_init(&segment->table, &key, mode, segment->pairs,
                            capacity, segment->index,
                            slots_capacity) == HWIRE_TABLE_OK);
}

static void init(segment_t *segment, size_t capacity, hwire_table_mode_t mode)
{
    init_slots(segment, capacity, mode, HWIRE_TABLE_SLOTS_CAP_2N);
}

static void push(hwire_table_t *table, const char *key)
{
    hwire_kv_pair_t pair = {
        .key   = {.ptr = key, .len = strlen(key)},
        .value = {.ptr = key, .len = strlen(key)},
    };
    assert(hwire_table_push(table, &pair) == HWIRE_TABLE_OK);
}

static void test_links(hwire_table_mode_t mode)
{
    segment_t a, b, c, wrong;
    init(&a, 1, mode);
    init_slots(&b, 2, mode, HWIRE_TABLE_SLOTS_CAP_4N);
    init_slots(&c, 4, mode, HWIRE_TABLE_SLOTS_CAP_8N);
    init(&wrong, 1,
         mode == (HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE) ?
             HWIRE_TABLE_CASE_SENSITIVE :
             (HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE));
    assert(a.table.next == NULL && a.table.tail == &a.table);
    assert(hwire_table_link(NULL, &b.table) == HWIRE_TABLE_EINVAL);
    assert(hwire_table_link(&a.table, NULL) == HWIRE_TABLE_EINVAL);
    assert(hwire_table_link(&a.table, &b.table) == HWIRE_TABLE_EINVAL);
    push(&a.table, "one");
    assert(hwire_table_link(&a.table, &a.table) == HWIRE_TABLE_EINVAL);
    assert(hwire_table_link(&a.table, &wrong.table) == HWIRE_TABLE_EINVAL);
    push(&b.table, "old");
    hwire_kv_pair_t old = b.pairs[0];
    assert(hwire_table_link(&a.table, &b.table) == HWIRE_TABLE_OK);
    assert(b.table.len == 0 && memcmp(&old, &b.pairs[0], sizeof(old)) == 0);
    assert(b.table.mask == 2u * HWIRE_TABLE_SLOTS_CAP_4N - 1u);
    assert(memcmp(&a.table.key, &b.table.key, sizeof(a.table.key)) == 0);
    assert(a.table.tail == &b.table && a.table.next == &b.table);
    assert(hwire_table_link(&a.table, &c.table) == HWIRE_TABLE_EINVAL);
    push(&a.table, "two");
    push(&a.table, "three");
    assert(a.table.len == 1 && b.table.len == 2);
    assert(hwire_table_link(&a.table, &b.table) == HWIRE_TABLE_EINVAL);
    assert(hwire_table_link(&a.table, &c.table) == HWIRE_TABLE_OK);
    assert(a.table.tail == &c.table && b.table.next == &c.table);
    assert(c.table.mask == 4u * HWIRE_TABLE_SLOTS_CAP_8N - 1u);
    push(&a.table, "four");
    assert(c.table.len == 1);
    assert(hwire_table_unlink(&a.table) == &b.table);
    assert(a.table.next == &c.table && a.table.tail == &c.table);
    assert(b.table.next == NULL && b.table.tail == &b.table &&
           b.table.len == 2);
    hwire_table_iter_t iter = {.table = NULL};
    assert(hwire_table_iterate(&b.table, &iter) == &b.pairs[0]);
    assert(hwire_table_iterate(&b.table, &iter) == &b.pairs[1]);
    assert(hwire_table_iterate(&b.table, &iter) == NULL);
    assert(hwire_table_unlink(&a.table) == &c.table);
    assert(a.table.next == NULL && a.table.tail == &a.table);
    assert(hwire_table_unlink(&a.table) == NULL);
    assert(hwire_table_unlink(NULL) == NULL);
    assert(hwire_table_link(&a.table, &b.table) == HWIRE_TABLE_OK);
    assert(b.table.len == 0);
    push(&a.table, "reused");
    assert(b.table.len == 1);
    (void)hwire_table_unlink(&a.table);
}

static void test_storage_modes(void)
{
    segment_t a, b, c;
    init(&a, 1, HWIRE_TABLE_CASE_INSENSITIVE);
    init(&b, 1, HWIRE_TABLE_CASE_SENSITIVE);
    init(&c, 1, HWIRE_TABLE_CASE_INSENSITIVE);
    push(&a.table, "Foo");
    push(&b.table, "stale");
    assert(hwire_table_link(&a.table, &b.table) == HWIRE_TABLE_OK);
    assert(b.table.mode == a.table.mode);
    push(&a.table, "FOO");
    assert(hwire_table_get(&a.table, "FOO", 3, NULL) == NULL);
    hwire_table_iter_t iter = {.table = NULL};
    assert(hwire_table_get_ci(&a.table, "foo", 3, &iter) == &a.pairs[0]);
    assert(hwire_table_next(&iter) == NULL);
    assert(hwire_table_next_ci(&iter) == &b.pairs[0]);
    assert(hwire_table_next_ci(&iter) == NULL);
    assert(hwire_table_link(&c.table, &a.table) == HWIRE_TABLE_EINVAL);
    (void)hwire_table_unlink(&a.table);
    init(&a, 1, HWIRE_TABLE_CASE_SENSITIVE);
    init(&b, 1, HWIRE_TABLE_CASE_INSENSITIVE);
    push(&a.table, "Foo");
    assert(hwire_table_link(&a.table, &b.table) == HWIRE_TABLE_OK);
    assert(b.table.mode == a.table.mode);
    push(&a.table, "foo");
    assert(hwire_table_get(&a.table, "foo", 3, NULL) == &b.pairs[0]);
    assert(hwire_table_get_ci(&a.table, "foo", 3, NULL) == NULL);
    {
        const hwire_table_iter_t before = iter;
        assert(hwire_table_get(&a.table, "absent", 6, &iter) == NULL);
        assert(iter.table == before.table && iter.index == before.index &&
               iter.hashes[0] == before.hashes[0] &&
               iter.hashes[1] == before.hashes[1]);
    }
}

static void test_traversal(void)
{
    const hwire_table_mode_t mode =
        HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE;
    segment_t a, b, c;
    init(&a, 2, mode);
    init(&b, 2, mode);
    init(&c, 4, mode);
    push(&a.table, "foo");
    push(&a.table, "Foo");
    assert(hwire_table_link(&a.table, &b.table) == HWIRE_TABLE_OK);
    push(&a.table, "skip");
    push(&a.table, "skip");
    assert(hwire_table_link(&a.table, &c.table) == HWIRE_TABLE_OK);
    push(&a.table, "foo");
    push(&a.table, "Foo");
    push(&a.table, "Foo");
    push(&a.table, "end");
    hwire_kv_pair_t pair = {
        .key = {.ptr = "full", .len = 4}
    };
    assert(hwire_table_push(&a.table, &pair) == HWIRE_TABLE_EFULL);
    hwire_table_iter_t iter = {.table = NULL};
    assert(hwire_table_get(&a.table, "foo", 3, &iter) == &a.pairs[0]);
    assert(hwire_table_next(&iter) == &c.pairs[0]);
    assert(hwire_table_next_ci(&iter) == &c.pairs[1]);
    assert(hwire_table_next(&iter) == &c.pairs[2]);
    assert(hwire_table_next(&iter) == NULL);
    assert(hwire_table_get(&a.table, "foo", 3, &iter) == &a.pairs[0]);
    assert(hwire_table_next_ci(&iter) == &a.pairs[1]);
    assert(hwire_table_next(&iter) == &c.pairs[1]);
    assert(hwire_table_next(&iter) == &c.pairs[2]);
    assert(hwire_table_get_ci(&a.table, "FOO", 3, &iter) == &a.pairs[0]);
    assert(hwire_table_next(&iter) == &c.pairs[0]);
    iter = (hwire_table_iter_t){.table = NULL};
    assert(hwire_table_iterate(&a.table, &iter) == &a.pairs[0]);
    assert(hwire_table_next_ci(&iter) == &a.pairs[1]);
    assert(hwire_table_next(&iter) == &c.pairs[1]);
    assert(hwire_table_iterate(&a.table, &iter) == &c.pairs[2]);
    assert(hwire_table_iterate(&a.table, &iter) == &c.pairs[3]);
    hwire_table_iter_t before = iter;
    assert(hwire_table_iterate(&a.table, &iter) == NULL);
    assert(memcmp(&iter, &before, sizeof(iter)) == 0);
    assert(hwire_table_get(&a.table, "end", 3, NULL) == &c.pairs[3]);
    assert(hwire_table_get(&a.table, "missing", 7, NULL) == NULL);
    (void)hwire_table_unlink(&a.table);
    (void)hwire_table_unlink(&a.table);
}

static void test_end_and_append(void)
{
    segment_t a, b, c;
    const hwire_table_mode_t mode =
        HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE;
    init(&a, 1, mode);
    init(&b, 1, mode);
    init(&c, 4, mode);
    push(&a.table, "foo");
    assert(hwire_table_link(&a.table, &b.table) == HWIRE_TABLE_OK);
    push(&a.table, "other");
    assert(hwire_table_link(&a.table, &c.table) == HWIRE_TABLE_OK);
    hwire_table_iter_t iter = {.table = NULL};
    assert(hwire_table_get(&a.table, "foo", 3, &iter) == &a.pairs[0]);
    hwire_table_iter_t before = iter;
    assert(hwire_table_next_ci(&iter) == NULL);
    assert(memcmp(&before, &iter, sizeof(iter)) == 0);
    push(&a.table, "Foo");
    assert(hwire_table_next_ci(&iter) == &c.pairs[0]);
    push(&a.table, "foo");
    assert(hwire_table_next(&iter) == NULL);
    assert(hwire_table_next_ci(&iter) == &c.pairs[1]);
    assert(hwire_table_next(&iter) == NULL);
    hwire_kv_pair_t empty = {
        .key = {.ptr = NULL, .len = 0}
    };
    assert(hwire_table_push(&a.table, &empty) == HWIRE_TABLE_OK);
    assert(hwire_table_get(&a.table, "", 0, NULL) == &c.pairs[2]);
    (void)hwire_table_unlink(&a.table);
    (void)hwire_table_unlink(&a.table);
}

/* Compare every duplicate chain against insertion order across seven segments,
 * including binary, empty and differently cased keys. */
static void test_reference(void)
{
    enum {
        SEGMENTS     = 7,
        MAX_CAPACITY = 64,
        PAIRS        = 127
    };
    hwire_table_t tables[SEGMENTS];
    hwire_kv_pair_t storage[SEGMENTS][MAX_CAPACITY];
    hwire_table_index_t indexes[SEGMENTS][HWIRE_TABLE_INDEX_BOTH_CAPACITY(
        MAX_CAPACITY, HWIRE_TABLE_SLOTS_CAP_8N)];
    const hwire_str_t keys[] = {
        {.ptr = "foo",    .len = 3},
        {.ptr = "Foo",    .len = 3},
        {.ptr = "FOO",    .len = 3},
        {.ptr = "f\0o",   .len = 3},
        {.ptr = "\xffoo", .len = 3},
        {.ptr = NULL,     .len = 0},
    };
    hwire_kv_pair_t *expected[PAIRS];
    size_t selected[PAIRS];
    hwire_table_key_t key;
    hwire_table_key_init(&key, 987);
    for (size_t i = 0; i < SEGMENTS; ++i) {
        assert(hwire_table_init(
                   &tables[i], &key,
                   HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE,
                   storage[i], (size_t)1 << i, indexes[i],
                   (hwire_table_slots_capacity_t)(2u << (i % 3u))) ==
               HWIRE_TABLE_OK);
    }
    size_t segment = 0;
    for (size_t i = 0; i < PAIRS; ++i) {
        if (tables[0].tail->len == tables[0].tail->capacity) {
            ++segment;
            assert(hwire_table_link(&tables[0], &tables[segment]) ==
                   HWIRE_TABLE_OK);
        }
        selected[i] = (i * 17u + i / 5u) % (sizeof(keys) / sizeof(keys[0]));
        hwire_kv_pair_t pair = {.key = keys[selected[i]]};
        expected[i]          = &tables[segment].entries[tables[segment].len];
        assert(hwire_table_push(&tables[0], &pair) == HWIRE_TABLE_OK);
    }
    for (size_t q = 0; q < sizeof(keys) / sizeof(keys[0]); ++q) {
        for (int ci = 0; ci < 2; ++ci) {
            hwire_table_iter_t iter = {.table = NULL};
            const hwire_kv_pair_t *entry =
                ci ? hwire_table_get_ci(&tables[0], keys[q].ptr, keys[q].len,
                                        &iter) :
                     hwire_table_get(&tables[0], keys[q].ptr, keys[q].len,
                                     &iter);
            for (size_t i = 0; i < PAIRS; ++i) {
                if (selected[i] == q || (ci && q < 3 && selected[i] < 3)) {
                    assert(entry == expected[i]);
                    entry = ci ? hwire_table_next_ci(&iter) :
                                 hwire_table_next(&iter);
                }
            }
            assert(entry == NULL);
        }
    }
    hwire_table_iter_t iter = {.table = NULL};
    for (size_t i = 0; i < PAIRS; ++i) {
        assert(hwire_table_iterate(&tables[0], &iter) == expected[i]);
    }
    assert(hwire_table_iterate(&tables[0], &iter) == NULL);
    for (size_t i = 1; i < SEGMENTS; ++i) {
        assert(hwire_table_unlink(&tables[0]) == &tables[i]);
    }
    assert(tables[0].tail == &tables[0]);
}

/* Reuse the lookup hash when duplicates cross differently sized segments. */
static void test_mixed_capacity_lookup(void)
{
    enum { CAPACITY = 32 };
    hwire_table_t root, child;
    hwire_kv_pair_t root_pairs[CAPACITY], child_pairs[2];
    hwire_table_index_t root_index[HWIRE_TABLE_INDEX_BOTH_CAPACITY(
        CAPACITY, HWIRE_TABLE_SLOTS_CAP_2N)];
    hwire_table_index_t child_index[HWIRE_TABLE_INDEX_BOTH_CAPACITY(
        2, HWIRE_TABLE_SLOTS_CAP_8N)];
    hwire_table_key_t key;
    hwire_table_key_init(&key, 42);
    const hwire_table_mode_t mode =
        HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE;
    assert(hwire_table_init(&root, &key, mode, root_pairs, CAPACITY, root_index,
                            HWIRE_TABLE_SLOTS_CAP_2N) == HWIRE_TABLE_OK);
    assert(hwire_table_init(&child, &key, mode, child_pairs, 2, child_index,
                            HWIRE_TABLE_SLOTS_CAP_8N) == HWIRE_TABLE_OK);
    push(&root, "foo");
    push(&root, "Foo");
    char names[CAPACITY - 2][16];
    for (size_t i = 0; i < CAPACITY - 2u; ++i) {
        int len = snprintf(names[i], sizeof(names[i]), "name-%zu", i);
        assert(len > 0 && (size_t)len < sizeof(names[i]));
        push(&root, names[i]);
    }
    assert(hwire_table_link(&root, &child) == HWIRE_TABLE_OK);
    push(&root, "foo");
    push(&root, "FOO");
    hwire_table_iter_t iter = {.table = NULL};
    assert(hwire_table_get(&root, "foo", 3, &iter) == &root_pairs[0]);
    assert(iter.hashes[0] != 0 && iter.hashes[1] == 0);
    assert(hwire_table_next(&iter) == &child_pairs[0]);
    assert(hwire_table_get_ci(&root, "FOO", 3, &iter) == &root_pairs[0]);
    assert(hwire_table_next_ci(&iter) == &root_pairs[1]);
    assert(hwire_table_next_ci(&iter) == &child_pairs[0]);
    assert(hwire_table_next_ci(&iter) == &child_pairs[1]);
    assert(hwire_table_next_ci(&iter) == NULL);
    assert(hwire_table_get(&root, "FOO", 3, &iter) == &child_pairs[1]);
    assert(iter.hashes[0] != 0 && iter.hashes[1] == 0);
    const hwire_table_iter_t before = iter;
    assert(hwire_table_get_ci(&root, "absent", 6, &iter) == NULL);
    assert(iter.table == before.table && iter.index == before.index &&
               iter.hashes[0] == before.hashes[0] &&
               iter.hashes[1] == before.hashes[1]);
}

/* CI traversal must invalidate exact hashes before crossing another segment. */
static void test_two_hash_cache(void)
{
    segment_t a, b, c;
    const hwire_table_mode_t mode =
        HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE;
    init(&a, 2, mode);
    init(&b, 2, mode);
    init(&c, 2, mode);
    push(&a.table, "Host");
    while (a.table.len < a.table.capacity) {
        push(&a.table, "filler");
    }
    assert(hwire_table_link(&a.table, &b.table) == HWIRE_TABLE_OK);
    push(&a.table, "HOST");
    while (b.table.len < b.table.capacity) {
        push(&a.table, "other");
    }
    assert(hwire_table_link(&a.table, &c.table) == HWIRE_TABLE_OK);
    push(&a.table, "HOST");
    hwire_table_iter_t iter = {.table = NULL};
    assert(hwire_table_get(&a.table, "Host", 4, &iter) == &a.pairs[0]);
    assert(hwire_table_next_ci(&iter) == &b.pairs[0]);
    assert(iter.hashes[0] == 0 && iter.hashes[1] != 0);
    uint64_t ci_hash = iter.hashes[1];
    assert(hwire_table_next(&iter) == &c.pairs[0]);
    assert(iter.hashes[0] != 0 && iter.hashes[1] == ci_hash);
    hwire_table_iter_t expected = {.table = NULL};
    assert(hwire_table_get(&c.table, "HOST", 4, &expected) == &c.pairs[0]);
    assert(iter.hashes[0] == expected.hashes[0]);
    /* A zero cache slot requests recomputation rather than an altered hash. */
    iter = (hwire_table_iter_t){.table = &b.table, .index = 0};
    assert(hwire_table_next(&iter) == &c.pairs[0]);
    assert(iter.hashes[0] == expected.hashes[0]);
}

int main(void)
{
    test_links(HWIRE_TABLE_CASE_SENSITIVE);
    test_links(HWIRE_TABLE_CASE_INSENSITIVE);
    test_links(HWIRE_TABLE_CASE_SENSITIVE | HWIRE_TABLE_CASE_INSENSITIVE);
    test_storage_modes();
    test_traversal();
    test_end_and_append();
    test_reference();
    test_mixed_capacity_lookup();
    test_two_hash_cache();
    puts("multitable tests passed");
    return 0;
}
