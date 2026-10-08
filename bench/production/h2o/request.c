/* H2O native header storage; no benchmark-driver dependency. */
#include "h2o.h"
#include "picohttpparser.h"

typedef h2o_req_t h2o_storage_t;

static h2o_storage_t *h2o_storage_new(void)
{
    h2o_storage_t *s = h2o_mem_alloc(sizeof(*s));
    h2o_init_request(s, NULL, NULL);
    return s;
}

/* Follow HTTP/1 init_headers: lowercase names in-place, preserve original
 * spelling, recognize native tokens and keep Host as authority separately. */
static int h2o_store_headers(h2o_storage_t *s,
                                    const struct phr_header *headers, size_t count)
{
    h2o_vector_reserve(&s->pool, &s->headers, count);
    for (size_t i = 0; i < count; i++) {
        const struct phr_header *h = headers + i;
        char original[h->name_len];
        memcpy(original, h->name, h->name_len);
        h2o_strtolower((char *)h->name, h->name_len);
        const h2o_token_t *token = h2o_lookup_token(h->name, h->name_len);
        if (token != NULL && token->flags.is_init_header_special) {
            if (token == H2O_TOKEN_HOST) {
                s->input.authority = h2o_iovec_init(h->value, h->value_len);
            } else if (token == H2O_TOKEN_UPGRADE) {
                s->upgrade = h2o_iovec_init(h->value, h->value_len);
            }
            /* Body framing and Expect handling are outside header storage. */
        } else if (token != NULL) {
            h2o_add_header(&s->pool, &s->headers, token, original,
                           h->value, h->value_len);
        } else {
            h2o_add_header_by_str(&s->pool, &s->headers, h->name,
                                  h->name_len, 0, original, h->value, h->value_len);
        }
    }
    return 0;
}

void h2o_context_free(void *context)
{
    h2o_storage_t *s = context;
    if (s != NULL) {
        h2o_mem_clear_pool(&s->pool);
        /* Do not retain arena-backed chunks beyond this context's lifetime. */
        h2o_mem_clear_recycle(&h2o_mem_pool_allocator, 1);
        free(s);
    }
}

size_t h2o_header_lookup(const void *context, const char *key,
                                     size_t len)
{
    const h2o_storage_t *s = context;
    char name[len];
    memcpy(name, key, len);
    h2o_strtolower(name, len);
    const h2o_token_t *token = h2o_lookup_token(name, len);
    if (token == H2O_TOKEN_HOST) {
        return s->input.authority.base != NULL ? s->input.authority.len + 1 : 0;
    }
    ssize_t index = token != NULL ? h2o_find_header(&s->headers, token, -1)
                                 : h2o_find_header_by_str(&s->headers, name, len, -1);
    return index >= 0 ? s->headers.entries[index].value.len + 1 : 0;
}

/* Tokens are library-global; the lowercase name is immutable query storage. */
typedef struct {
    const h2o_token_t *token;
    size_t len;
    char name[];
} h2o_header_query_t;

void *h2o_header_query_new(const char *key, size_t len)
{
    h2o_header_query_t *query = malloc(sizeof(*query) + len);
    if (query == NULL) {
        return NULL;
    }
    memcpy(query->name, key, len);
    h2o_strtolower(query->name, len);
    query->token = h2o_lookup_token(query->name, len);
    query->len = len;
    return query;
}

void h2o_header_query_free(void *query)
{
    free(query);
}

size_t h2o_header_lookup_prepared(const void *context, const void *prepared)
{
    const h2o_storage_t *s = context;
    const h2o_header_query_t *query = prepared;
    if (query->token == H2O_TOKEN_HOST) {
        return s->input.authority.base != NULL ? s->input.authority.len + 1 : 0;
    }
    ssize_t index = query->token != NULL ?
        h2o_find_header(&s->headers, query->token, -1) :
        h2o_find_header_by_str(&s->headers, query->name, query->len, -1);
    return index >= 0 ? s->headers.entries[index].value.len + 1 : 0;
}

int h2o_request_with_store(void **context, const unsigned char *data,
                                     size_t len, size_t header_capacity)
{
    (void)header_capacity;
    h2o_storage_t *s = h2o_storage_new();
    *context = s;
    struct phr_header headers[H2O_MAX_HEADERS];
    size_t count = H2O_MAX_HEADERS;
    int minor;
    if (phr_parse_request((const char *)data, len,
                          (const char **)&s->input.method.base, &s->input.method.len,
                          (const char **)&s->input.path.base, &s->input.path.len,
                          &minor, headers, &count, 0) <= 0) {
        return -1;
    }
    s->version = 0x100 | (minor != 0);
    return h2o_store_headers(s, headers, count);
}

int h2o_request_with_store_split(void **context, const unsigned char *data,
                                 size_t len, size_t header_capacity,
                                 size_t split_at)
{
    (void)header_capacity;
    h2o_storage_t *s = h2o_storage_new();
    *context = s;
    struct phr_header headers[H2O_MAX_HEADERS];
    size_t count = H2O_MAX_HEADERS;
    int minor;
    if (phr_parse_request((const char *)data, split_at,
                          (const char **)&s->input.method.base, &s->input.method.len,
                          (const char **)&s->input.path.base, &s->input.path.len,
                          &minor, headers, &count, 0) != -2) {
        return -1;
    }
    count = H2O_MAX_HEADERS;
    if (phr_parse_request((const char *)data, len,
                          (const char **)&s->input.method.base, &s->input.method.len,
                          (const char **)&s->input.path.base, &s->input.path.len,
                          &minor, headers, &count, split_at) <= 0) {
        return -1;
    }
    s->version = 0x100 | (minor != 0);
    return h2o_store_headers(s, headers, count);
}
