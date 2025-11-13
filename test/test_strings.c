#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Test string parsing with zero-copy lazy decoding */

/* Test SIMD find quote function */
TEST(simd_find_quote_simple) {
    const char* input = "hello world\"";
    const char* result = edn_simd_find_quote(input, input + strlen(input));
    assert(result != NULL);
    assert(*result == '"');
    assert(result == input + 11);
}

TEST(simd_find_quote_with_escape) {
    const char* input = "hello \\\" world\"";
    const char* result = edn_simd_find_quote(input, input + strlen(input));
    assert(result != NULL);
    assert(*result == '"');
    assert(result == input + 14); /* After escaped quote */
}

TEST(simd_find_quote_not_found) {
    const char* input = "hello world";
    const char* result = edn_simd_find_quote(input, input + strlen(input));
    assert(result == NULL);
}

TEST(simd_has_backslash_true) {
    const char* input = "hello\\nworld";
    bool result = edn_simd_has_backslash(input, strlen(input));
    assert(result == true);
}

TEST(simd_has_backslash_false) {
    const char* input = "hello world";
    bool result = edn_simd_has_backslash(input, strlen(input));
    assert(result == false);
}

TEST(simd_has_backslash_long) {
    /* Test SIMD path with 20+ characters */
    const char* input = "hello world this is long\\n";
    bool result = edn_simd_has_backslash(input, strlen(input));
    assert(result == true);
}

/* Test lazy string parsing */
TEST(parse_string_simple) {
    const char* input = "\"hello\"";
    edn_string_scan_t result = edn_parse_string_lazy(input, input + strlen(input));

    assert(result.valid == true);
    assert(result.has_escapes == false);
    assert(result.end - result.start == 5);
    assert(strncmp(result.start, "hello", 5) == 0);
}

TEST(parse_string_empty) {
    const char* input = "\"\"";
    edn_string_scan_t result = edn_parse_string_lazy(input, input + strlen(input));

    assert(result.valid == true);
    assert(result.has_escapes == false);
    assert(result.end - result.start == 0);
}

TEST(parse_string_with_escapes) {
    const char* input = "\"hello\\nworld\"";
    edn_string_scan_t result = edn_parse_string_lazy(input, input + strlen(input));

    assert(result.valid == true);
    assert(result.has_escapes == true);
    assert(result.end - result.start == 12);
}

TEST(parse_string_with_escaped_quote) {
    const char* input = "\"hello \\\" world\"";
    edn_string_scan_t result = edn_parse_string_lazy(input, input + strlen(input));

    assert(result.valid == true);
    assert(result.has_escapes == true);
}

TEST(parse_string_unterminated) {
    const char* input = "\"hello world";
    edn_string_scan_t result = edn_parse_string_lazy(input, input + strlen(input));

    assert(result.valid == false);
}

TEST(parse_string_long) {
    /* Test SIMD path with 50+ character string */
    const char* input = "\"This is a very long string that will test the SIMD path\"";
    edn_string_scan_t result = edn_parse_string_lazy(input, input + strlen(input));

    assert(result.valid == true);
    assert(result.has_escapes == false);
    assert(result.end - result.start == 55); /* 55 chars (not 56) */
}

/* Test string decoding */
TEST(decode_string_no_escapes) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "hello world";
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert(strcmp(result, "hello world") == 0);

    edn_arena_destroy(arena);
}

TEST(decode_string_newline) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "hello\\nworld";
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert(strcmp(result, "hello\nworld") == 0);

    edn_arena_destroy(arena);
}

TEST(decode_string_tab) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "hello\\tworld";
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert(strcmp(result, "hello\tworld") == 0);

    edn_arena_destroy(arena);
}

TEST(decode_string_all_escapes) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\\"\\\\\\n\\t\\r\\f\\b";
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert(strcmp(result, "\"\\\n\t\r\f\b") == 0);

    edn_arena_destroy(arena);
}

TEST(decode_string_unicode_ascii) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\u0041"; /* 'A' */
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert(strcmp(result, "A") == 0);

    edn_arena_destroy(arena);
}

TEST(decode_string_unicode_2byte) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\u00E9"; /* é (e-acute) */
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    /* é in UTF-8 is 0xC3 0xA9 */
    assert((unsigned char) result[0] == 0xC3);
    assert((unsigned char) result[1] == 0xA9);
    assert(result[2] == '\0');

    edn_arena_destroy(arena);
}

TEST(decode_string_unicode_3byte) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\u2764"; /* ❤ (heart) */
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    /* ❤ in UTF-8 is 0xE2 0x9D 0xA4 */
    assert((unsigned char) result[0] == 0xE2);
    assert((unsigned char) result[1] == 0x9D);
    assert((unsigned char) result[2] == 0xA4);
    assert(result[3] == '\0');

    edn_arena_destroy(arena);
}

TEST(decode_string_unicode_mixed) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "Hello \\u0041\\u00E9\\u2764";
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert(strncmp(result, "Hello A", 7) == 0);

    edn_arena_destroy(arena);
}

TEST(decode_string_invalid_escape) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "hello\\xworld"; /* \x is not valid */
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result == NULL); /* Should fail */

    edn_arena_destroy(arena);
}

TEST(decode_string_invalid_unicode) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\u123"; /* Not enough digits */
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result == NULL); /* Should fail */

    edn_arena_destroy(arena);
}

int main(void) {
    printf("Running string parsing tests...\n");

    /* SIMD function tests */
    run_test_simd_find_quote_simple();
    run_test_simd_find_quote_with_escape();
    run_test_simd_find_quote_not_found();
    run_test_simd_has_backslash_true();
    run_test_simd_has_backslash_false();
    run_test_simd_has_backslash_long();

    /* Lazy parsing tests */
    run_test_parse_string_simple();
    run_test_parse_string_empty();
    run_test_parse_string_with_escapes();
    run_test_parse_string_with_escaped_quote();
    run_test_parse_string_unterminated();
    run_test_parse_string_long();

    /* Decoding tests */
    run_test_decode_string_no_escapes();
    run_test_decode_string_newline();
    run_test_decode_string_tab();
    run_test_decode_string_all_escapes();
    run_test_decode_string_unicode_ascii();
    run_test_decode_string_unicode_2byte();
    run_test_decode_string_unicode_3byte();
    run_test_decode_string_unicode_mixed();
    run_test_decode_string_invalid_escape();
    run_test_decode_string_invalid_unicode();

    TEST_SUMMARY("string parsing");
}
