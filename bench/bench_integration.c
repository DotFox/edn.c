/**
 * Integration benchmarks using real EDN data files
 * 
 * Benchmarks the complete parsing pipeline with realistic workloads.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/edn.h"
#include "bench_framework.h"

/* Benchmark function that parses EDN and frees result (roundtrip) */
static int bench_parse_roundtrip(const char* data, size_t size) {
    edn_result_t result = edn_parse(data, size);
    if (result.error != EDN_OK) {
        return 0;
    }
    edn_free(result.value);
    return 1;
}

/* Benchmark function that only parses (no free) - measures pure parsing performance */
typedef struct {
    edn_value_t** values;
    size_t capacity;
    size_t count;
} value_list_t;

static value_list_t* value_list_create(size_t capacity) {
    value_list_t* list = malloc(sizeof(value_list_t));
    if (!list) return NULL;
    list->values = malloc(sizeof(edn_value_t*) * capacity);
    if (!list->values) {
        free(list);
        return NULL;
    }
    list->capacity = capacity;
    list->count = 0;
    return list;
}

static void value_list_add(value_list_t* list, edn_value_t* value) {
    if (list->count < list->capacity) {
        list->values[list->count++] = value;
    }
}

static void value_list_free_all(value_list_t* list) {
    for (size_t i = 0; i < list->count; i++) {
        edn_free(list->values[i]);
    }
    list->count = 0;
}

static void value_list_destroy(value_list_t* list) {
    if (!list) return;
    value_list_free_all(list);
    free(list->values);
    free(list);
}

/* Parse-only benchmark with deferred cleanup */
static int bench_parse_only(const char* data, size_t size, value_list_t* values) {
    edn_result_t result = edn_parse(data, size);
    if (result.error != EDN_OK) {
        return 0;
    }
    value_list_add(values, result.value);
    return 1;
}

/* Read entire file into memory */
static char* read_file(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return NULL;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0) {
        fclose(f);
        return NULL;
    }

    /* Allocate buffer */
    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    /* Read file */
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

/* Benchmark a single file with both parse-only and roundtrip modes */
static void bench_file(const char* filename, const char* description, int mode) {
    char path[256];
    snprintf(path, sizeof(path), "bench/data/%s", filename);

    size_t size;
    char* data = read_file(path, &size);

    if (!data) {
        printf("%-40s FAILED (could not read file)\n", description);
        return;
    }

    if (mode == 0) {
        /* Parse-only mode: measures pure parsing performance */
        value_list_t* values = value_list_create(10000);
        if (!values) {
            printf("%-40s FAILED (out of memory)\n", description);
            free(data);
            return;
        }

        /* Warmup */
        for (int i = 0; i < 3; i++) {
            bench_parse_only(data, size, values);
        }
        value_list_free_all(values);

        /* Run benchmark */
        uint64_t iterations = 0;
        uint64_t start_time = bench_get_time_ns();
        uint64_t elapsed = 0;
        uint64_t target_duration_ns = 500 * 1000000ULL;
        uint64_t min_iterations = 1000;

        while (elapsed < target_duration_ns || iterations < min_iterations) {
            if (!bench_parse_only(data, size, values)) {
                printf("ERROR: Benchmark function failed for %s\n", description);
                value_list_destroy(values);
                free(data);
                return;
            }
            iterations++;
            
            /* Check time periodically to avoid too many time calls */
            if (iterations % 100 == 0) {
                elapsed = bench_get_time_ns() - start_time;
            }
        }
        elapsed = bench_get_time_ns() - start_time;

        bench_result_t result = {0};
        result.iterations = iterations;
        result.total_time_ns = elapsed;
        result.mean_time_us = (double) elapsed / (double) iterations / 1000.0;
        result.data_size = size;

        double total_bytes = (double) iterations * (double) size;
        double time_seconds = (double) elapsed / 1000000000.0;
        result.throughput_gbps = (total_bytes / time_seconds) / (1024.0 * 1024.0 * 1024.0);

        bench_print_result(description, result);

        /* Cleanup outside of timing */
        value_list_destroy(values);
        
    } else {
        /* Roundtrip mode: includes parse + free */
        bench_result_t result = bench_run(description, data, size, 500, 1000, bench_parse_roundtrip);
        bench_print_result(description, result);
    }

    free(data);
}

