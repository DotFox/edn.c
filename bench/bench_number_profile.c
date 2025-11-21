#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#include <process.h> /* _getpid() */
#include <windows.h> /* Sleep() */
#define getpid _getpid
#define sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#include "../include/edn.h"
#include "../src/edn_internal.h"

/* Detailed profiling of number parsing to identify bottlenecks */

static char* read_file(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* data = malloc(*size);
    fread(data, 1, *size, f);
    fclose(f);

    return data;
}

int main(void) {
    /* Load integer benchmark file */
    size_t size;
    char* data = read_file("bench/data/ints_1400.edn", &size);

    if (!data) {
        printf("ERROR: Could not load ints_1400.edn\n");
        return 1;
    }

    printf("Number Parsing Profiling Benchmark\n");
    printf("===================================\n");
    printf("File: ints_1400.edn (%zu bytes, 1400 integers)\n", size);
    printf("PID: %d\n", getpid());
    printf("\nRunning for 20 seconds...\n");
    printf("Attach profiler now!\n\n");
    fflush(stdout);

    /* Sleep briefly to allow profiler attachment */
    sleep_ms(2000); /* 2 seconds */

    /* Run for 20 seconds */
    time_t start = time(NULL);
    time_t end = start + 20;

    uint64_t iterations = 0;
    uint64_t total_numbers = 0;

    while (time(NULL) < end) {
        /* Parse the integer array */
        edn_result_t result = edn_read(data, size);

        if (result.value) {
            /* Count numbers (should be 1400) */
            if (result.value->type == EDN_TYPE_VECTOR) {
                total_numbers += (uint64_t) result.value->as.vector.count;
            }
            edn_free(result.value);
        }

        iterations++;

        /* Print progress every 100 iterations */
        if (iterations % 100 == 0) {
            printf(".");
            fflush(stdout);
        }
    }

    double avg_numbers_per_iter = (double) total_numbers / iterations;
    double numbers_per_sec = (double) total_numbers / 20.0;

    printf("\n\nComplete!\n");
    printf("Iterations: %llu\n", (unsigned long long) iterations);
    printf("Total numbers parsed: %llu\n", (unsigned long long) total_numbers);
    printf("Avg numbers per iteration: %.0f\n", avg_numbers_per_iter);
    printf("Numbers parsed per second: %.0f\n", numbers_per_sec);
    printf("Time per number: %.2f ns\n", 20.0e9 / total_numbers);

    free(data);
    return 0;
}
