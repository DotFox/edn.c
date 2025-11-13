#include <stdio.h>

#include "../include/edn.h"
#include "bench_time.h"

/* Simple benchmark placeholder - will be implemented in Phase 10 */

int main(void) {
    printf("EDN.C Benchmark Suite\n");
    printf("=====================\n\n");

    const char* sample = "{:name \"Alice\" :age 30}";
    int iterations = 100000;

    printf("Warming up...\n");
    for (int i = 0; i < 1000; i++) {
        edn_result_t result = edn_parse(sample, 0);
        edn_free(result.value);
    }

    printf("Running benchmark: %d iterations\n", iterations);
    double start = get_time();

    for (int i = 0; i < iterations; i++) {
        edn_result_t result = edn_parse(sample, 0);
        edn_free(result.value);
    }

    double elapsed = get_time() - start;
    double per_iter = (elapsed / iterations) * 1e6; /* microseconds */

    printf("\n");
    printf("Total time:  %.3f seconds\n", elapsed);
    printf("Per parse:   %.3f µs\n", per_iter);
    printf("Throughput:  %.0f parses/sec\n", iterations / elapsed);

    return 0;
}
