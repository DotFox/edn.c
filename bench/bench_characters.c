#include <stdio.h>
#include <string.h>

#include "../include/edn.h"
#include "bench_time.h"

#define ITERATIONS 1000000

/* Helper to measure time in nanoseconds */
static double measure_ns(void (*fn)(void*), void* arg, int iterations) {
    double start = get_time();

    for (int i = 0; i < iterations; i++) {
        fn(arg);
    }

    double end = get_time();
    double elapsed = (end - start) * 1e9;
    return elapsed / iterations;
}

/* Benchmark data structure */
typedef struct {
    const char* input;
} bench_data_t;

/* Parse character using public API */
static void bench_parse(void* arg) {
    bench_data_t* data = (bench_data_t*) arg;
    edn_result_t result = edn_parse(data->input, 0);
    if (result.value) {
        edn_free(result.value);
    }
}

int main(void) {
    printf("Character Parsing Benchmarks\n");
    printf("=============================\n");
    printf("Iterations: %d\n\n", ITERATIONS);

    /* Benchmark 1: Single character */
    {
        bench_data_t data = {.input = "\\a"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Single character (\\a):        %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    /* Benchmark 2: Named character - short (tab) */
    {
        bench_data_t data = {.input = "\\tab"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Named character (\\tab):       %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    /* Benchmark 3: Named character - medium (space) */
    {
        bench_data_t data = {.input = "\\space"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Named character (\\space):     %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    /* Benchmark 4: Named character - long (newline) */
    {
        bench_data_t data = {.input = "\\newline"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Named character (\\newline):   %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    /* Benchmark 5: Unicode escape */
    {
        bench_data_t data = {.input = "\\u0041"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Unicode escape (\\u0041):      %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    /* Benchmark 6: Special character (backslash) */
    {
        bench_data_t data = {.input = "\\\\"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Special character (\\\\):       %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    printf("\nBenchmark complete.\n");
    return 0;
}
