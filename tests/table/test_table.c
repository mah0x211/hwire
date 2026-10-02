#include "hwire_table.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    CAP = HWIRE_TABLE_CAPACITY(3)
};
static hwire_kv_pair_t entries[CAP];
static hwire_table_index_t index_storage[HWIRE_TABLE_INDEX_CI_CAPACITY(CAP)];
static hwire_table_t table;

static hwire_kv_pair_t pair(const char *key, size_t keylen, const char *value,
                            size_t valuelen)
{
    hwire_kv_pair_t kv = {
        {keylen,   key  },
        {valuelen, value}
    };
    return kv;
}

static const hwire_kv_pair_t *push(hwire_table_t *target,
                                   const hwire_kv_pair_t *kv)
{
    uint16_t entry_index = target->len;
    assert(hwire_table_push(target, kv) == HWIRE_TABLE_OK);
    return &target->entries[entry_index];
}

static void init(void)
{
    hwire_table_key_t key;
    hwire_table_key_init(&key, UINT64_C(12345));
    assert(hwire_table_init(&table, entries, index_storage, CAP, &key, 1) ==
           HWIRE_TABLE_OK);
}

static void test_capacity(void)
{
    assert(HWIRE_TABLE_CAPACITY(0) == 0);
    assert(HWIRE_TABLE_CAPACITY(1) == 1);
    assert(HWIRE_TABLE_CAPACITY(3) == 4);
    assert(HWIRE_TABLE_CAPACITY(512) == 512);
    assert(HWIRE_TABLE_CAPACITY(32768) == 32768);
    assert(HWIRE_TABLE_CAPACITY(32769) == 0);
    assert(sizeof(hwire_table_index_t) == 2u);
    assert(HWIRE_TABLE_INDEX_CAPACITY(CAP) == (size_t)CAP * 4u);
    assert(HWIRE_TABLE_INDEX_CI_CAPACITY(CAP) == (size_t)CAP * 8u);

    hwire_table_key_t key = {
        {1, 2}
    };
    table                        = (hwire_table_t){0};
    table.len                    = UINT16_C(17);
    hwire_table_t table_before   = table;
    entries[0]                   = pair("keep", 4, "value", 5);
    hwire_kv_pair_t entry_before = entries[0];
    index_storage[0]             = UINT16_C(0x5a5a);
    assert(hwire_table_init(&table, entries, index_storage, 3, &key, 1) ==
           HWIRE_TABLE_ECAPACITY);
    assert(hwire_table_init(&table, entries, index_storage, 0, &key, 1) ==
           HWIRE_TABLE_ECAPACITY);
    assert(hwire_table_init(&table, entries, index_storage, 32769, &key, 1) ==
           HWIRE_TABLE_ECAPACITY);
    assert(hwire_table_init(NULL, entries, index_storage, CAP, &key, 1) ==
           HWIRE_TABLE_EINVAL);
    assert(hwire_table_init(&table, NULL, index_storage, CAP, &key, 1) ==
           HWIRE_TABLE_EINVAL);
    assert(hwire_table_init(&table, entries, NULL, CAP, &key, 1) ==
           HWIRE_TABLE_EINVAL);
    assert(hwire_table_init(&table, entries, index_storage, CAP, NULL, 1) ==
           HWIRE_TABLE_EINVAL);
    assert(memcmp(&table, &table_before, sizeof table) == 0);
    assert(memcmp(&entries[0], &entry_before, sizeof entry_before) == 0);
    assert(index_storage[0] == UINT16_C(0x5a5a));
    hwire_table_key_t a, b;
    hwire_table_key_init(&a, 7);
    hwire_table_key_init(&b, 7);
    assert(memcmp(&a, &b, sizeof a) == 0);
    hwire_table_key_init(&b, 8);
    assert(memcmp(&a, &b, sizeof a) != 0);
}

