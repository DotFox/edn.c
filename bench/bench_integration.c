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

/* Benchmark function that parses EDN and frees result */
static int bench_parse(const char* data, size_t size) {
    edn_result_t result = edn_parse(data, size);
    if (result.error != EDN_OK) {
        return 0;
    }
    edn_free(result.value);
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

/* Benchmark a single file */
static void bench_file(const char* filename, const char* description) {
    char path[256];
    snprintf(path, sizeof(path), "bench/data/%s", filename);

    size_t size;
    char* data = read_file(path, &size);
    if (!data) {
        printf("%-40s FAILED (could not read file)\n", description);
        return;
    }

    /* Run benchmark: minimum 100ms or 10 iterations */
    bench_result_t result = bench_run(description, data, size, 500, 1000, bench_parse);
    bench_print_result(description, result);

    free(data);
}

int main(void) {
    printf("EDN.C Integration Benchmarks\n");
    printf("============================\n\n");

    bench_print_header();

    /* Basic maps with increasing size */
    printf("\n--- Basic Maps ---\n");
    bench_file("basic_10.edn", "basic_10 (9 bytes)");
    bench_file("basic_100.edn", "basic_100 (97 bytes)");
    bench_file("basic_1000.edn", "basic_1000 (898 bytes)");
    bench_file("basic_10000.edn", "basic_10000 (10 KB)");
    bench_file("basic_100000.edn", "basic_100000 (99 KB)");

    /* Keywords */
    printf("\n--- Keyword Vectors ---\n");
    bench_file("keywords_10.edn", "keywords_10 (116 bytes)");
    bench_file("keywords_100.edn", "keywords_100 (886 bytes)");
    bench_file("keywords_1000.edn", "keywords_1000 (9.7 KB)");
    bench_file("keywords_10000.edn", "keywords_10000 (117 KB)");

    /* Integers */
    printf("\n--- Integer Arrays ---\n");
    bench_file("ints_1400.edn", "ints_1400 (10 KB)");

    /* Strings */
    printf("\n--- String Collections ---\n");
    bench_file("strings_1000.edn", "strings_1000 (55 KB)");
    bench_file("strings_uni_250.edn", "strings_uni_250 (56 KB)");

    /* Deeply nested */
    printf("\n--- Nested Structures ---\n");
    bench_file("nested_100000.edn", "nested_100000 (96 KB)");

    printf("\n");
    printf("Notes:\n");
    printf("  - Throughput includes parsing and freeing the result\n");
    printf("  - Each benchmark runs for minimum 500ms or 1000 iterations\n");
    printf("  - Warmup: 3 iterations before measurement\n");
    printf("  - GB/s calculated as: (iterations × file_size) / time / 1024³\n");

    return 0;
}
