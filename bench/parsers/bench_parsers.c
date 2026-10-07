/* Standalone HTTP parser benchmark driver. */

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


typedef int (*parser_fn_t)(void **context, const unsigned char *data,
                           size_t len);
typedef void (*context_free_fn_t)(void *context);

typedef struct {
    const char *name;
    parser_fn_t request;
    parser_fn_t response;
    context_free_fn_t context_free;
} parsers_t;

typedef struct {
    const char *name;
    const unsigned char *data;
    size_t len;
} fixture_t;

# include "fixtures.c"
# include "parserlist.c"

static double bench_parser(parser_fn_t fn, context_free_fn_t context_free,
                           const fixture_t *fixture, size_t iterations)
{
    const uint64_t overhead = measure_timer_overhead(iterations);
    uint64_t elapsed        = 0;

    for (size_t i = 0; i < iterations; i++) {
        void *context  = NULL;
        uint64_t start = now_ns();
        int result     = fn(&context, fixture->data, fixture->len);

        elapsed += now_ns() - start;
        context_free(context);
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

static int bench_fixture(FILE *out, parser_fn_t fn,
                         context_free_fn_t context_free,
                         const fixture_t *fixture, const char *parser,
                         const char *direction)
{
    const size_t warmup = 100;
    size_t iterations;
    bench_stats_t stats = {0};
    uint64_t start;
    double elapsed;

    void *context = NULL;

    if (fn(&context, fixture->data, fixture->len) != 0) {
        context_free(context);
        fprintf(stderr, "%s: %s/%s: parse failed\n", parser, direction,
                fixture->name);
        return -1;
    }
    context_free(context);
    if (check_only) {
        return 0;
    }
    start = now_ns();
    if (bench_parser(fn, context_free, fixture, warmup) < 0.0) {
        return -1;
    }
    elapsed = (double)(now_ns() - start);
    iterations = sample_iterations(elapsed, warmup, quick);
    do {
        double sample = bench_parser(fn, context_free, fixture, iterations);
        if (sample < 0.0) {
            return -1;
        }
        bench_stats_add(&stats, sample);
    } while (quick ? stats.count < 3 : !bench_stats_done(&stats));
    fprintf(out, "%s/%zu %zu %zu %.3f %.3f %.6f\n", fixture->name,
            fixture->len, stats.count, iterations, stats.mean,
            bench_stats_stddev(&stats), bench_stats_rciw(&stats));
    fprintf(stderr, "%s/%s: %zu samples, RCIW %.2f%%\n", parser,
            fixture->name, stats.count, 100.0 * bench_stats_rciw(&stats));
    return 0;
}

static int bench_direction(FILE *out, parser_fn_t fn,
                           context_free_fn_t context_free,
                           const fixture_t *fixtures, size_t count,
                           const char *parser, const char *direction)
{
    for (size_t fixture = 0; fixture < count; fixture++) {
        if (bench_fixture(out, fn, context_free, &fixtures[fixture], parser,
                          direction) != 0) {
            return -1;
        }
    }
    return 0;
}

static int run_parser_benchmarks(int do_requests, int do_responses)
{
    for (size_t i = 0; i < sizeof(parsers) / sizeof(parsers[0]); i++) {
        const parsers_t *parser = &parsers[i];
        char path[256];
        FILE *out  = NULL;
        int result = 0;

        if (!check_only) {
            snprintf(path, sizeof(path), "results/%s.txt", parser->name);
            out = fopen(path, "w");
            if (out == NULL) {
                fprintf(stderr, "%s: cannot open result file\n", parser->name);
                return 1;
            }
        }
        if (do_requests) {
            result |= bench_direction(out, parser->request,
                                      parser->context_free, REQUESTS,
                                      sizeof(REQUESTS) / sizeof(REQUESTS[0]),
                                      parser->name, "request");
        }
        if (do_responses) {
            result |= bench_direction(out, parser->response,
                                      parser->context_free, RESPONSES,
                                      sizeof(RESPONSES) / sizeof(RESPONSES[0]),
                                      parser->name, "response");
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
    int do_requests = 1;
    int do_responses = 1;
    if (argc > 1) {
        if (argc != 2) {
            fprintf(stderr, "usage: %s [request|response|--check|--quick]\n", argv[0]);
            return 1;
        }
        if (strcmp(argv[1], "--check") == 0) {
            check_only = 1;
        } else if (strcmp(argv[1], "--quick") == 0) {
            quick = 1;
        } else if (strcmp(argv[1], "request") == 0) {
            do_responses = 0;
        } else if (strcmp(argv[1], "response") == 0) {
            do_requests = 0;
        } else {
            fprintf(stderr, "invalid argument: %s\n", argv[1]);
            return 1;
        }
    }
    if (!check_only && mkdir("results", 0755) != 0 && errno != EEXIST) {
        perror("results");
        return 1;
    }
    return run_parser_benchmarks(do_requests, do_responses);
}