static void test_index_modes_and_initialization(void)
{
    enum {
        N = 4
    };
    const hwire_table_index_t marker = UINT16_C(0x5a5a);
    hwire_kv_pair_t storage[N];
    hwire_kv_pair_t before[N];
    hwire_table_index_t exact_index[HWIRE_TABLE_INDEX_CAPACITY(N) + 2u];
    hwire_table_index_t ci_index[HWIRE_TABLE_INDEX_CI_CAPACITY(N) + 2u];
    hwire_table_key_t key = {
        {11, 29}
    };
    hwire_table_t t;

    for (size_t i = 0; i < N; ++i) {
        storage[i] = pair("unchanged", 9, "value", 5);
        before[i]  = storage[i];
    }
    for (size_t i = 0; i < HWIRE_TABLE_INDEX_CAPACITY(N) + 2u; ++i) {
        exact_index[i] = marker;
    }

    assert(hwire_table_init(&t, storage, exact_index + 1, N, &key, 0) ==
           HWIRE_TABLE_OK);
    assert(t.enabled_ci == 0);
    assert(memcmp(storage, before, sizeof storage) == 0);
    assert(exact_index[0] == marker);
    assert(exact_index[HWIRE_TABLE_INDEX_CAPACITY(N) + 1u] == marker);
    for (size_t i = 0; i < HWIRE_TABLE_INDEX_CAPACITY(N); ++i) {
        assert(exact_index[i + 1u] == 0);
    }

    hwire_kv_pair_t a             = pair("foo", 3, "A", 1);
    hwire_kv_pair_t b             = pair("foo", 3, "B", 1);
    const hwire_kv_pair_t *first  = push(&t, &a);
    const hwire_kv_pair_t *second = push(&t, &b);
    assert(hwire_table_get(&t, "foo", 3) == first);
    assert(hwire_table_next(&t, first) == second);
    assert(hwire_table_next(&t, second) == NULL);
    assert(hwire_table_get_ci(&t, "FOO", 3) == NULL);
    assert(hwire_table_next_ci(&t, first) == NULL);

    for (size_t i = 0; i < HWIRE_TABLE_INDEX_CI_CAPACITY(N) + 2u; ++i) {
        ci_index[i] = marker;
    }
    assert(hwire_table_init(&t, storage, ci_index + 1, N, &key, 42) ==
           HWIRE_TABLE_OK);
    assert(t.enabled_ci == 1);
    assert(ci_index[0] == marker);
    assert(ci_index[HWIRE_TABLE_INDEX_CI_CAPACITY(N) + 1u] == marker);
    for (size_t i = 0; i < HWIRE_TABLE_INDEX_CI_CAPACITY(N); ++i) {
        assert(ci_index[i + 1u] == 0);
    }
}

static void test_chains(void)
{
    init();
    assert(hwire_table_get(&table, "foo", 3) == NULL);
    const hwire_kv_pair_t a   = pair("foo", 3, "A", 1);
    const hwire_kv_pair_t b   = pair("Foo", 3, "B", 1);
    const hwire_kv_pair_t c   = pair("foo", 3, "C", 1);
    const hwire_kv_pair_t d   = pair("bar", 3, NULL, 0);
    const hwire_kv_pair_t *ea = push(&table, &a);
    assert(ea == &entries[0] && ea->key.ptr == a.key.ptr);
    assert(hwire_table_get(&table, "foo", 3) == ea);
    const hwire_kv_pair_t *eb = push(&table, &b);
    const hwire_kv_pair_t *ec = push(&table, &c);
    const hwire_kv_pair_t *ed = push(&table, &d);
    assert(ea == &entries[0] && eb == &entries[1]);
    assert(hwire_table_get(&table, "foo", 3) == ea);
    assert(hwire_table_get(&table, "Foo", 3) == eb);
    assert(hwire_table_get_ci(&table, "FOO", 3) == ea);
    assert(hwire_table_next(&table, ea) == ec);
    assert(hwire_table_next(&table, ec) == NULL);
    assert(hwire_table_next(&table, eb) == NULL);
    assert(hwire_table_next_ci(&table, ea) == eb);
    assert(hwire_table_next_ci(&table, eb) == ec);
    assert(hwire_table_next_ci(&table, ec) == NULL);
    assert(hwire_table_next(&table, NULL) == NULL);
    assert(hwire_table_next_ci(&table, NULL) == NULL);
    assert(hwire_table_get_ci(&table, "BAR", 3) == ed);
    assert(ed->value.ptr == NULL && ed->value.len == 0);
    assert(hwire_table_get(&table, "absent", 6) == NULL);
    assert(hwire_table_get_ci(&table, "absent", 6) == NULL);
    assert(hwire_table_get(NULL, "foo", 3) == NULL);
    assert(hwire_table_get_ci(NULL, "foo", 3) == NULL);
    hwire_table_iter_t it = 0;
    assert(hwire_table_iterate(&table, &it) == ea);
    assert(hwire_table_iterate(&table, &it) == eb);
    assert(hwire_table_iterate(&table, &it) == ec);
    assert(hwire_table_iterate(&table, &it) == ed);
    assert(hwire_table_iterate(&table, &it) == NULL && it == CAP);
    hwire_kv_pair_t overflow = pair("baz", 3, "E", 1);
    assert(hwire_table_push(&table, &overflow) == HWIRE_TABLE_EFULL);
    assert(table.len == CAP);
    assert(hwire_table_get(&table, "foo", 3) == ea);
    init();
    assert(table.len == 0 && hwire_table_get(&table, "foo", 3) == NULL);
}

