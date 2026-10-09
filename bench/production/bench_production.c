/* Standalone native header-processing production benchmark driver. */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "bench_stats.h"

static int check_only;
static int quick;

#include "bench_timer.h"


#include "../data/scenarios/browser_cdn.h"
#include "../data/scenarios/browser_auth.h"
#ifdef BENCH_POOL
#  include "pool.h"
#endif

enum {
    HEADER_CAPACITY = 128
};

typedef int (*parser_fn_t)(void **context, const unsigned char *data,
                           size_t len, size_t header_capacity);
typedef int (*split_parser_fn_t)(void **context, const unsigned char *data,
                                 size_t len, size_t header_capacity, size_t split_at);
typedef void (*context_free_fn_t)(void *context);

typedef struct {
    const char *name;
    parser_fn_t request;
    split_parser_fn_t request_split;
    context_free_fn_t context_free;
    size_t (*header_lookup)(const void *, const char *, size_t);
    void *(*header_query_new)(const char *, size_t);
    size_t (*header_lookup_prepared)(const void *, const void *);
    void (*header_query_free)(void *);
} implementations_t;

#include "implementationlist.c"

typedef struct {
    const char *name;
    const unsigned char *data;
    size_t len;
    int split_input;
} store_fixture_t;

static const store_fixture_t STORE_FIXTURES[] = {
    { .name = "scenario_browser_cdn", .data = MSG_BROWSER_CDN,
      .len = sizeof(MSG_BROWSER_CDN) - 1 },
    { .name = "scenario_browser_auth", .data = MSG_BROWSER_AUTH,
      .len = sizeof(MSG_BROWSER_AUTH) - 1, .split_input = 1 }
};

static double bench_store(const implementations_t *parser, size_t split_at,
                          const store_fixture_t *fixture,
                          size_t header_capacity,
                          size_t iterations)
{
    const uint64_t overhead = measure_timer_overhead(iterations);
    uint64_t elapsed        = 0;
    unsigned char input[fixture->len + 1];

    for (size_t i = 0; i < iterations; i++) {
        void *context  = NULL;
        memcpy(input, fixture->data, fixture->len);
        input[fixture->len] = 0;
#ifdef BENCH_POOL
        parser_pool_begin();
#endif
        uint64_t start = now_ns();
        int result = split_at != 0 ?
            parser->request_split(&context, input, fixture->len, header_capacity, split_at) :
            parser->request(&context, input, fixture->len, header_capacity);

        elapsed += now_ns() - start;
        parser->context_free(context);
#ifdef BENCH_POOL
        parser_pool_end();
#endif
        if (result != 0) {
            return -1.0;
        }
    }
    if (elapsed > overhead) {
        elapsed -= overhead;
    } else {
        elapsed = 0;
    }
    return (double)elapsed / (double)iterations;
}

static int bench_store_scenario(FILE *out, const implementations_t *parser,
                                const store_fixture_t *fixture, size_t split_at,
                                const char *operation)
{
    const size_t warmup = 100;
    size_t iterations;
    bench_stats_t stats = {0};
    uint64_t start;
    double elapsed;

    /* Initialize lazy runtime/TLS storage before activating the request arena.
     * Such storage must outlive a request, unlike the timed header context. */
    {
        unsigned char input[fixture->len + 1];
        memcpy(input, fixture->data, fixture->len);
        void *context = NULL;
        input[fixture->len] = 0;
        int result = split_at != 0 ?
            parser->request_split(&context, input, fixture->len, HEADER_CAPACITY, split_at) :
            parser->request(&context, input, fixture->len, HEADER_CAPACITY);
        if (result != 0) {
            parser->context_free(context);
            return -1;
        }
        parser->context_free(context);
        if (check_only) {
            return 0;
        }
    }
    start = now_ns();
    if (bench_store(parser, split_at, fixture, HEADER_CAPACITY,
                    warmup) < 0.0) {
        return -1;
    }
    elapsed = (double)(now_ns() - start);
    iterations = sample_iterations(elapsed, warmup, quick);
    do {
        double sample = bench_store(parser, split_at, fixture,
                                     HEADER_CAPACITY,
                                     iterations);
        if (sample < 0.0) {
            return -1;
        }
        bench_stats_add(&stats, sample);
    } while (quick ? stats.count < 3 : !bench_stats_done(&stats));
    fprintf(out, "%s,%s,request,%zu,%u,%zu,%zu,%.3f,%.3f,%.6f,%s\n",
            fixture->name, operation, fixture->len, HEADER_CAPACITY,
            stats.count, iterations, stats.mean, bench_stats_stddev(&stats),
            bench_stats_rciw(&stats),
#ifdef BENCH_POOL
            "preallocated"
#else
            "allocated"
#endif
    );
    fprintf(stderr, "%s/%s/%s: %zu samples, RCIW %.2f%%\n", parser->name,
            fixture->name, operation, stats.count, 100.0 * bench_stats_rciw(&stats));
    return 0;
}

