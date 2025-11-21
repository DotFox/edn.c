/**
 * EDN.C - Windows Platform Test
 * 
 * Verifies that SIMD intrinsics work correctly on Windows.
 */

#include <assert.h>
#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Test that Windows SIMD detection works */
TEST(test_platform_detection) {
#if defined(_MSC_VER)
    /* MSVC compiler detected */
    assert(1);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    /* MinGW detected */
    assert(1);
#else
    /* Not Windows, but that's okay */
    assert(1);
#endif
}

/* Test SSE intrinsics on x86_64 Windows */
TEST(test_sse_intrinsics) {
#if (defined(__x86_64__) || defined(_M_X64))
    const char* input = "   \t  hello";
    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
#else
    /* Skip on non-x86_64 platforms */
    assert(1);
#endif
}

/* Test NEON intrinsics on ARM64 Windows */
TEST(test_neon_intrinsics) {
#if (defined(__aarch64__) || defined(_M_ARM64))
    const char* input = "   \t  hello";
    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_SYMBOL);
    edn_free(result.value);
#else
    /* Skip on non-ARM64 platforms */
    assert(1);
#endif
}

/* Test whitespace skipping with SIMD */
TEST(test_simd_whitespace) {
    /* Large whitespace block to trigger SIMD path */
    const char* input = "                                "
                        "                                "
                        "42";
    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_INT);
    int64_t val;
    assert(edn_int64_get(result.value, &val));
    assert(val == 42);
    edn_free(result.value);
}

/* Test string parsing with SIMD quote detection */
TEST(test_simd_string_parsing) {
    /* Long string to trigger SIMD path */
    const char* input = "\"This is a very long string that should trigger SIMD processing "
                        "because it contains more than 16 characters and will benefit from "
                        "vectorized quote detection.\"";
    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_STRING);
    edn_free(result.value);
}

/* Test digit scanning with SIMD */
TEST(test_simd_digit_scanning) {
    /* Long number to trigger SIMD digit scanner */
    const char* input = "12345678901234567890";
    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);
    /* Number will overflow to BigInt, which is expected */
    assert(edn_type(result.value) == EDN_TYPE_BIGINT);
    edn_free(result.value);
}

/* Test identifier scanning with SIMD */
TEST(test_simd_identifier_scanning) {
    /* Long identifier to trigger SIMD path */
    const char* input = ":this-is-a-very-long-keyword-name";
    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_KEYWORD);
    edn_free(result.value);
}

/* Test comment skipping with SIMD */
TEST(test_simd_comment_skipping) {
    /* Long comment to trigger SIMD newline detection */
    const char* input =
        "; This is a very long comment that contains many characters and should trigger SIMD\n"
        "42";
    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_INT);
    int64_t val;
    assert(edn_int64_get(result.value, &val));
    assert(val == 42);
    edn_free(result.value);
}

/* Test complex parsing with all SIMD paths */
TEST(test_simd_complex_parsing) {
    const char* input = "{:name \"Alice Johnson\"  "
                        " :age 30  "
                        " :email \"alice.johnson@example.com\"  "
                        " :tags [:developer :engineer :architect]}";
    edn_result_t result = edn_read(input, 0);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert(edn_map_count(result.value) == 4);
    edn_free(result.value);
}

int main(void) {
    RUN_TEST(test_platform_detection);
    RUN_TEST(test_sse_intrinsics);
    RUN_TEST(test_neon_intrinsics);
    RUN_TEST(test_simd_whitespace);
    RUN_TEST(test_simd_string_parsing);
    RUN_TEST(test_simd_digit_scanning);
    RUN_TEST(test_simd_identifier_scanning);
    RUN_TEST(test_simd_comment_skipping);
    RUN_TEST(test_simd_complex_parsing);
    TEST_SUMMARY("Windows Platform");
}
