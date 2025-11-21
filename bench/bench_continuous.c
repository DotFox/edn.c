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

/* Continuous benchmark that runs for a specified duration for profiling */

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
    /* Load benchmark files */
    size_t keywords_size, maps_size, ints_size;
    char* keywords_data = read_file("bench/data/keywords_1000.edn", &keywords_size);
    char* maps_data = read_file("bench/data/basic_1000.edn", &maps_size);
    char* ints_data = read_file("bench/data/ints_1400.edn", &ints_size);

    if (!keywords_data || !maps_data || !ints_data) {
        printf("ERROR: Could not load benchmark files\n");
        return 1;
    }

    printf("Continuous profiling benchmark - running for 20 seconds...\n");
    printf("Attach profiler to PID: %d\n", getpid());
    fflush(stdout);

    /* Sleep briefly to allow profiler attachment */
    sleep_ms(2000); /* 2 seconds */

    /* Run for 20 seconds */
    time_t start = time(NULL);
    time_t end = start + 20;

    unsigned long iterations = 0;
    while (time(NULL) < end) {
        /* Parse each file type */
        edn_result_t kw_result = edn_read(keywords_data, keywords_size);
        if (kw_result.value)
            edn_free(kw_result.value);

        edn_result_t map_result = edn_read(maps_data, maps_size);
        if (map_result.value)
            edn_free(map_result.value);

        edn_result_t ints_result = edn_read(ints_data, ints_size);
        if (ints_result.value)
            edn_free(ints_result.value);

        iterations++;

        /* Print progress every 100 iterations */
        if (iterations % 100 == 0) {
            printf(".");
            fflush(stdout);
        }
    }

    printf("\nComplete! %lu iterations\n", iterations);

    free(keywords_data);
    free(maps_data);
    free(ints_data);

    return 0;
}
