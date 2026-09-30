#define _GNU_SOURCE

#include "test_helpers.h"

#include <stdint.h>

#if defined(__APPLE__) || defined(__linux__)
# include <sys/mman.h>
# include <unistd.h>
#endif

#if defined(MAP_ANONYMOUS)
# define HWIRE_MAP_ANONYMOUS MAP_ANONYMOUS
#elif defined(MAP_ANON)
# define HWIRE_MAP_ANONYMOUS MAP_ANON
#endif

static hwire_request_t captured_request;
static int request_called = 0;

static int capture_request(hwire_ctx_t *ctx, hwire_request_t *req)
{
    (void)ctx;
    captured_request = *req;
    request_called++;
    return 0;
}

static int parse_request_from(const char *buf, size_t len, size_t *pos,
                              size_t maxlen)
{
    char key_storage[TEST_KEY_SIZE];
    hwire_ctx_t ctx = {
        .key_lc = {.buf = key_storage, .size = sizeof(key_storage), .len = 0},
        .request_cb = capture_request,
        .header_cb  = mock_header_cb
    };
    captured_request = (hwire_request_t){0};
    request_called   = 0;
    return hwire_parse_request(&ctx, buf, len, pos, maxlen);
}

static int parse_request(const char *buf, size_t len, size_t maxlen)
{
    size_t pos = 0;

    return parse_request_from(buf, len, &pos, maxlen);
}

static int slice_eq(hwire_str_t slice, const char *value)
{
    size_t len = strlen(value);

    return slice.ptr != NULL && slice.len == len &&
           memcmp(slice.ptr, value, len) == 0;
}

static int slice_absent(hwire_str_t slice)
{
    return slice.ptr == NULL && slice.len == 0;
}

static int is_regname_byte(unsigned char c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9')) {
        return 1;
    }
    return c == '-' || c == '.' || c == '_' || c == '~' || c == '!' ||
           c == '$' || c == '&' || c == '\'' || c == '(' || c == ')' ||
           c == '*' || c == '+' || c == ',' || c == ';' || c == '=';
}

