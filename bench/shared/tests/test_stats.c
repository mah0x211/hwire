#include "bench_stats.h"
#include <assert.h>

int main(void)
{
    bench_stats_t stable   = {0};
    bench_stats_t moderate = {0};
    bench_stats_t noisy    = {0};

    for (size_t n = 1; n <= MAX_SAMPLES; n++) {
        bench_stats_add(&stable, 100);
        bench_stats_add(&moderate, n % 2 ? 98 : 102);
        bench_stats_add(&noisy, n % 2 ? 90 : 110);
        if (n < MIN_SAMPLES) {
            assert(!bench_stats_done(&stable));
        }
        if (n == 20) {
            assert(bench_stats_done(&stable));
            assert(!bench_stats_done(&moderate));
            assert(bench_stats_rciw(&moderate) > TARGET_RCIW);
        }
        if (n == 30) {
            assert(!bench_stats_done(&moderate));
        }
        if (n == 40) {
            assert(bench_stats_done(&moderate));
            assert(bench_stats_rciw(&moderate) <= TARGET_RCIW);
        }
        if (n < MAX_SAMPLES) {
            assert(!bench_stats_done(&noisy));
        }
    }
    assert(bench_stats_done(&noisy));
    assert(bench_stats_rciw(&noisy) > TARGET_RCIW);
    assert(fabs(moderate.mean - 100) < 1e-12);
    assert(fabs(bench_stats_stddev(&moderate) - sqrt(400.0 / 99)) < 1e-12);
    return 0;
}
