/* nginx request pool, native header list and known-header references. */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

enum { NGINX_REQUEST_POOL_SIZE = 4096, NGINX_INITIAL_HEADERS = 20, NGINX_INITIAL_TRAILERS = 4 };

typedef struct {
    ngx_http_request_t request;
    ngx_log_t log;
    size_t limit;
} nginx_storage_t;

extern ngx_hash_t native_header_hash;
void nginx_allocator_init(void);

static nginx_storage_t *nginx_storage_new(size_t limit)
{
    nginx_storage_t *s = malloc(sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    *s = (nginx_storage_t){ .log = { .log_level = NGX_LOG_ERR }, .limit = limit };
    ngx_http_request_t *r = &s->request;
    r->signature = NGX_HTTP_MODULE;
    r->main = r;
    r->count = 1;
    r->method = NGX_HTTP_UNKNOWN;
    r->http_version = NGX_HTTP_VERSION_10;
    r->headers_in.content_length_n = -1;
    r->headers_in.keep_alive_n = -1;
    r->headers_out.content_length_n = -1;
    r->headers_out.last_modified_time = -1;
    r->uri_changes = NGX_HTTP_MAX_URI_CHANGES + 1;
    r->subrequests = NGX_HTTP_MAX_SUBREQUESTS + 1;
    r->http_state = NGX_HTTP_READING_REQUEST_STATE;
    s->request.pool = ngx_create_pool(NGINX_REQUEST_POOL_SIZE, &s->log);
    if (s->request.pool == NULL ||
        ngx_list_init(&r->headers_out.headers, r->pool,
                      NGINX_INITIAL_HEADERS, sizeof(ngx_table_elt_t)) != NGX_OK ||
        ngx_list_init(&r->headers_out.trailers, r->pool,
                      NGINX_INITIAL_TRAILERS, sizeof(ngx_table_elt_t)) != NGX_OK ||
        ngx_list_init(&r->headers_in.headers, r->pool,
                      NGINX_INITIAL_HEADERS, sizeof(ngx_table_elt_t)) != NGX_OK) {
        if (s->request.pool != NULL) {
            ngx_destroy_pool(s->request.pool);
        }
        free(s);
        return NULL;
    }
    return s;
}

/* Native HTTP header population, omitting value validation and routing. */
static int nginx_store_header(nginx_storage_t *s, ngx_http_request_t *request)
{
    if (request->headers_in.count++ >= s->limit) {
        return -1;
    }
    ngx_table_elt_t *h = ngx_list_push(&request->headers_in.headers);
    if (h == NULL) {
        return -1;
    }
    *h = (ngx_table_elt_t){
        .hash = request->header_hash,
        .key = { .len = (size_t)(request->header_name_end - request->header_name_start),
                 .data = request->header_name_start },
        .value = { .len = (size_t)(request->header_end - request->header_start),
                   .data = request->header_start }
    };
    *request->header_name_end = '\0';
    *request->header_end = '\0';
    h->lowcase_key = ngx_pnalloc(request->pool, h->key.len);
    if (h->lowcase_key == NULL) {
        return -1;
    }
    if (h->key.len == request->lowcase_index) {
        ngx_memcpy(h->lowcase_key, request->lowcase_header, h->key.len);
    } else {
        ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
    }
    uintptr_t offset = (uintptr_t)ngx_hash_find(&native_header_hash, h->hash,
                                                h->lowcase_key, h->key.len);
    if (offset != 0) {
        ngx_table_elt_t **slot = (ngx_table_elt_t **)((u_char *)&request->headers_in + offset - 1);
        while (*slot != NULL) {
            slot = &(*slot)->next;
        }
        *slot = h;
    }
    return 0;
}

#include "bin/header_keys.h"
ngx_hash_t native_header_hash;

/* nginx creates its known-header dispatch hash during server configuration. */
static void __attribute__((constructor)) nginx_init_header_hash(void)
{
    /* Darwin does not honor constructor priorities; initialize dependencies explicitly. */
    nginx_allocator_init();
    static ngx_log_t log = { .log_level = NGX_LOG_ERR };
    ngx_pool_t *pool = ngx_create_pool(4096, &log);
    ngx_hash_init_t init = { .hash = &native_header_hash, .key = ngx_hash_key_lc,
        .max_size = 512, .bucket_size = 64, .name = "headers_in_hash", .pool = pool };
    size_t count = sizeof(native_header_keys) / sizeof(native_header_keys[0]);
    for (size_t i = 0; i < count; i++) {
        native_header_keys[i].key_hash = ngx_hash_key_lc(native_header_keys[i].key.data,
                                                       native_header_keys[i].key.len);
    }
    if (ngx_hash_init(&init, native_header_keys, count) != NGX_OK) {
        abort();
    }
}

void nginx_context_free(void *context)
{
    nginx_storage_t *s = context;
    if (s != NULL) {
        ngx_destroy_pool(s->request.pool);
        free(s);
    }
}

size_t nginx_header_lookup(const void *context, const char *key, size_t len)
{
    const nginx_storage_t *s = context;
    u_char name[len];
    ngx_uint_t hash = ngx_hash_strlow(name, (u_char *)key, len);
    size_t offset = (size_t)(uintptr_t)ngx_hash_find(&native_header_hash, hash, name, len);
    if (offset != 0) {
        ngx_table_elt_t *h = *(ngx_table_elt_t *const *)((const u_char *)&s->request.headers_in + offset - 1);
        return h != NULL ? h->value.len + 1 : 0;
    }
    for (const ngx_list_part_t *part = &s->request.headers_in.headers.part;
         part != NULL; part = part->next) {
        const ngx_table_elt_t *headers = part->elts;
        for (ngx_uint_t i = 0; i < part->nelts; i++) {
            if (headers[i].key.len == len &&
                ngx_strncmp(headers[i].lowcase_key, name, len) == 0) {
                return headers[i].value.len + 1;
            }
        }
    }
    return 0;
}

int nginx_request_with_store(void **context, const unsigned char *data,
                             size_t len, size_t header_capacity)
{
    nginx_storage_t *s = nginx_storage_new(header_capacity);
    if (s == NULL) {
        return -1;
    }
    *context = s;
    ngx_http_request_t *request = &s->request;
    ngx_buf_t buffer = { .pos = (u_char *)data, .last = (u_char *)data + len };
    ngx_int_t code = ngx_http_parse_request_line(request, &buffer);
    if (code != NGX_OK) {
        return -1;
    }
    request->request_length = buffer.pos - request->request_start;
    request->request_line = (ngx_str_t){
        .len = (size_t)(request->request_end - request->request_start),
        .data = request->request_start
    };
    request->method_name = (ngx_str_t){
        .len = (size_t)(request->method_end - request->request_start + 1),
        .data = request->request_start
    };
    if (request->http_protocol.data != NULL) {
        request->http_protocol.len = request->request_end - request->http_protocol.data;
    }
    request->uri.len = request->args_start != NULL
        ? (size_t)(request->args_start - 1 - request->uri_start)
        : (size_t)(request->uri_end - request->uri_start);
    if (request->complex_uri || request->quoted_uri || request->empty_path_in_uri) {
        if (request->empty_path_in_uri) {
            request->uri.len++;
        }
        request->uri.data = ngx_pnalloc(request->pool, request->uri.len);
        if (request->uri.data == NULL || ngx_http_parse_complex_uri(request, 1) != NGX_OK) {
            return -1;
        }
    } else {
        request->uri.data = request->uri_start;
    }
    request->unparsed_uri = (ngx_str_t){
        .len = (size_t)(request->uri_end - request->uri_start),
        .data = request->uri_start
    };
    request->valid_unparsed_uri = !request->empty_path_in_uri;
    if (request->uri_ext != NULL) {
        request->exten = (ngx_str_t){
            .len = (size_t)((request->args_start != NULL ? request->args_start - 1 : request->uri_end) - request->uri_ext),
            .data = request->uri_ext
        };
    }
    if (request->args_start != NULL && request->uri_end > request->args_start) {
        request->args = (ngx_str_t){
            .len = (size_t)(request->uri_end - request->args_start),
            .data = request->args_start
        };
    }
    while ((code = ngx_http_parse_header_line(request, &buffer, 0)) == NGX_OK) {
        request->request_length += buffer.pos - request->header_name_start;
        if (nginx_store_header(s, request) != 0) {
            return -1;
        }
    }
    if (code == NGX_HTTP_PARSE_HEADER_DONE) {
        request->request_length += buffer.pos - request->header_name_start;
        request->http_state = NGX_HTTP_PROCESS_REQUEST_STATE;
        return 0;
    }
    return -1;
}
