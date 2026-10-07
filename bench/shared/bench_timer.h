#ifndef BENCH_TIMER_H
#define BENCH_TIMER_H
#include <stddef.h>
#include <stdint.h>
#include <time.h>

static inline uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static uint64_t measure_timer_overhead(size_t iterations)
{
    uint64_t elapsed = 0;

    for (size_t i = 0; i < iterations; i++) {
        uint64_t start = now_ns();

        elapsed += now_ns() - start;
    }
    return elapsed;
}

/* Calibrate total sample duration, including the timer and cleanup work.
 * Each operation is still timed separately, excluding context destruction. */
static size_t sample_iterations(double elapsed, size_t warmup, int quick)
{
    double count = 1000000.0 * (double)warmup / elapsed;

    if (quick) {
        return 100;
    }
    if (count < 1.0) {
        return 1;
    }
    if (count > 1000000.0) {
        return 1000000;
    }
    return (size_t)count;
}

#endif
