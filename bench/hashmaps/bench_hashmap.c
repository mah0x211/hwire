#define _POSIX_C_SOURCE 200809L
#include "bench_input.h"
#include "bench_stats.h"
#include "hashmap_hash.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

enum { PILOT_OPS = 16384, MAX_OPS = 1000000 };
static const double SAMPLE_NS = 1000000;
typedef void *(*new_fn)(size_t);
typedef void (*free_fn)(void *);
typedef size_t (*stat_fn)(const void *);
typedef double (*loadfactor_fn)(const void *);
typedef void (*populate_fn)(void *, size_t);
typedef double (*lookup_fn)(const void *, const hwire_str_t *, size_t, size_t);
typedef struct {
    const char *id;
    const char *(*name)(void);
    new_fn create, create_ci, grow, grow_ci;
    free_fn destroy;
    stat_fn bytes;
    loadfactor_fn loadfactor;
    stat_fn growths;
    populate_fn populate;
    lookup_fn hit, hit_ci;
} hashmap_t;
typedef struct {
    size_t bytes, growths;
    double loadfactor;
} metadata_t;
static volatile uintptr_t sink;
static int quick, metadata_only;
static const char *scenario;
static size_t key_count;
static double timer_overhead;

static inline double now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}
#define BENCH_DIRECT_LOOKUP(wrapper, operation) \
static double wrapper(const void *ctx, const hwire_str_t *query, size_t count, size_t ops) \
{ \
    uintptr_t result = 0; \
    double start = now_ns(); \
    for (size_t i = 0; i < ops; i++) { \
        const hwire_str_t *key = &query[i & (count - 1)]; \
        result ^= (uintptr_t)operation(ctx, key->ptr, key->len); \
    } \
    double elapsed = now_ns() - start - timer_overhead; \
    sink ^= result; \
    return elapsed / (double)ops; \
}
#define BENCH_DIRECT_POPULATE(wrapper, operation) \
static void wrapper(void *ctx, size_t count) \
{ \
    for (size_t i = 0; i < count; i++) { \
        operation(ctx, &selected_pairs[i]); \
    } \
}
#include "maplist.c"
#undef BENCH_DIRECT_LOOKUP
#undef BENCH_DIRECT_POPULATE

static void *create_context(new_fn create)
{
    void *ctx = create(key_count);
    if (ctx == NULL) {
        fputs("allocation failed\n", stderr);
        exit(1);
    }
    return ctx;
}
/* One untimed population records final metadata.
 * This does not inspect results or repeat key/value correctness comparisons. */
