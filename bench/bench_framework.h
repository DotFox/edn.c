/**
 * Benchmark Framework
 * 
 * Simple timing utilities for benchmarking EDN parsing performance.
 * 
 * Note: On Unix/macOS/Linux, requires _POSIX_C_SOURCE=200809L for clock_gettime()
 * This is set via -D flag in Makefile for benchmark builds.
 * On Windows, uses QueryPerformanceCounter.
 */

#ifndef BENCH_FRAMEWORK_H
#define BENCH_FRAMEWORK_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
/* Windows timing */
#include <windows.h>

static inline uint64_t bench_get_time_ns(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    /* Convert to nanoseconds: (counter * 1e9) / freq */
    return (uint64_t) ((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
}
#else
/* POSIX timing */
#include <time.h>

/* Get current time in nanoseconds */
static inline uint64_t bench_get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;
}
#endif

/* Benchmark result */
typedef struct {
    uint64_t iterations;
    uint64_t total_time_ns;
    double mean_time_us;
    double throughput_gbps;
    size_t data_size;
} bench_result_t;

/**
 * Run benchmark for a minimum duration or minimum iterations
 * 
 * @param name Benchmark name
 * @param data Input data
 * @param size Size of input data in bytes
 * @param min_duration_ms Minimum duration in milliseconds
 * @param min_iterations Minimum number of iterations
 * @param bench_fn Benchmark function to run (returns true on success)
 */
static inline bench_result_t bench_run(const char* name, const char* data, size_t size,
                                       uint64_t min_duration_ms, uint64_t min_iterations,
                                       int (*bench_fn)(const char*, size_t)) {
    bench_result_t result = {0};
    result.data_size = size;

    uint64_t target_duration_ns = min_duration_ms * 1000000ULL;
    uint64_t iterations = 0;
    uint64_t start_time = bench_get_time_ns();
    uint64_t elapsed = 0;

    /* Warmup */
    for (int i = 0; i < 3; i++) {
        bench_fn(data, size);
    }

    /* Main benchmark loop */
    start_time = bench_get_time_ns();
    while (elapsed < target_duration_ns || iterations < min_iterations) {
        if (!bench_fn(data, size)) {
            printf("ERROR: Benchmark function failed for %s\n", name);
            return result;
        }
        iterations++;
        elapsed = bench_get_time_ns() - start_time;
    }

    result.iterations = iterations;
    result.total_time_ns = elapsed;
    result.mean_time_us = (double) elapsed / (double) iterations / 1000.0;

    /* Calculate throughput in GB/s */
    /* throughput = (iterations * size) / (time_in_seconds) / (1024^3) */
    double total_bytes = (double) iterations * (double) size;
    double time_seconds = (double) elapsed / 1000000000.0;
    result.throughput_gbps = (total_bytes / time_seconds) / (1024.0 * 1024.0 * 1024.0);

    return result;
}

/**
 * Print benchmark result
 */
static inline void bench_print_result(const char* name, bench_result_t result) {
    printf("%-40s ", name);
    printf("%14llu  ", (unsigned long long) result.iterations);
    printf("%8.2f  ", (double) result.total_time_ns / 1000000.0);
    printf("%10.3f  ", result.mean_time_us);
    printf("%10.3f  ", result.throughput_gbps);
    printf("(%zu bytes)\n", result.data_size);
}

/**
 * Print benchmark header
 */
static inline void bench_print_header(void) {
    printf("%-40s %14s  %8s  %10s  %10s  %s\n", "Benchmark", "Iterations", "Total", "Mean",
           "Throughput", "Size");
    printf("%-40s %14s  %8s  %10s  %10s  %s\n", "---------", "----------", "-----", "----",
           "----------", "----");
}

#endif /* BENCH_FRAMEWORK_H */