void test_request_target_forms_and_components(void)
{
    TEST_START("test_request_target_forms_and_components");

    const char *buf = "GET /path?q=?/ HTTP/1.1\r\n\r\n";
    int rv          = parse_request(buf, strlen(buf), 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(request_called, 1);
    ASSERT_EQ(captured_request.uri_type, HWIRE_ORIGIN_URI);
    ASSERT(slice_eq(captured_request.uri, "/path?q=?/"));
    ASSERT(slice_eq(captured_request.path, "/path"));
    ASSERT(slice_eq(captured_request.query, "q=?/"));
    ASSERT(slice_absent(captured_request.scheme));
    ASSERT(slice_absent(captured_request.userinfo));
    ASSERT(slice_absent(captured_request.host));
    ASSERT(slice_absent(captured_request.port));

    buf = "GET /? HTTP/1.1\r\n\r\n";
    rv  = parse_request(buf, strlen(buf), 1024);
    ASSERT_OK(rv);
    ASSERT(slice_eq(captured_request.path, "/"));
    ASSERT(captured_request.query.ptr != NULL);
    ASSERT_EQ(captured_request.query.len, 0);

    buf = "OPTIONS * HTTP/1.1\r\n\r\n";
    rv  = parse_request(buf, strlen(buf), 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(captured_request.uri_type, HWIRE_ASTERISK_URI);
    ASSERT(slice_eq(captured_request.uri, "*"));
    ASSERT(slice_absent(captured_request.scheme));
    ASSERT(slice_absent(captured_request.userinfo));
    ASSERT(slice_absent(captured_request.host));
    ASSERT(slice_absent(captured_request.port));
    ASSERT(slice_absent(captured_request.path));
    ASSERT(slice_absent(captured_request.query));

    buf = "CONNECT [2001:db8::1]:443 HTTP/1.1\r\n\r\n";
    rv  = parse_request(buf, strlen(buf), 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(captured_request.uri_type, HWIRE_AUTHORITY_URI);
    ASSERT(slice_eq(captured_request.uri, "[2001:db8::1]:443"));
    ASSERT(slice_eq(captured_request.host, "[2001:db8::1]"));
    ASSERT(slice_eq(captured_request.port, "443"));
    ASSERT(slice_absent(captured_request.scheme));
    ASSERT(slice_absent(captured_request.userinfo));
    ASSERT(slice_absent(captured_request.path));
    ASSERT(slice_absent(captured_request.query));

    char long_host[255];
    char long_request[600];
    memset(long_host, 'a', sizeof(long_host) - 1);
    long_host[sizeof(long_host) - 1] = '\0';
    int n = snprintf(long_request, sizeof(long_request),
                     "CONNECT %s:443 HTTP/1.1\r\n\r\n", long_host);
    ASSERT(n > 0);
    ASSERT((size_t)n < sizeof(long_request));
    rv = parse_request(long_request, (size_t)n, 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(captured_request.host.len, sizeof(long_host) - 1);
    ASSERT(slice_eq(captured_request.port, "443"));

    buf = "GET http://user:info@[2001:db8::1]:8080/p?q HTTP/1.1\r\n\r\n";
    rv  = parse_request(buf, strlen(buf), 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(captured_request.uri_type, HWIRE_ABSOLUTE_URI);
    ASSERT(slice_eq(captured_request.scheme, "http"));
    ASSERT(slice_eq(captured_request.userinfo, "user:info"));
    ASSERT(slice_eq(captured_request.host, "[2001:db8::1]"));
    ASSERT(slice_eq(captured_request.port, "8080"));
    ASSERT(slice_eq(captured_request.path, "/p"));
    ASSERT(slice_eq(captured_request.query, "q"));

    buf = "GET http://user:12x:a@example.com:8080/p HTTP/1.1\r\n\r\n";
    rv  = parse_request(buf, strlen(buf), 1024);
    ASSERT_OK(rv);
    ASSERT(slice_eq(captured_request.userinfo, "user:12x:a"));
    ASSERT(slice_eq(captured_request.host, "example.com"));
    ASSERT(slice_eq(captured_request.port, "8080"));

    buf = "GET http://@example.com/ HTTP/1.1\r\n\r\n";
    rv  = parse_request(buf, strlen(buf), 1024);
    ASSERT_OK(rv);
    ASSERT(captured_request.userinfo.ptr != NULL);
    ASSERT_EQ(captured_request.userinfo.len, 0);
    ASSERT(slice_eq(captured_request.host, "example.com"));

    buf = "GET urn:example:animal?type=ferret HTTP/1.1\r\n\r\n";
    rv  = parse_request(buf, strlen(buf), 1024);
    ASSERT_OK(rv);
    ASSERT_EQ(captured_request.uri_type, HWIRE_ABSOLUTE_URI);
    ASSERT(slice_eq(captured_request.scheme, "urn"));
    ASSERT(slice_eq(captured_request.path, "example:animal"));
    ASSERT(slice_eq(captured_request.query, "type=ferret"));
    ASSERT(slice_absent(captured_request.host));

    buf = "GET http://example.com HTTP/1.1\r\n\r\n";
    rv  = parse_request(buf, strlen(buf), 1024);
    ASSERT_OK(rv);
    ASSERT(slice_eq(captured_request.host, "example.com"));
    ASSERT(captured_request.path.ptr != NULL);
    ASSERT_EQ(captured_request.path.len, 0);
    ASSERT(slice_absent(captured_request.query));

    buf = "GET http://example.com:? HTTP/1.1\r\n\r\n";
    rv  = parse_request(buf, strlen(buf), 1024);
    ASSERT_OK(rv);
    ASSERT(captured_request.port.ptr != NULL);
    ASSERT_EQ(captured_request.port.len, 0);
    ASSERT(captured_request.path.ptr != NULL);
    ASSERT_EQ(captured_request.path.len, 0);
    ASSERT(captured_request.query.ptr != NULL);
    ASSERT_EQ(captured_request.query.len, 0);

    TEST_END();
}

void test_request_target_method_dispatch(void)
{
    TEST_START("test_request_target_method_dispatch");

    static const struct {
        const char *buf;
        int expected;
    } cases[] = {
        {"GET * HTTP/1.1\r\n\r\n",                        HWIRE_EURI},
        {"CONNECT /path HTTP/1.1\r\n\r\n",                HWIRE_EURI},
        {"CONNECT http://example.com HTTP/1.1\r\n\r\n",   HWIRE_EURI},
        {"OPTIONS /path HTTP/1.1\r\n\r\n",                HWIRE_OK  },
        {"OPTIONS ** HTTP/1.1\r\n\r\n",                   HWIRE_EURI},
        {"OPTIONS *:443 HTTP/1.1\r\n\r\n",                HWIRE_EURI},
        {"GET *:443 HTTP/1.1\r\n\r\n",                    HWIRE_EURI},
        {"GET example:443 HTTP/1.1\r\n\r\n",              HWIRE_OK  },
        {"CONNECT *:443 HTTP/1.1\r\n\r\n",                HWIRE_OK  },
        {"CONNECT :443 HTTP/1.1\r\n\r\n",                 HWIRE_EURI},
        {"CONNECT user@example.com:443 HTTP/1.1\r\n\r\n", HWIRE_EURI},
        {"CONNECT example.com HTTP/1.1\r\n\r\n",          HWIRE_EURI},
        {"CONNECT example.com: HTTP/1.1\r\n\r\n",         HWIRE_EURI},
        {"CONNECT example.com:443/path HTTP/1.1\r\n\r\n", HWIRE_EURI},
        {"CONNECT example.com:443?q=x HTTP/1.1\r\n\r\n",  HWIRE_EURI},
        {"CONNECT example%2Ecom:443 HTTP/1.1\r\n\r\n",    HWIRE_OK  },
        {"CONNECT example%GGcom:443 HTTP/1.1\r\n\r\n",    HWIRE_EURI},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int rv = parse_request(cases[i].buf, strlen(cases[i].buf), 1024);
        ASSERT_EQ(rv, cases[i].expected);
    }

    TEST_END();
}

void test_request_target_absolute_uri_grammar(void)
{
    TEST_START("test_request_target_absolute_uri_grammar");

    static const struct {
        const char *buf;
        const char *scheme;
        const char *userinfo;
        const char *host;
        const char *port;
        const char *path;
        const char *query;
    } valid[] = {
        {"GET x:rootless HTTP/1.1\r\n\r\n", "x", NULL, NULL, NULL,
         "rootless", NULL},
        {"GET x:/absolute HTTP/1.1\r\n\r\n", "x", NULL, NULL, NULL,
         "/absolute", NULL},
        {"GET x: HTTP/1.1\r\n\r\n", "x", NULL, NULL, NULL, "", NULL},
        {"GET x:? HTTP/1.1\r\n\r\n", "x", NULL, NULL, NULL, "", ""},
        {"GET x://host/path HTTP/1.1\r\n\r\n", "x", NULL, "host", NULL,
         "/path", NULL},
        {"GET x:///path HTTP/1.1\r\n\r\n", "x", NULL, "", NULL, "/path",
         NULL},
        {"GET x://?q HTTP/1.1\r\n\r\n", "x", NULL, "", NULL, "", "q"},
        {"GET x://@:/? HTTP/1.1\r\n\r\n", "x", "", "", "", "/", ""},
    };

    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        int rv = parse_request(valid[i].buf, strlen(valid[i].buf), 1024);
        ASSERT_OK(rv);
        ASSERT_EQ(captured_request.uri_type, HWIRE_ABSOLUTE_URI);
        ASSERT(valid[i].scheme != NULL &&
               slice_eq(captured_request.scheme, valid[i].scheme));
        ASSERT(valid[i].userinfo != NULL ?
                   slice_eq(captured_request.userinfo, valid[i].userinfo) :
                   slice_absent(captured_request.userinfo));
        ASSERT(valid[i].host != NULL ?
                   slice_eq(captured_request.host, valid[i].host) :
                   slice_absent(captured_request.host));
        ASSERT(valid[i].port != NULL ?
                   slice_eq(captured_request.port, valid[i].port) :
                   slice_absent(captured_request.port));
        ASSERT(valid[i].path != NULL &&
               slice_eq(captured_request.path, valid[i].path));
        ASSERT(valid[i].query != NULL ?
                   slice_eq(captured_request.query, valid[i].query) :
                   slice_absent(captured_request.query));
    }

    static const char *invalid[] = {
        "GET 1x:path HTTP/1.1\r\n\r\n",
        "GET x^:path HTTP/1.1\r\n\r\n",
        "GET :path HTTP/1.1\r\n\r\n",
        "GET x://host[bad]/ HTTP/1.1\r\n\r\n",
        "GET x://host:12x/ HTTP/1.1\r\n\r\n",
        "GET x://[::1]extra/ HTTP/1.1\r\n\r\n",
        "GET x://a@b@c/ HTTP/1.1\r\n\r\n",
    };

    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        ASSERT_EQ(parse_request(invalid[i], strlen(invalid[i]), 1024),
                  HWIRE_EURI);
    }

    TEST_END();
}

void test_request_target_nonzero_cursor_slices(void)
{
    TEST_START("test_request_target_nonzero_cursor_slices");

    const char *buf =
        "GET /first HTTP/1.1\r\n\r\n"
        "GET http://user@[::1]:80/p?q HTTP/1.1\r\n\r\n";
    size_t first_len  = strlen("GET /first HTTP/1.1\r\n\r\n");
    size_t second_len = strlen(buf) - first_len;
    const char *target = strstr(buf + first_len, "http://");
    size_t pos         = first_len;

    ASSERT(target != NULL);
    ASSERT_OK(parse_request_from(buf, strlen(buf), &pos, second_len));
    ASSERT_EQ(pos, strlen(buf));
    ASSERT_EQ(request_called, 1);
    ASSERT(captured_request.uri.ptr == target);
    ASSERT(captured_request.scheme.ptr == target);
    ASSERT(captured_request.userinfo.ptr == target + strlen("http://"));
    ASSERT(captured_request.host.ptr == strstr(target, "[::1]"));
    ASSERT(captured_request.port.ptr == strstr(target, "80/p"));
    ASSERT(captured_request.path.ptr == strstr(target, "/p?q"));
    ASSERT(captured_request.query.ptr == strstr(target, "q HTTP"));
    ASSERT(slice_eq(captured_request.uri, "http://user@[::1]:80/p?q"));
    ASSERT(slice_eq(captured_request.scheme, "http"));
    ASSERT(slice_eq(captured_request.userinfo, "user"));
    ASSERT(slice_eq(captured_request.host, "[::1]"));
    ASSERT(slice_eq(captured_request.port, "80"));
    ASSERT(slice_eq(captured_request.path, "/p"));
    ASSERT(slice_eq(captured_request.query, "q"));

    TEST_END();
}

void test_request_target_percent_encoding(void)
{
    TEST_START("test_request_target_percent_encoding");

    static const struct {
        const char *buf;
        int expected;
    } cases[] = {
        {"GET /a%20b?q=%2F%3F HTTP/1.1\r\n\r\n",                    HWIRE_OK  },
        {"GET /a% HTTP/1.1\r\n\r\n",                                HWIRE_EURI},
        {"GET /a%2 HTTP/1.1\r\n\r\n",                               HWIRE_EURI},
        {"GET /a%GG HTTP/1.1\r\n\r\n",                              HWIRE_EURI},
        {"GET /path#fragment HTTP/1.1\r\n\r\n",                     HWIRE_EURI},
        {"GET /[host] HTTP/1.1\r\n\r\n",                            HWIRE_EURI},
        {"GET http://user%3Aname@example%2Ecom/p HTTP/1.1\r\n\r\n", HWIRE_OK  },
        {"GET http://a@b@example.com/p HTTP/1.1\r\n\r\n",           HWIRE_EURI},
        {"GET http://[::1]extra/p HTTP/1.1\r\n\r\n",                HWIRE_EURI},
        {"GET http://example.com:abc/p HTTP/1.1\r\n\r\n",           HWIRE_EURI},
        {"GET http://example.com:80:90/p HTTP/1.1\r\n\r\n",         HWIRE_EURI},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int rv = parse_request(cases[i].buf, strlen(cases[i].buf), 1024);
        ASSERT_EQ(rv, cases[i].expected);
    }

    TEST_END();
}

void test_request_target_regname_bytes(void)
{
    TEST_START("test_request_target_regname_bytes");

    static const char prefix[] = "CONNECT a";
    static const char suffix[] = "b:443 HTTP/1.1\r\n\r\n";
    unsigned char buf[sizeof(prefix) + sizeof(suffix)];
    size_t prefix_len = sizeof(prefix) - 1;
    size_t suffix_len = sizeof(suffix) - 1;
    size_t len        = prefix_len + 1 + suffix_len;

    memcpy(buf, prefix, prefix_len);
    memcpy(buf + prefix_len + 1, suffix, suffix_len);
    for (unsigned int value = 0; value <= UINT8_MAX; value++) {
        unsigned char c = (unsigned char)value;
        buf[prefix_len] = c;

        int rv = parse_request((const char *)buf, len, 1024);
        ASSERT_EQ(rv == HWIRE_OK, is_regname_byte(c));
    }

    TEST_END();
}

void test_request_target_ip_literals(void)
{
    TEST_START("test_request_target_ip_literals");

    static const struct {
        const char *target;
        int expected;
    } cases[] = {
        {"[::]:443",                     HWIRE_OK  },
        {"[::1]:443",                    HWIRE_OK  },
        {"[1::]:443",                    HWIRE_OK  },
        {"[1::8]:443",                   HWIRE_OK  },
        {"[1:2::8]:443",                 HWIRE_OK  },
        {"[1:2:3::8]:443",               HWIRE_OK  },
        {"[1:2:3:4::8]:443",             HWIRE_OK  },
        {"[1:2:3:4:5::8]:443",           HWIRE_OK  },
        {"[1:2:3:4:5:6::8]:443",         HWIRE_OK  },
        {"[1:2:3:4:5:6:7::]:443",        HWIRE_OK  },
        {"[1:2:3:4:5:6:7:8]:443",        HWIRE_OK  },
        {"[::192.0.2.1]:443",             HWIRE_OK  },
        {"[1:2:3:4:5::192.0.2.1]:443",   HWIRE_OK  },
        {"[1:2:3:4:5:6:192.0.2.1]:443",  HWIRE_OK  },
        {"[2001:db8::1]:443",            HWIRE_OK  },
        {"[::ffff:192.0.2.1]:443",       HWIRE_OK  },
        {"[::ffff:255.255.255.255]:443", HWIRE_OK  },
        {"[v1.fe80::a]:443",             HWIRE_OK  },
        {"[2001:db8:::1]:443",           HWIRE_EURI},
        {"[1:2:3:4:5:6:7]:443",          HWIRE_EURI},
        {"[1:2:3:4:5:6:7:]:443",         HWIRE_EURI},
        {"[1::2::3]:443",                HWIRE_EURI},
        {"[1:2:3:4:5:6:7:8:9]:443",      HWIRE_EURI},
        {"[1:2:3:4:5:6:7:8::]:443",      HWIRE_EURI},
        {"[1:2:3:4:5:6::192.0.2.1]:443", HWIRE_EURI},
        {"[::ffff:192.00.2.1]:443",      HWIRE_EURI},
        {"[::ffff:192.0.2.256]:443",     HWIRE_EURI},
        {"[::ffff:192.0.2]:443",         HWIRE_EURI},
        {"[::ffff:192.0.2.1.5]:443",     HWIRE_EURI},
        {"[fe80::1%25eth0]:443",         HWIRE_EURI},
        {"2001:db8::1:443",              HWIRE_EURI},
        {"[v.fe80]:443",                 HWIRE_EURI},
    };
    char buf[160];

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int n = snprintf(buf, sizeof(buf), "CONNECT %s HTTP/1.1\r\n\r\n",
                         cases[i].target);
        ASSERT(n > 0);
        ASSERT((size_t)n < sizeof(buf));
        int rv = parse_request(buf, (size_t)n, 1024);
        ASSERT_EQ(rv, cases[i].expected);
    }

    const char *absolute = "GET http://[::ffff:192.0.2.1]/ HTTP/1.1\r\n\r\n";
    ASSERT_OK(parse_request(absolute, strlen(absolute), 1024));
    absolute = "GET http://[::ffff:192.00.2.1]/ HTTP/1.1\r\n\r\n";
    ASSERT_EQ(parse_request(absolute, strlen(absolute), 1024), HWIRE_EURI);

    TEST_END();
}

void test_request_target_ip_literal_differential(void)
{
    TEST_START("test_request_target_ip_literal_differential");

    static const char alphabet[] =
        "0123456789abcdefABCDEF:vV.-_~!$&'()*+,;=gxyz";
    uint32_t state = 0x91123986U;
    char literal[49];
    char connect[128];
    char absolute[160];

    for (size_t sample = 0; sample < 5000; sample++) {
        state      = state * 1664525U + 1013904223U;
        size_t len = (size_t)(state % (uint32_t)sizeof(literal));

        for (size_t i = 0; i < len; i++) {
            state      = state * 1664525U + 1013904223U;
            literal[i] = alphabet[state % (sizeof(alphabet) - 1U)];
        }
        literal[len] = '\0';
        int nconnect = snprintf(connect, sizeof(connect),
                                "CONNECT [%s]:443 HTTP/1.1\r\n\r\n", literal);
        int nabsolute =
            snprintf(absolute, sizeof(absolute),
                     "GET http://[%s]:443/ HTTP/1.1\r\n\r\n", literal);
        ASSERT(nconnect > 0);
        ASSERT(nabsolute > 0);
        ASSERT((size_t)nconnect < sizeof(connect));
        ASSERT((size_t)nabsolute < sizeof(absolute));

        int connect_rv  = parse_request(connect, (size_t)nconnect, 1024);
        int absolute_rv = parse_request(absolute, (size_t)nabsolute, 1024);
        ASSERT_EQ(connect_rv == HWIRE_OK, absolute_rv == HWIRE_OK);
    }

    TEST_END();
}

#if defined(HWIRE_MAP_ANONYMOUS)
static void assert_guarded_target(const char *prefix, unsigned char fill,
                                  unsigned char final_byte)
{
    long page_size = sysconf(_SC_PAGESIZE);
    ASSERT(page_size > 0);
    ASSERT((size_t)page_size >= 64);
    ASSERT((size_t)page_size <= SIZE_MAX / 2);

    size_t map_len        = (size_t)page_size * 2;
    unsigned char *region = mmap(NULL, map_len, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | HWIRE_MAP_ANONYMOUS, -1, 0);
    ASSERT(region != MAP_FAILED);
    ASSERT_EQ(
        mprotect(region + (size_t)page_size, (size_t)page_size, PROT_NONE), 0);

    size_t maxlen      = 64;
    unsigned char *buf = region + (size_t)page_size - maxlen;
    size_t prefix_len  = strlen(prefix);
    ASSERT(prefix_len < maxlen);
    memset(buf, fill, maxlen);
    memcpy(buf, prefix, prefix_len);
    buf[maxlen - 1] = final_byte;

    ASSERT_EQ(parse_request((const char *)buf, maxlen + 3, maxlen), HWIRE_ELEN);

    ASSERT_EQ(mprotect(region + (size_t)page_size, (size_t)page_size,
                       PROT_READ | PROT_WRITE),
              0);
    ASSERT_EQ(munmap(region, map_len), 0);
}
#endif

void test_request_target_guard_page(void)
{
    TEST_START("test_request_target_guard_page");

#if defined(HWIRE_MAP_ANONYMOUS)
    assert_guarded_target("GET /", 'a', 'a');
    assert_guarded_target("GET /path?", 'q', '%');
    assert_guarded_target("GET http://example.com/", 'a', '%');
    assert_guarded_target("CONNECT [v1.", 'a', 'a');
#else
    fprintf(stdout, "[SKIP] anonymous mmap is unavailable\n");
#endif

    TEST_END();
}

void test_request_target_fragmentation_and_maxlen(void)
{
    TEST_START("test_request_target_fragmentation_and_maxlen");

    const char *buf = "GET http://[2001:db8::1]/a%20b?q=? HTTP/1.1\r\n\r\n";
    size_t len      = strlen(buf);

    for (size_t i = 0; i < len; i++) {
        int rv = parse_request(buf, i, 1024);
        ASSERT_EQ(rv, HWIRE_EAGAIN);
    }
    ASSERT_OK(parse_request(buf, len, 1024));

    static const char *authority_messages[] = {
        "CONNECT [2001:db8::192.0.2.1]:443 HTTP/1.1\r\n\r\n",
        "CONNECT [v1.fe80::a]:443 HTTP/1.1\r\n\r\n"};
    for (size_t i = 0;
         i < sizeof(authority_messages) / sizeof(authority_messages[0]); i++) {
        size_t authority_len = strlen(authority_messages[i]);

        for (size_t prefix = 0; prefix < authority_len; prefix++) {
            int rv = parse_request(authority_messages[i], prefix, 1024);
            ASSERT_EQ(rv, HWIRE_EAGAIN);
        }
        ASSERT_OK(parse_request(authority_messages[i], authority_len, 1024));
    }

    const char *line_end = strstr(buf, "\r\n");
    ASSERT(line_end != NULL);
    size_t request_line_len = (size_t)(line_end - buf) + 2;
    ASSERT_EQ(parse_request(buf, len, request_line_len), HWIRE_EHDRLEN);
    ASSERT_EQ(parse_request(buf, len, request_line_len - 1), HWIRE_ELEN);

    static const struct {
        const char *buf;
        const char *prefix;
    } boundary_cases[] = {
        {"GET a: HTTP/1.1\r\n\r\n",                  "GET a"               },
        {"GET x:/path HTTP/1.1\r\n\r\n",             "GET x:/"             },
        {"GET /%20 HTTP/1.1\r\n\r\n",                "GET /%2"             },
        {"OPTIONS * HTTP/1.1\r\n\r\n",               "OPTIONS *"           },
        {"CONNECT [::1]:443 HTTP/1.1\r\n\r\n",       "CONNECT ["           },
        {"CONNECT example.com:443 HTTP/1.1\r\n\r\n", "CONNECT example.com:"},
    };

    for (size_t i = 0; i < sizeof(boundary_cases) / sizeof(boundary_cases[0]);
         i++) {
        size_t boundary = strlen(boundary_cases[i].prefix);

        ASSERT_EQ(parse_request(boundary_cases[i].buf, boundary, boundary + 1),
                  HWIRE_EAGAIN);
        ASSERT_EQ(parse_request(boundary_cases[i].buf,
                                strlen(boundary_cases[i].buf), boundary),
                  HWIRE_ELEN);
    }

    TEST_END();
}

int main(void)
{
    test_request_target_forms_and_components();
    test_request_target_method_dispatch();
    test_request_target_absolute_uri_grammar();
    test_request_target_nonzero_cursor_slices();
    test_request_target_percent_encoding();
    test_request_target_regname_bytes();
    test_request_target_ip_literals();
    test_request_target_ip_literal_differential();
    test_request_target_guard_page();
    test_request_target_fragmentation_and_maxlen();
    print_test_summary();
    return g_tests_failed;
}
