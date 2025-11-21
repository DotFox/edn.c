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

/* Parse identifier using public API */
static void bench_parse(void* arg) {
    bench_data_t* data = (bench_data_t*) arg;
    edn_result_t result = edn_read(data->input, 0);
    if (result.value) {
        edn_free(result.value);
    }
}

int main(void) {
    printf("Identifier Parsing Benchmarks\n");
    printf("==============================\n");
    printf("Iterations: %d\n\n", ITERATIONS);

    /* Benchmark 1: Reserved keyword (nil) */
    {
        bench_data_t data = {.input = "nil"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Reserved (nil):                   %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    /* Benchmark 2: Simple symbol */
    {
        bench_data_t data = {.input = "foo"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Simple symbol (foo):              %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    /* Benchmark 3: Symbol with dash */
    {
        bench_data_t data = {.input = "bar-baz"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Symbol with dash (bar-baz):       %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    /* Benchmark 4: Simple keyword */
    {
        bench_data_t data = {.input = ":foo"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Simple keyword (:foo):            %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    /* Benchmark 5: Namespaced symbol (medium) */
    {
        bench_data_t data = {.input = "my.ns/func"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Namespaced symbol (my.ns/func):   %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    /* Benchmark 6: Namespaced symbol (long - SIMD path) */
    {
        bench_data_t data = {.input = "clojure.core/map"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Namespaced symbol (clojure.core/map): %.2f ns/op (%.0f Mops/sec)\n", ns,
               1000.0 / ns);
    }

    /* Benchmark 7: Namespaced keyword */
    {
        bench_data_t data = {.input = ":my.company.project/some-function"};
        double ns = measure_ns(bench_parse, &data, ITERATIONS);
        printf("Long namespaced keyword:          %.2f ns/op (%.0f Mops/sec)\n", ns, 1000.0 / ns);
    }

    printf("\nBenchmark complete.\n");
    return 0;
}
