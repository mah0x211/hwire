/* The standalone native allocator needs page-size initialization and a logger.
 * Successful pool/list operations use the unmodified upstream implementation.
 * Only allocation failures invoke this logger; server logging is not linked. */
#include <ngx_config.h>
#include <ngx_core.h>
#include <stdio.h>


void nginx_allocator_init(void)
{
    ngx_cacheline_size = 64;
    ngx_pagesize = (ngx_uint_t)sysconf(_SC_PAGESIZE);
}

void ngx_log_error_core(ngx_uint_t level, ngx_log_t *log, ngx_err_t error,
                         const char *format, ...)
{
    (void)level;
    (void)log;
    fprintf(stderr, "nginx allocation error %d: %s\n", (int)error, format);
}
