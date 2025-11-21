#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/edn_internal.h"
#include "bench_time.h"

/* Benchmark SIMD comment skipping performance */

static void benchmark_short_comments(void) {
    printf("Short comments (< 16 chars):\n");
    const char* input = "; short\n";
    size_t len = strlen(input);
    int iterations = 10000000;

    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        const char* result = edn_simd_skip_whitespace(input, input + len);
        (void) result; /* Prevent optimization */
    }
    double elapsed = get_time() - start;

    printf("  Time: %.3f seconds\n", elapsed);
    printf("  Per iteration: %.2f ns\n", (elapsed / iterations) * 1e9);
    printf("  Throughput: %.0f Mops/sec\n\n", iterations / elapsed / 1e6);
}

static void benchmark_medium_comments(void) {
    printf("Medium comments (50 chars):\n");
    const char* input = "; This is a medium comment with about 50 chars\n";
    size_t len = strlen(input);
    int iterations = 10000000;

    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        const char* result = edn_simd_skip_whitespace(input, input + len);
        (void) result;
    }
    double elapsed = get_time() - start;

    printf("  Time: %.3f seconds\n", elapsed);
    printf("  Per iteration: %.2f ns\n", (elapsed / iterations) * 1e9);
    printf("  Throughput: %.0f Mops/sec\n\n", iterations / elapsed / 1e6);
}

static void benchmark_long_comments(void) {
    printf("Long comments (200 chars):\n");
    /* 200 character comment */
    const char* input = "; This is a very long comment that contains many characters to test the "
                        "SIMD optimization for finding newlines in comments. It needs to be long "
                        "enough to benefit from SIMD processing...\n";
    size_t len = strlen(input);
    int iterations = 10000000;

    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        const char* result = edn_simd_skip_whitespace(input, input + len);
        (void) result;
    }
    double elapsed = get_time() - start;

    printf("  Time: %.3f seconds\n", elapsed);
    printf("  Per iteration: %.2f ns\n", (elapsed / iterations) * 1e9);
    printf("  Throughput: %.0f Mops/sec\n\n", iterations / elapsed / 1e6);
}

static void benchmark_whitespace_vs_comments(void) {
    printf("Comparison: Pure whitespace vs Comments:\n");

    /* Pure whitespace */
    const char* ws_input = "                                                  "; /* 50 spaces */
    size_t ws_len = strlen(ws_input);
    int iterations = 10000000;

    double start = get_time();
    for (int i = 0; i < iterations; i++) {
        const char* result = edn_simd_skip_whitespace(ws_input, ws_input + ws_len);
        (void) result;
    }
    double ws_elapsed = get_time() - start;

    /* Comment with same length */
    const char* comment_input = "; .............................................\n"; /* ~50 chars */
    size_t comment_len = strlen(comment_input);

    start = get_time();
    for (int i = 0; i < iterations; i++) {
        const char* result = edn_simd_skip_whitespace(comment_input, comment_input + comment_len);
        (void) result;
    }
    double comment_elapsed = get_time() - start;

    printf("  Pure whitespace: %.2f ns/op\n", (ws_elapsed / iterations) * 1e9);
    printf("  Comment:         %.2f ns/op\n", (comment_elapsed / iterations) * 1e9);
    printf("  Ratio:           %.2fx\n\n", comment_elapsed / ws_elapsed);
}

int main(void) {
    printf("EDN.C SIMD Comment Skipping Benchmark\n");
    printf("======================================\n\n");

    printf("Warming up...\n");
    for (int i = 0; i < 100000; i++) {
        const char* test = "; warm up comment\n";
        edn_simd_skip_whitespace(test, test + strlen(test));
    }
    printf("\n");

    benchmark_short_comments();
    benchmark_medium_comments();
    benchmark_long_comments();
    benchmark_whitespace_vs_comments();

    printf("Note: SIMD acceleration is most beneficial for long comments (50+ chars)\n");

    return 0;
}
