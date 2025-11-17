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

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
    double stddev_time_us;         /* Standard deviation in microseconds */
    double confidence_interval_us; /* 95% confidence interval (±) in microseconds */
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
 * @param bench_fn Benchmark function to run (returns closure or NULL on error)
 * @param bench_after_fn Optional cleanup function called with closure (may be NULL)
 * @param include_after_in_timing If true, bench_after_fn is included in timing; if false, called after
 */
static inline bench_result_t bench_run(const char* name, const char* data, size_t size,
                                       uint64_t min_duration_ms, uint64_t min_iterations,
                                       void* (*bench_fn)(const char*, size_t),
                                       void (*bench_after_fn)(void*), int include_after_in_timing) {
    bench_result_t result = {0};
    result.data_size = size;

    uint64_t target_duration_ns = min_duration_ms * 1000000ULL;

    /* Warmup */
    for (int i = 0; i < 3; i++) {
        void* closure = bench_fn(data, size);
        if (bench_after_fn) {
            bench_after_fn(closure);
        }
    }

    /* Allocate array for sample times (sample up to 1000 iterations) */
    size_t max_samples = 1000;
    uint64_t* sample_times = (uint64_t*) malloc(max_samples * sizeof(uint64_t));
    if (!sample_times) {
        printf("ERROR: Failed to allocate sample buffer for %s\n", name);
        return result;
    }

    /* Main benchmark loop - collect samples */
    uint64_t iterations = 0;
    size_t sample_count = 0;
    uint64_t start_time = bench_get_time_ns();
    uint64_t elapsed = 0;
    uint64_t sample_interval = 1; /* Start by sampling every iteration */

    while (elapsed < target_duration_ns || iterations < min_iterations) {
        uint64_t iter_start = bench_get_time_ns();

        void* closure = bench_fn(data, size);
        if (closure == NULL) {
            printf("ERROR: Benchmark function failed for %s\n", name);
            free(sample_times);
            return result;
        }

        /* Call after function inside timing if requested */
        if (bench_after_fn && include_after_in_timing) {
            bench_after_fn(closure);
        }

        uint64_t iter_end = bench_get_time_ns();

        /* Sample this iteration? */
        if (sample_count < max_samples && iterations % sample_interval == 0) {
            sample_times[sample_count++] = iter_end - iter_start;
        }

        iterations++;
        elapsed = bench_get_time_ns() - start_time;

        /* Adjust sampling interval dynamically to get ~1000 samples */
        if (iterations == 100 && sample_count == 100) {
            /* Estimate total iterations and adjust interval */
            uint64_t estimated_total = (iterations * target_duration_ns) / elapsed;
            sample_interval = (estimated_total / max_samples) + 1;
            if (sample_interval < 1)
                sample_interval = 1;
        }

        /* Call after function outside timing if not included */
        if (bench_after_fn && !include_after_in_timing) {
            bench_after_fn(closure);
        }
    }

    result.iterations = iterations;
    result.total_time_ns = elapsed;
    result.mean_time_us = (double) elapsed / (double) iterations / 1000.0;

    /* Calculate standard deviation from samples */
    if (sample_count > 1) {
        /* Calculate mean of samples */
        double sample_mean = 0.0;
        for (size_t i = 0; i < sample_count; i++) {
            sample_mean += (double) sample_times[i];
        }
        sample_mean /= (double) sample_count;

        /* Calculate variance */
        double variance = 0.0;
        for (size_t i = 0; i < sample_count; i++) {
            double diff = (double) sample_times[i] - sample_mean;
            variance += diff * diff;
        }
        variance /= (double) (sample_count - 1); /* Sample variance (Bessel's correction) */

        /* Standard deviation in microseconds */
        result.stddev_time_us = sqrt(variance) / 1000.0;

        /* 95% confidence interval: ±1.96 * (stddev / sqrt(n)) */
        result.confidence_interval_us = 1.96 * result.stddev_time_us / sqrt((double) sample_count);
    } else {
        result.stddev_time_us = 0.0;
        result.confidence_interval_us = 0.0;
    }

    free(sample_times);

    /* Calculate throughput in GB/s */
    /* throughput = (iterations * size) / (time_in_seconds) / (1024^3) */
    double total_bytes = (double) iterations * (double) size;
    double time_seconds = (double) elapsed / 1000000000.0;
    result.throughput_gbps = (total_bytes / time_seconds) / (1024.0 * 1024.0 * 1024.0);

    return result;
}

/**
 * Format number with thousands separator (comma)
 */
static inline void format_with_separator(char* buffer, size_t buffer_size, unsigned long long num) {
    char temp[64];
    snprintf(temp, sizeof(temp), "%llu", num);

    size_t len = strlen(temp);
    size_t commas = (len - 1) / 3;
    size_t total_len = len + commas;

    if (total_len >= buffer_size) {
        snprintf(buffer, buffer_size, "%llu", num);
        return;
    }

    buffer[total_len] = '\0';

    size_t src = len;
    size_t dst = total_len;
    size_t count = 0;

    while (src > 0) {
        if (count == 3) {
            buffer[--dst] = ',';
            count = 0;
        }
        buffer[--dst] = temp[--src];
        count++;
    }
}

/**
 * Print benchmark result
 */
static inline void bench_print_result(const char* name, bench_result_t result) {
    char iterations_str[64];
    format_with_separator(iterations_str, sizeof(iterations_str), result.iterations);

    printf("%-25s ", name);
    printf("%14s  ", iterations_str);
    printf("%10.2f  ", (double) result.total_time_ns / 1000000.0);

    printf("%10.3f ± %-7.3f  ", result.mean_time_us, result.confidence_interval_us);

    printf("%5.3f GB/s  ", result.throughput_gbps);
    printf("(%zu bytes)\n", result.data_size);
}

/**
 * Print benchmark header
 */
static inline void bench_print_header(void) {
    printf("%-25s %14s  %10s  %21s  %10s  %s\n", "Benchmark", "Iterations", "Total (ms)",
           "Mean (μs)", "Throughput", "Size");
    printf("%-25s %14s  %10s  %20s  %10s  %s\n", "---------", "----------", "----------",
           "---------", "----------", "----");
}

#endif /* BENCH_FRAMEWORK_H */