static metadata_t prepare_case(const hashmap_t *map, new_fn create)
{
    void *ctx = create_context(create);
    map->populate(ctx, key_count);
    metadata_t info = {
        .bytes = map->bytes(ctx),
        .loadfactor = map->loadfactor(ctx),
        .growths = map->growths(ctx)
    };
    map->destroy(ctx);
    return info;
}
static double measure_lookup(const hashmap_t *map, new_fn create, lookup_fn find,
                             const hwire_str_t *query, size_t ops)
{
    void *ctx = create_context(create);
    map->populate(ctx, key_count);
    double value = find(ctx, query, key_count, ops);
    map->destroy(ctx);
    return value;
}
static double measure_build(const hashmap_t *map, new_fn create, size_t ops,
                            int include_initial)
{
    size_t batches = ops / key_count;
    double elapsed = 0;
    for (size_t i = 0; i < batches; i++) {
        void *ctx;
        double start;
        if (include_initial) {
            /* Initial allocation and initialization are timed. */
            start = now_ns();
            ctx = create(key_count);
        } else {
            ctx = create_context(create);
            start = now_ns();
        }
        map->populate(ctx, key_count);
        elapsed += now_ns() - start - timer_overhead;
        map->destroy(ctx);
    }
    return elapsed / (double)(include_initial ? batches : batches * key_count);
}
static size_t sample_ops(double ns_per_key)
{
    double batches = ceil(SAMPLE_NS / (ns_per_key * (double)key_count));
    size_t limit = MAX_OPS / key_count;
    if (quick) {
        return key_count * 4;
    }
    if (batches < 1) {
        batches = 1;
    }
    if (batches > (double)limit) {
        batches = (double)limit;
    }
    return (size_t)batches * key_count;
}
static void progress(const hashmap_t *map, int ci, const char *operation,
                     const bench_stats_t *stats)
{
    fprintf(stderr, "%s/%s %s %zu keys %s %s: %zu samples",
            map->name(), BENCH_VARIANT, scenario, key_count,
            ci ? "case-insensitive" : "case-sensitive", operation, stats->count);
    if (!quick) {
        double rciw = bench_stats_rciw(stats);
        fprintf(stderr, ", RCIW %.2f%% / 2.00%%%s", 100 * rciw,
                rciw <= TARGET_RCIW ? "" : " (target unmet)");
    }
    fputc('\n', stderr);
    fflush(stderr);
}
static void bench_case(FILE *out, const hashmap_t *map, new_fn create, int ci)
{
    metadata_t info = prepare_case(map, create);
    if (metadata_only) {
        fprintf(out, "%s,%s,%zu,%s,%.17g\n", map->name(), scenario, key_count,
                ci ? "case-insensitive" : "case-sensitive", info.loadfactor);
        return;
    }
    static const char *operations[] = {"build", "insert", "hit", "miss"};
    for (size_t op = 0; op < 4; op++) {
        bench_stats_t stats = {0};
        const hwire_str_t *query = op == 3 ? selected_misses :
                                  ci ? selected_ci_hits : selected_hits;
        lookup_fn find = ci ? map->hit_ci : map->hit;
        size_t pilot_ops = quick ? key_count * 4 : PILOT_OPS;
        double pilot = op < 2 ? measure_build(map, create, pilot_ops, op == 0) :
                               measure_lookup(map, create, find, query, pilot_ops);
        size_t ops = sample_ops(op == 0 ? pilot / (double)key_count : pilot);
        do {
            double value = op < 2 ? measure_build(map, create, ops, op == 0) :
                                   measure_lookup(map, create, find, query, ops);
            bench_stats_add(&stats, value);
            if (!quick && stats.count >= MIN_SAMPLES &&
                (stats.count - MIN_SAMPLES) % SAMPLE_STEP == 0) {
                progress(map, ci, operations[op], &stats);
            }
        } while (quick ? stats.count < 3 : !bench_stats_done(&stats));
        if (quick) {
            progress(map, ci, operations[op], &stats);
        }
        fprintf(out, "%s,%s,%zu,%s,%s,%zu,%zu,%.9g,%.9g,%zu,%zu,%.17g,",
                map->name(), scenario, key_count,
                ci ? "case-insensitive" : "case-sensitive", operations[op],
                stats.count, ops, stats.mean, bench_stats_stddev(&stats),
                info.bytes, info.growths, info.loadfactor);
        if (quick) {
            fputs(",\n", out);
        } else {
            fprintf(out, "%.9g,%.9g\n", bench_stats_rciw(&stats), TARGET_RCIW);
        }
        fflush(out);
    }
}
int main(int argc, char **argv)
{
    int growth_only = 0;
    const char *only = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--quick") == 0) {
            quick = 1;
        } else if (strcmp(argv[i], "--metadata") == 0) {
            metadata_only = 1;
        } else if (strcmp(argv[i], "--growth") == 0) {
            growth_only = 1;
        } else if (strcmp(argv[i], "--map") == 0 && i + 1 < argc) {
            only = argv[++i];
        } else if (strcmp(argv[i], "--describe") == 0) {
            printf("hash: %s\n", hashmap_hash_backend());
            return 0;
        } else {
            fprintf(stderr, "usage: %s [--quick] [--growth] [--map ID] [--describe|--metadata]\n", argv[0]);
            return 1;
        }
    }
    prepare_input();
    if (!metadata_only) {
        /* Mean cost of an empty timer pair, excluded from all measured intervals. */
        for (size_t i = 0; i < 100000; i++) {
            double start = now_ns();
            timer_overhead += now_ns() - start;
        }
        timer_overhead /= 100000;
        fprintf(stderr, "timer pair overhead: %.2f ns\n", timer_overhead);
        mkdir("results", 0755);
        mkdir("results/storage", 0755);
    } else {
        fputs("map,scenario,count,mode,load_factor\n", stdout);
    }
    static const size_t counts[] = {32, 64, 128, 256};
    static const char *scenarios[] = {"reserved", "reserved", "reserved",
                                     "growth-allocated"};
    for (size_t m = 0; m < sizeof(maps) / sizeof(maps[0]); m++) {
        const hashmap_t *map = &maps[m];
        if (only && strcmp(only, map->id) != 0) {
            continue;
        }
        char path[256];
        snprintf(path, sizeof(path), "results/storage/%s-%s.csv", map->id, BENCH_VARIANT);
        FILE *out = metadata_only ? stdout : fopen(path, "w");
        if (out == NULL) {
            perror(path);
            return 1;
        }
        if (!metadata_only) {
            fputs("map,scenario,count,mode,operation,samples,iterations,mean_ns,stddev_ns,container_bytes,growths,load_factor,rciw,target_rciw\n", out);
        }
        for (size_t s = growth_only ? 3 : 0; s < sizeof(counts) / sizeof(counts[0]); s++) {
            key_count = counts[s];
            scenario = scenarios[s];
            fprintf(stderr, "%s/%s %s %zu keys: starting\n",
                    map->name(), BENCH_VARIANT, scenario, key_count);
            for (int ci = 0; ci < 2; ci++) {
                new_fn create = s < 3 ? (ci ? map->create_ci : map->create) :
                                       (ci ? map->grow_ci : map->grow);
                bench_case(out, map, create, ci);
            }
        }
        if (!metadata_only) {
            fclose(out);
        }
    }
    return 0;
}