static void test_binary_and_errors(void)
{
    init();
    const char binary[]          = {'A', 0, 'B'};
    const char lower[]           = {'a', 0, 'b'};
    const char high[]            = {(char)0xc0, 'A'};
    const char high_lower[]      = {(char)0xc0, 'a'};
    hwire_kv_pair_t kv           = pair(NULL, 0, "", 0);
    const hwire_kv_pair_t *empty = push(&table, &kv);
    assert(empty && hwire_table_get(&table, NULL, 0) == empty);
    assert(empty->value.ptr != NULL);
    kv                           = pair(binary, sizeof binary, "v", 1);
    const hwire_kv_pair_t *first = push(&table, &kv);
    assert(first);
    assert(hwire_table_get(&table, binary, 3) == first);
    assert(hwire_table_get(&table, lower, 3) == NULL);
    assert(hwire_table_get_ci(&table, lower, 3) == first);
    assert(hwire_table_get(&table, binary, 2) == NULL);
    kv = pair(high, sizeof high, NULL, 0);
    assert(push(&table, &kv));
    assert(hwire_table_get_ci(&table, high_lower, 2) == &entries[2]);
    kv = pair(lower, sizeof lower, NULL, 0);
    assert(push(&table, &kv));
    assert(hwire_table_next_ci(&table, first) == &entries[3]);
    kv = pair(NULL, 1, NULL, 0);
    assert(hwire_table_push(&table, &kv) == HWIRE_TABLE_EINVAL);
    assert(table.len == 4);
    assert(hwire_table_get(&table, NULL, 1) == NULL);
    assert(hwire_table_get_ci(&table, NULL, 1) == NULL);
}

static void test_full_unique(void)
{
    enum {
        N = 512
    };
    hwire_kv_pair_t storage[N];
    hwire_table_index_t storage_index[HWIRE_TABLE_INDEX_CI_CAPACITY(N)];
    hwire_table_t t;
    hwire_table_key_t key;
    hwire_table_key_init(&key, 19);
    assert(hwire_table_init(&t, storage, storage_index, N, &key, 1) ==
           HWIRE_TABLE_OK);
    char names[N][8];
    for (unsigned i = 0; i < N; ++i) {
        int len = snprintf(names[i], sizeof names[i], "k%04u", i);
        assert(len == 5);
        hwire_kv_pair_t kv = pair(names[i], (size_t)len, NULL, 0);
        assert(push(&t, &kv) == &storage[i]);
    }
    assert(t.len == N);
    for (unsigned i = 0; i < N; ++i) {
        assert(hwire_table_get(&t, names[i], 5) == &storage[i]);
        assert(hwire_table_get_ci(&t, names[i], 5) == &storage[i]);
    }
    hwire_kv_pair_t extra = pair("extra", 5, NULL, 0);
    assert(hwire_table_push(&t, &extra) == HWIRE_TABLE_EFULL);
}

static void test_max_capacity(void)
{
    enum {
        N = 32768
    };
    static hwire_kv_pair_t storage[N];
    static hwire_table_index_t storage_index[HWIRE_TABLE_INDEX_CAPACITY(N)];
    static uint16_t keys[N];
    hwire_table_t t;
    hwire_table_key_t key;

    hwire_table_key_init(&key, 23);
    assert(hwire_table_init(&t, storage, storage_index, N, &key, 0) ==
           HWIRE_TABLE_OK);
    assert(t.mask == UINT16_MAX);

    for (uint32_t i = 0; i < N; ++i) {
        keys[i] = (uint16_t)i;
        hwire_kv_pair_t kv =
            pair((const char *)&keys[i], sizeof keys[i], NULL, 0);
        assert(push(&t, &kv) == &storage[i]);
    }

    assert(t.len == N);
    assert(hwire_table_get(&t, (const char *)&keys[0], sizeof keys[0]) ==
           &storage[0]);
    assert(hwire_table_get(&t, (const char *)&keys[N / 2],
                           sizeof keys[N / 2]) == &storage[N / 2]);
    assert(hwire_table_get(&t, (const char *)&keys[N - 1],
                           sizeof keys[N - 1]) == &storage[N - 1]);
}

