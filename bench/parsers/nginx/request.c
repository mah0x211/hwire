/* nginx request adapter: upstream parsing state, without application storage. */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

int nginx_request(void **context, const unsigned char *data, size_t len)
{
    ngx_http_request_t request = {0};
    ngx_buf_t buffer = { .pos = (u_char *)data, .last = (u_char *)data + len };
    ngx_int_t code;

    (void)context;
    code = ngx_http_parse_request_line(&request, &buffer);
    if (code != NGX_OK) {
        return -1;
    }
    do {
        code = ngx_http_parse_header_line(&request, &buffer, 0);
    } while (code == NGX_OK);
    return code == NGX_HTTP_PARSE_HEADER_DONE ? 0 : -1;
}

void nginx_context_free(void *context)
{
    (void)context;
}
