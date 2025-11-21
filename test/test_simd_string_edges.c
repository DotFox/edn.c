/**
 * Edge case tests for SIMD string parser
 */

#include <stdio.h>

#include "../include/edn.h"
#include "test_framework.h"

TEST(escape_at_chunk_boundary_15) {
    /* Escape at position 15 (end of 16-byte chunk) */
    edn_result_t result = edn_read("\"0123456789abcde\\\"fg\"", 0);
    assert(result.error == EDN_OK);
    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert_str_eq(str, "0123456789abcde\"fg");
    assert_uint_eq(len, 18);
    edn_free(result.value);
}

TEST(escape_at_chunk_boundary_16) {
    /* Escape at position 16 (start of second chunk) */
    edn_result_t result = edn_read("\"0123456789abcdef\\\"g\"", 0);
    assert(result.error == EDN_OK);
    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert_str_eq(str, "0123456789abcdef\"g");
    assert_uint_eq(len, 18);
    edn_free(result.value);
}

TEST(quote_at_chunk_boundary_15) {
    /* Quote at position 15 */
    edn_result_t result = edn_read("\"0123456789abcde\"", 0);
    assert(result.error == EDN_OK);
    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_uint_eq(len, 15);
    edn_free(result.value);
}

TEST(quote_at_chunk_boundary_16) {
    /* Quote at position 16 */
    edn_result_t result = edn_read("\"0123456789abcdef\"", 0);
    assert(result.error == EDN_OK);
    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_uint_eq(len, 16);
    edn_free(result.value);
}

TEST(multiple_escapes_across_chunks) {
    /* String > 32 bytes with escapes in multiple chunks */
    edn_result_t result = edn_read("\"0123456789abc\\\"e0123456789abc\\\"e0123456789abc\\\"e\"", 0);
    assert(result.error == EDN_OK);
    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    /* Each \" becomes " so length is 48 - 3 = 45 */
    assert_uint_eq(len, 45);
    edn_free(result.value);
}

TEST(consecutive_escapes) {
    /* \\\" = backslash + quote */
    edn_result_t result = edn_read("\"\\\\\\\"\"", 0);
    assert(result.error == EDN_OK);
    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert_str_eq(str, "\\\"");
    assert_uint_eq(len, 2);
    edn_free(result.value);
}

TEST(escape_at_very_end) {
    /* Backslash at end should escape the closing quote */
    edn_result_t result = edn_read("\"test\\\"\"", 0);
    assert(result.error == EDN_OK);
    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert_str_eq(str, "test\"");
    assert_uint_eq(len, 5);
    edn_free(result.value);
}

TEST(all_escapes_no_regular_chars) {
    edn_result_t result = edn_read("\"\\\\\\\"\\n\\t\\r\"", 0);
    assert(result.error == EDN_OK);
    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert_uint_eq(len, 5); /* \, ", \n, \t, \r */
    /* Verify content too */
    assert(str[0] == '\\');
    assert(str[1] == '"');
    assert(str[2] == '\n');
    assert(str[3] == '\t');
    assert(str[4] == '\r');
    edn_free(result.value);
}

TEST(exactly_16_bytes) {
    /* Exactly 16 bytes - fills one SIMD chunk */
    edn_result_t result = edn_read("\"0123456789abcdef\"", 0);
    assert(result.error == EDN_OK);
    edn_free(result.value);
}

TEST(exactly_32_bytes) {
    /* Exactly 32 bytes - fills two SIMD chunks */
    edn_result_t result = edn_read("\"0123456789abcdef0123456789abcdef\"", 0);
    assert(result.error == EDN_OK);
    edn_free(result.value);
}

int main(void) {
    RUN_TEST(escape_at_chunk_boundary_15);
    RUN_TEST(escape_at_chunk_boundary_16);
    RUN_TEST(quote_at_chunk_boundary_15);
    RUN_TEST(quote_at_chunk_boundary_16);
    RUN_TEST(multiple_escapes_across_chunks);
    RUN_TEST(consecutive_escapes);
    RUN_TEST(escape_at_very_end);
    RUN_TEST(all_escapes_no_regular_chars);
    RUN_TEST(exactly_16_bytes);
    RUN_TEST(exactly_32_bytes);

    TEST_SUMMARY("SIMD string edge cases");
}