static void test_capacity_one_and_invalid_push(void)
{
    hwire_kv_pair_t storage[1];
    hwire_table_index_t storage_index[HWIRE_TABLE_INDEX_CI_CAPACITY(1)];
    hwire_table_t t;
    hwire_table_key_t key = {
        {0, 1}
    };
    assert(hwire_table_init(&t, storage, storage_index, 1, &key, 1) ==
           HWIRE_TABLE_OK);
    hwire_kv_pair_t invalid = pair(NULL, 1, NULL, 0);
    assert(hwire_table_push(NULL, &invalid) == HWIRE_TABLE_EINVAL);
    assert(hwire_table_push(&t, NULL) == HWIRE_TABLE_EINVAL);
    assert(hwire_table_push(&t, &invalid) == HWIRE_TABLE_EINVAL);
    assert(t.len == 0);
    invalid = pair("x", 1, NULL, 1);
    assert(hwire_table_push(&t, &invalid) == HWIRE_TABLE_EINVAL);
    assert(t.len == 0);
    hwire_kv_pair_t good = pair("x", 1, NULL, 0);
    assert(push(&t, &good) == &storage[0]);
    assert(hwire_table_get(&t, "x", 1) == &storage[0]);
    assert(hwire_table_get_ci(&t, "X", 1) == &storage[0]);
    assert(hwire_table_push(&t, &good) == HWIRE_TABLE_EFULL);
    assert(t.len == 1);
    hwire_table_iter_t iter = 0;
    assert(hwire_table_iterate(&t, &iter) == &storage[0]);
    assert(hwire_table_iterate(&t, &iter) == NULL && iter == 1);
    assert(hwire_table_init(&t, storage, storage_index, 1, &key, 1) ==
           HWIRE_TABLE_OK);
    assert(hwire_table_push(&t, &good) == HWIRE_TABLE_OK);
    assert(hwire_table_get(&t, "x", 1) == &storage[0]);
}

static void test_reference_groups(void)
{
    enum {
        N = 128
    };
    hwire_kv_pair_t storage[N];
    hwire_table_index_t storage_index[HWIRE_TABLE_INDEX_CI_CAPACITY(N)];
    hwire_table_t t;
    hwire_table_key_t key = {
        {5, 9}
    };
    char names[N][4];
    assert(hwire_table_init(&t, storage, storage_index, N, &key, 1) ==
           HWIRE_TABLE_OK);
    for (unsigned i = 0; i < N; ++i) {
        names[i][0] = i % 3u == 0 ? 'K' : 'k';
        int length  = snprintf(names[i] + 1, 3, "%02u", i % 23u);
        assert(length == 2);
        hwire_kv_pair_t kv = pair(names[i], 3, NULL, 0);
        assert(push(&t, &kv) == &storage[i]);
    }
    for (unsigned q = 0; q < N; ++q) {
        const hwire_kv_pair_t *exact = hwire_table_get(&t, names[q], 3);
        const hwire_kv_pair_t *ci    = hwire_table_get_ci(&t, names[q], 3);
        for (unsigned j = 0; j < N; ++j) {
            int same_group = memcmp(names[q] + 1, names[j] + 1, 2) == 0;
            if (same_group && names[q][0] == names[j][0]) {
                assert(exact == &storage[j]);
                exact = hwire_table_next(&t, exact);
            }
            if (same_group) {
                assert(ci == &storage[j]);
                ci = hwire_table_next_ci(&t, ci);
            }
        }
        assert(exact == NULL && ci == NULL);
    }
}

static void test_long_keys(void)
{
    init();
    const char mixed[]           = "Content-Type-Long";
    const char lower[]           = "content-type-long";
    const char upper[]           = "CONTENT-TYPE-LONG";
    const size_t len             = sizeof mixed - 1u;
    hwire_kv_pair_t kv           = pair(mixed, len, "A", 1);
    const hwire_kv_pair_t *first = push(&table, &kv);
    assert(hwire_table_get(&table, mixed, len) == first);
    assert(hwire_table_get(&table, lower, len) == NULL);
    assert(hwire_table_get_ci(&table, upper, len) == first);
    kv                            = pair(lower, len, "B", 1);
    const hwire_kv_pair_t *second = push(&table, &kv);
    kv                            = pair(mixed, len, "C", 1);
    const hwire_kv_pair_t *third  = push(&table, &kv);
    assert(hwire_table_get(&table, lower, len) == second);
    assert(hwire_table_get(&table, mixed, len) == first);
    assert(hwire_table_get_ci(&table, lower, len) == first);
    assert(hwire_table_next(&table, first) == third);
    assert(hwire_table_next(&table, third) == NULL);
    assert(hwire_table_next_ci(&table, first) == second);
    assert(hwire_table_next_ci(&table, second) == third);
    assert(hwire_table_next_ci(&table, third) == NULL);
    assert(hwire_table_get(&table, "Content-Type-Lonx", len) == NULL);
    assert(hwire_table_get_ci(&table, "CONTENT-TYPE-LONX", len) == NULL);
}

int main(void)
{
    test_capacity();
    test_index_modes_and_initialization();
    test_chains();
    test_binary_and_errors();
    test_full_unique();
    test_max_capacity();
    test_capacity_one_and_invalid_push();
    test_reference_groups();
    test_long_keys();
    puts("table tests passed");
    return 0;
}
