/**
 * Simple cross-platform timing utilities for benchmarks
 * 
 * Provides a simple get_time() function that returns seconds as a double.
 * Uses QueryPerformanceCounter on Windows, clock_gettime on POSIX systems.
 */

#ifndef BENCH_TIME_H
#define BENCH_TIME_H

#if defined(_WIN32) || defined(_WIN64)
/* Windows */
#include <windows.h>

static inline double get_time(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double) counter.QuadPart / (double) freq.QuadPart;
}

#else
/* POSIX */
#include <time.h>

static inline double get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

#endif

#endif /* BENCH_TIME_H */
