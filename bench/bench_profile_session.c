/**
 * Profile Session Benchmark
 * 
 * Long-running benchmark designed specifically for profiling.
 * Runs for ~10 seconds to gather sufficient profiling samples.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/edn.h"
#include "bench_framework.h"

/* Read entire file into memory */
static char* read_file(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return NULL;
    }

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, size, f);
    fclose(f);

    if ((long) read_size != size) {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    *out_size = size;
    return buffer;
}

/* Profile a single file for a specified duration */
static void profile_file(const char* filename, const char* description, double target_seconds) {
    char path[256];
    snprintf(path, sizeof(path), "bench/data/%s", filename);

    size_t size;
    char* data = read_file(path, &size);
    if (!data) {
        printf("Failed to read %s\n", filename);
        return;
    }

    printf("Profiling: %s (size: %zu bytes)\n", description, size);
    printf("Target duration: %.1f seconds\n", target_seconds);
    printf("Starting...\n");
    fflush(stdout);

    uint64_t target_ns = (uint64_t) (target_seconds * 1000000000.0);
    uint64_t start = bench_get_time_ns();
    uint64_t elapsed = 0;
    size_t iterations = 0;
    size_t total_bytes = 0;

    /* Run until target duration reached */
    while (elapsed < target_ns) {
        edn_result_t result = edn_parse(data, size);
        if (result.error != EDN_OK) {
            printf("Parse error: %s\n", result.error_message);
            free(data);
            return;
        }
        edn_free(result.value);

        iterations++;
        total_bytes += size;
        elapsed = bench_get_time_ns() - start;
    }

    double duration_s = (double) elapsed / 1000000000.0;
    double throughput_gbps = ((double) total_bytes / (1024.0 * 1024.0 * 1024.0)) / duration_s;
    double mean_ns = (double) elapsed / (double) iterations;

    printf("Completed: %zu iterations in %.2f seconds\n", iterations, duration_s);
    printf("Mean time: %.2f μs per iteration\n", mean_ns / 1000.0);
    printf("Throughput: %.3f GB/s\n", throughput_gbps);
    printf("\n");

    free(data);
}

int main(void) {
    printf("===========================================\n");
    printf("EDN.C Profiling Session Benchmark\n");
    printf("===========================================\n\n");

    /* Profile different workload types for ~3 seconds each */
    /* Total runtime: ~12 seconds - good for profiling */

    printf("--- Workload 1: Basic Maps (mixed types) ---\n");
    profile_file("basic_10000.edn", "basic_10000 (10 KB)", 3.0);

    printf("--- Workload 2: Keywords (identifiers) ---\n");
    profile_file("keywords_10000.edn", "keywords_10000 (117 KB)", 3.0);

    printf("--- Workload 3: Integers (numbers) ---\n");
    profile_file("ints_1400.edn", "ints_1400 (10 KB)", 3.0);

    printf("--- Workload 4: Strings ---\n");
    profile_file("strings_1000.edn", "strings_1000 (55 KB)", 3.0);

    printf("===========================================\n");
    printf("Profiling session complete!\n");
    printf("Total runtime: ~12 seconds\n");
    printf("\n");
    printf("To analyze results:\n");
    printf("  1. Open the generated trace file in Instruments\n");
    printf("  2. View 'Heaviest Stack Trace' for CPU hotspots\n");
    printf("  3. Look for functions consuming >5%% CPU time\n");
    printf("===========================================\n");

    return 0;
}
