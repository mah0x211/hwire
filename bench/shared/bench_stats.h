#ifndef BENCH_STATS_H
#define BENCH_STATS_H

#include <math.h>
#include <stddef.h>

enum {
    MIN_SAMPLES = 20,
    SAMPLE_STEP = 10,
    MAX_SAMPLES = 100
};

static const double TARGET_RCIW = 0.02;

typedef struct {
    size_t count;
    double mean;
    double m2;
} bench_stats_t;

static inline void bench_stats_add(bench_stats_t *stats, double value)
{
    double delta = value - stats->mean;

    stats->count++;
    stats->mean += delta / (double)stats->count;
    stats->m2 += delta * (value - stats->mean);
}

static inline double bench_stats_stddev(const bench_stats_t *stats)
{
    return sqrt(fmax(0, stats->m2) / (double)(stats->count - 1));
}

static inline double bench_stats_rciw(const bench_stats_t *stats)
{
    /* Nine predefined looks: n=20,30,...,100. Bonferroni allocates 0.05/9
     * error to each two-sided t interval. Values are t(1-0.05/(2*9), n-1),
     * rounded upward to six decimals. Coverage assumes independent normal
     * sample means. RCIW is the full interval width divided by the mean. */
    static const double critical[] = {3.126596, 2.995913, 2.935705,
                                      2.901087, 2.878608, 2.862836,
                                      2.851159, 2.842167, 2.835028};
    size_t n                       = stats->count;

    if (n < MIN_SAMPLES || n > MAX_SAMPLES ||
        (n - MIN_SAMPLES) % SAMPLE_STEP != 0 || stats->mean <= 0) {
        return INFINITY;
    }
    return 2 * critical[(n - MIN_SAMPLES) / SAMPLE_STEP] *
           bench_stats_stddev(stats) / (sqrt((double)n) * stats->mean);
}

static inline int bench_stats_done(const bench_stats_t *stats)
{
    return stats->count >= MAX_SAMPLES ||
           bench_stats_rciw(stats) <= TARGET_RCIW;
}

#endif
