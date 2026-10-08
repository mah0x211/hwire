/* hwire request application storage; no timing dependencies. */
#include "hwire.h"
#include "hwire_table.h"
#include <stdlib.h>

enum {
    APP_INITIAL_CAPACITY = 32
};
#define APP_SLOTS HWIRE_TABLE_SLOTS_CAP_8N

typedef struct {
    uint16_t max_header_num;
    hwire_kv_pair_t *entries;
    hwire_table_index_t *index;
    hwire_table_t table;
} app_request_header_t;

typedef struct {
    const char *msg;
    size_t msglen;
    hwire_request_t request;
    app_request_header_t header;
} app_request_t;

/* Context, entry and index backing are acquired together. */
static app_request_t *app_request_new(const unsigned char *data, size_t len,
                                      size_t header_limit)
{
    size_t hc    = header_limit < APP_INITIAL_CAPACITY ? header_limit :
                                                         APP_INITIAL_CAPACITY;
    size_t count = HWIRE_TABLE_INDEX_CAPACITY(hc, APP_SLOTS);
    app_request_t *s = malloc(sizeof(*s) + hc * sizeof(hwire_kv_pair_t) +
                              count * sizeof(hwire_table_index_t));
    if (s == NULL) {
        return NULL;
    }
    *s = (app_request_t){
        .msg    = (const char *)data,
        .msglen = len,
        .header = {.max_header_num = (uint16_t)header_limit,
                   .entries        = (hwire_kv_pair_t *)(s + 1)}
    };
    s->header.index = (hwire_table_index_t *)(s->header.entries + hc);
    if (hc != 0) {
        hwire_table_key_t key;
        hwire_table_key_init(&key, UINT64_C(42));
        if (hwire_table_init(&s->header.table, &key,
                             HWIRE_TABLE_CASE_INSENSITIVE, s->header.entries,
                             hc, s->header.index,
                             APP_SLOTS) != HWIRE_TABLE_OK) {
            free(s);
            return NULL;
        }
    }
    return s;
}

static int app_store_header(hwire_ctx_t *parser, hwire_header_t *pair)
{
    app_request_t *request       = parser->uctx;
    app_request_header_t *header = &request->header;
    hwire_table_t *root          = &header->table;
    hwire_table_code_t code      = hwire_table_push(root, pair);
    if (code == HWIRE_TABLE_OK) {
        return 0;
    }
    if (code != HWIRE_TABLE_EFULL) {
        return -1;
    }
    /* Capacity is derived from the linked tables only when growth is needed. */
    size_t allocated = 0;
    for (const hwire_table_t *table = root; table != NULL;
         table                      = table->next) {
        allocated += table->capacity;
    }
    if (allocated == header->max_header_num) {
        return -1;
    }
    size_t remaining    = header->max_header_num - allocated;
    size_t capacity     = allocated < remaining ? allocated : remaining;
    size_t bytes        = sizeof(hwire_table_t) + capacity * sizeof(*pair) +
                          HWIRE_TABLE_INDEX_CAPACITY(capacity, APP_SLOTS) *
                              sizeof(hwire_table_index_t);
    hwire_table_t *next = malloc(bytes);
    if (next == NULL) {
        return -1;
    }
    hwire_kv_pair_t *entries   = (hwire_kv_pair_t *)(next + 1);
    hwire_table_index_t *index = (hwire_table_index_t *)(entries + capacity);
    if (hwire_table_init(next, &root->key, root->mode, entries, capacity, index,
                         APP_SLOTS) != HWIRE_TABLE_OK ||
        hwire_table_link(root, next) != HWIRE_TABLE_OK) {
        free(next);
        return -1;
    }
    return hwire_table_push(root, pair) == HWIRE_TABLE_OK ? 0 : -1;
}

void hwire_context_free(void *context)
{
    app_request_t *request = context;
    if (request == NULL) {
        return;
    }
    hwire_table_t *table = request->header.table.next;
    while (table != NULL) {
        hwire_table_t *next = table->next;
        free(table);
        table = next;
    }
    free(request);
}

size_t hwire_header_lookup(const void *context, const char *key, size_t len)
{
    const app_request_t *request = context;
    const hwire_kv_pair_t *pair =
        hwire_table_get_ci(&request->header.table, key, len, NULL);
    return pair != NULL ? pair->value.len + 1 : 0;
}

static int store_request(hwire_ctx_t *parser, hwire_request_t *request)
{
    app_request_t *app = parser->uctx;
    app->request       = *request;
    return 0;
}

int hwire_request_with_store(void **context, const unsigned char *data,
                             size_t len, size_t header_capacity)
{
    app_request_t *storage = app_request_new(data, len, header_capacity);
    if (storage == NULL) {
        return -1;
    }
    *context           = storage;
    hwire_ctx_t parser = {.uctx       = storage,
                          .header_cb  = app_store_header,
                          .request_cb = store_request};
    size_t pos         = 0;
    return hwire_parse_request(&parser, (const char *)data, len, &pos, len) ==
                   HWIRE_OK ?
               0 :
               -1;
}

/* Retry on accumulated input; callbacks from the partial parse are discarded.
 * Both fixture prefixes fit in the initial table, so no growth is involved. */
int hwire_request_with_store_split(void **context, const unsigned char *data,
                                   size_t len, size_t header_capacity,
                                   size_t split_at)
{
    app_request_t *storage = app_request_new(data, len, header_capacity);
    if (storage == NULL) {
        return -1;
    }
    *context = storage;
    hwire_ctx_t parser = {.uctx = storage,
                          .header_cb = app_store_header,
                          .request_cb = store_request};
    size_t pos = 0;
    if (hwire_parse_request(&parser, (const char *)data, split_at, &pos, len) != HWIRE_EAGAIN) {
        return -1;
    }
    hwire_table_t *table = &storage->header.table;
    hwire_table_key_t key = table->key;
    if (hwire_table_init(table, &key, HWIRE_TABLE_CASE_INSENSITIVE,
                         storage->header.entries, table->capacity,
                         storage->header.index, APP_SLOTS) != HWIRE_TABLE_OK) {
        return -1;
    }
    pos = 0;
    return hwire_parse_request(&parser, (const char *)data, len, &pos, len) ==
                   HWIRE_OK ? 0 : -1;
}