/* Search workload is identical across representations, using conventional header names.
 * The input and complete storage context are prepared before lookup timing. */
typedef struct {
    const char *key;
    size_t len;
} lookup_key_t;

enum { LOOKUP_KEY_COUNT = 6 };
typedef struct {
    const char *operation;
    lookup_key_t keys[LOOKUP_KEY_COUNT];
} lookup_case_t;

static const lookup_case_t LOOKUP_CASES[] = {
    { "hit", {
        { "Host", 4 }, { "Accept", 6 }, { "Cookie", 6 },
        { "User-Agent", 10 }, { "Connection", 10 }, { "Referer", 7 }
    } },
    { "hit_unknown", {
        { "Sec-Fetch-Site", 14 }, { "Sec-Fetch-Mode", 14 },
        { "Sec-Fetch-User", 14 }, { "Sec-Fetch-Dest", 14 },
        { "Sec-CH-UA", 9 }, { "Sec-CH-UA-Platform", 18 }
    } },
    { "hit_mixed", {
        { "Host", 4 }, { "Sec-Fetch-Site", 14 }, { "Cookie", 6 },
        { "Sec-Fetch-Mode", 14 }, { "Connection", 10 },
        { "Sec-CH-UA-Platform", 18 }
    } },
    { "miss", {
        { "Hots", 4 }, { "Accpet", 6 }, { "Cooxie", 6 },
        { "User-Agend", 10 }, { "Sec-CH-UA-Platforn", 18 }, { "Referef", 7 }
    } }
};

static volatile size_t lookup_sink;

static double lookup_sample(const implementations_t *parser, const void *context,
                             const lookup_key_t *keys, void *const *queries, size_t count, size_t groups)
{
    size_t sum = 0;
    uint64_t overhead = measure_timer_overhead(1);
    uint64_t start = now_ns();
    if (queries != NULL) {
        for (size_t i = 0; i < groups; ++i) {
            for (size_t k = 0; k < count; ++k) {
                sum += parser->header_lookup_prepared(context, queries[k]);
            }
        }
    } else {
        for (size_t i = 0; i < groups; ++i) {
            for (size_t k = 0; k < count; ++k) {
                sum += parser->header_lookup(context, keys[k].key, keys[k].len);
            }
        }
    }
    uint64_t elapsed = now_ns() - start;
    lookup_sink = sum;
    return (double)(elapsed > overhead ? elapsed - overhead : 0) / (double)(groups * count);
}

static int bench_lookup(FILE *out, const implementations_t *parser, const store_fixture_t *fixture,
                         const char *operation, const lookup_key_t *keys, size_t count, int prepared)
{
    void *queries[LOOKUP_KEY_COUNT] = {0};
    if (prepared) {
        for (size_t k = 0; k < count; ++k) {
            queries[k] = parser->header_query_new(keys[k].key, keys[k].len);
            if (queries[k] == NULL) {
                for (size_t j = 0; j < k; ++j) {
                    parser->header_query_free(queries[j]);
                }
                return -1;
            }
        }
    }
    unsigned char input[fixture->len + 1];
    memcpy(input, fixture->data, fixture->len);
    input[fixture->len] = 0;
    void *context = NULL;
#ifdef BENCH_POOL
    parser_pool_begin();
#endif
    if (parser->request(&context, input, fixture->len,
                         HEADER_CAPACITY) != 0) {
        parser->context_free(context);
#ifdef BENCH_POOL
        parser_pool_end();
#endif
        if (prepared) {
            for (size_t k = 0; k < count; ++k) {
                parser->header_query_free(queries[k]);
            }
        }
        return -1;
    }
    double warmup = lookup_sample(parser, context, keys, prepared ? queries : NULL, count, 1000);
    size_t groups = sample_iterations(warmup * 1000 * count, 1000, quick);
    bench_stats_t stats = {0};
    do {
        bench_stats_add(&stats, lookup_sample(parser, context, keys, prepared ? queries : NULL, count, groups));
    } while (quick ? stats.count < 3 : !bench_stats_done(&stats));
    parser->context_free(context);
#ifdef BENCH_POOL
    parser_pool_end();
#endif
    if (prepared) {
        for (size_t k = 0; k < count; ++k) {
            parser->header_query_free(queries[k]);
        }
    }
    fprintf(out, "%s,%s%s,request,%zu,%u,%zu,%zu,%.3f,%.3f,%.6f,%s\n",
            fixture->name, prepared ? "prepared_" : "", operation, fixture->len, HEADER_CAPACITY,
            stats.count, groups * count, stats.mean, bench_stats_stddev(&stats),
            bench_stats_rciw(&stats),
#ifdef BENCH_POOL
            "preallocated"
#else
            "allocated"
#endif
    );
    fprintf(stderr, "%s/%s/%s%s: %zu samples, RCIW %.2f%%\n", parser->name,
            fixture->name, prepared ? "prepared_" : "", operation, stats.count, 100.0 * bench_stats_rciw(&stats));
    return 0;
}