int main(void) {
    printf("EDN.C Integration Benchmarks\n");
    printf("============================\n\n");

    printf("=== PARSE-ONLY MODE (Pure Parsing Performance) ===\n");
    printf("Measures only parsing time, excludes memory cleanup\n\n");
    bench_print_header();

    /* Basic maps with increasing size */
    printf("\n--- Basic Maps ---\n");
    bench_file("basic_10.edn", "basic_10 (9 bytes)", 0);
    bench_file("basic_100.edn", "basic_100 (97 bytes)", 0);
    bench_file("basic_1000.edn", "basic_1000 (898 bytes)", 0);
    bench_file("basic_10000.edn", "basic_10000 (10 KB)", 0);
    bench_file("basic_100000.edn", "basic_100000 (99 KB)", 0);

    /* Keywords */
    printf("\n--- Keyword Vectors ---\n");
    bench_file("keywords_10.edn", "keywords_10 (116 bytes)", 0);
    bench_file("keywords_100.edn", "keywords_100 (886 bytes)", 0);
    bench_file("keywords_1000.edn", "keywords_1000 (9.7 KB)", 0);
    bench_file("keywords_10000.edn", "keywords_10000 (117 KB)", 0);

    /* Integers */
    printf("\n--- Integer Arrays ---\n");
    bench_file("ints_1400.edn", "ints_1400 (10 KB)", 0);

    /* Strings */
    printf("\n--- String Collections ---\n");
    bench_file("strings_1000.edn", "strings_1000 (55 KB)", 0);
    bench_file("strings_uni_250.edn", "strings_uni_250 (56 KB)", 0);

    /* Deeply nested */
    printf("\n--- Nested Structures ---\n");
    bench_file("nested_100000.edn", "nested_100000 (96 KB)", 0);

    printf("\n\n=== ROUNDTRIP MODE (Parse + Free) ===\n");
    printf("Measures complete roundtrip including memory cleanup\n\n");
    bench_print_header();

    /* Basic maps with increasing size */
    printf("\n--- Basic Maps ---\n");
    bench_file("basic_10.edn", "basic_10 (9 bytes)", 1);
    bench_file("basic_100.edn", "basic_100 (97 bytes)", 1);
    bench_file("basic_1000.edn", "basic_1000 (898 bytes)", 1);
    bench_file("basic_10000.edn", "basic_10000 (10 KB)", 1);
    bench_file("basic_100000.edn", "basic_100000 (99 KB)", 1);

    /* Keywords */
    printf("\n--- Keyword Vectors ---\n");
    bench_file("keywords_10.edn", "keywords_10 (116 bytes)", 1);
    bench_file("keywords_100.edn", "keywords_100 (886 bytes)", 1);
    bench_file("keywords_1000.edn", "keywords_1000 (9.7 KB)", 1);
    bench_file("keywords_10000.edn", "keywords_10000 (117 KB)", 1);

    /* Integers */
    printf("\n--- Integer Arrays ---\n");
    bench_file("ints_1400.edn", "ints_1400 (10 KB)", 1);

    /* Strings */
    printf("\n--- String Collections ---\n");
    bench_file("strings_1000.edn", "strings_1000 (55 KB)", 1);
    bench_file("strings_uni_250.edn", "strings_uni_250 (56 KB)", 1);

    /* Deeply nested */
    printf("\n--- Nested Structures ---\n");
    bench_file("nested_100000.edn", "nested_100000 (96 KB)", 1);

    printf("\n");
    printf("Notes:\n");
    printf("  - Parse-only: Measures pure parsing performance (cleanup deferred)\n");
    printf("  - Roundtrip: Includes both parsing and memory cleanup\n");
    printf("  - Each benchmark runs for minimum 500ms or 1000 iterations\n");
    printf("  - Warmup: 3 iterations before measurement\n");
    printf("  - GB/s calculated as: (iterations × file_size) / time / 1024³\n");
    printf("  - Difference shows memory allocator overhead\n");

    return 0;
}