static FILE *open_store_results(const char *name)
{
    char path[256];
    FILE *out;

    if (check_only) {
        return NULL;
    }
    snprintf(path, sizeof(path), "results/%s-%s.csv", name,
             BENCH_VARIANT);
    out = fopen(path, "w");
    if (out != NULL) {
        fputs("fixture,operation,direction,bytes,header_capacity,samples,"
              "iterations,mean_ns,stddev_ns,rciw,allocation\n",
              out);
    }
    return out;
}

static int run_storage_benchmarks(void)
{
    if (!check_only && mkdir("results", 0755) != 0 && errno != EEXIST) {
        perror("results");
        return 1;
    }
    for (size_t i = 0; i < sizeof(implementations) / sizeof(implementations[0]); i++) {
        const implementations_t *parser = &implementations[i];
        FILE *out               = open_store_results(parser->name);
        int result              = 0;

        if (!check_only && out == NULL) {
            fprintf(stderr, "%s: cannot open result file\n", parser->name);
            return 1;
        }
        enum { CASE_COUNT = sizeof(LOOKUP_CASES) / sizeof(LOOKUP_CASES[0]) };
        for (size_t f = 0; f < sizeof(STORE_FIXTURES) / sizeof(STORE_FIXTURES[0]); f++) {
            if (bench_store_scenario(out, parser, &STORE_FIXTURES[f], 0, "parse") != 0) {
                result = -1;
                break;
            }
            if (STORE_FIXTURES[f].split_input && parser->request_split != NULL) {
                const size_t length = STORE_FIXTURES[f].len;
                if (bench_store_scenario(out, parser, &STORE_FIXTURES[f],
                                         length / 2, "parse_split_50") != 0 ||
                    bench_store_scenario(out, parser, &STORE_FIXTURES[f],
                                         length * 9 / 10, "parse_split_90") != 0) {
                    result = -1;
                    break;
                }
            }
            if (!check_only) {
                for (size_t c = 0; c < CASE_COUNT; c++) {
                    if (bench_lookup(out, parser, &STORE_FIXTURES[f],
                                     LOOKUP_CASES[c].operation, LOOKUP_CASES[c].keys,
                                     LOOKUP_KEY_COUNT, 0) != 0) {
                        result = -1;
                        break;
                    }
                    if (parser->header_query_new != NULL &&
                        bench_lookup(out, parser, &STORE_FIXTURES[f],
                                     LOOKUP_CASES[c].operation, LOOKUP_CASES[c].keys,
                                     LOOKUP_KEY_COUNT, 1) != 0) {
                        result = -1;
                        break;
                    }
                }
                if (result != 0) {
                    break;
                }
            }
        }
        if (out != NULL && fclose(out) != 0) {
            return 1;
        }
        if (result != 0) {
            return 1;
        }
        printf("%s: done\n", parser->name);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc > 1) {
        if (argc != 2) {
            fprintf(stderr, "usage: %s [--check|--quick]\n", argv[0]);
            return 1;
        }
        if (strcmp(argv[1], "--check") == 0) {
            check_only = 1;
        } else if (strcmp(argv[1], "--quick") == 0) {
            quick = 1;
        } else {
            fprintf(stderr, "invalid argument: %s\n", argv[1]);
            return 1;
        }
    }
    return run_storage_benchmarks();
}
