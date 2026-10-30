#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Test string parsing with zero-copy lazy decoding */

/* Test SIMD find quote function */
TEST(simd_find_quote_simple) {
    const char* input = "hello world\"";
    bool has_escape = false;
    const char* result = edn_simd_find_quote(input, input + strlen(input), &has_escape);
    assert(has_escape == false); /* No backslash in this string */
    assert(result != NULL);
    assert(*result == '"');
    assert(result == input + 11);
}

TEST(simd_find_quote_with_escape) {
    const char* input = "hello \\\" world\"";
    bool has_escape = false;
    const char* result = edn_simd_find_quote(input, input + strlen(input), &has_escape);
    assert(has_escape == true);
    assert(result != NULL);
    assert(*result == '"');
    assert(result == input + 14); /* After escaped quote */
}

TEST(simd_find_quote_not_found) {
    const char* input = "hello world";
    bool has_escape = false;
    const char* result = edn_simd_find_quote(input, input + strlen(input), &has_escape);
    assert(has_escape == false);
    assert(result == NULL);
}

TEST(simd_find_quote_backslash_after_quote) {
    /* body of "abc", followed by , "d\ne"] and padding -> >= 16 bytes */
    const char* input = "abc\",\"d\\ne\"]xxxxx";
    bool has_escape = true;
    const char* result = edn_simd_find_quote(input, input + strlen(input), &has_escape);
    assert(result != NULL);
    assert(*result == '"');
    assert(result == input + 3);
    assert(has_escape == false);
}

TEST(simd_find_quote_simd_block_escape) {
    const char* input = "abcdef\\\"ghijklmn\"";
    bool has_escape = false;
    const char* result = edn_simd_find_quote(input, input + strlen(input), &has_escape);
    assert(result != NULL);
    assert(*result == '"');
    assert(result == input + 16);
    assert(has_escape == true);
}

/* Test string parsing via edn_read */
TEST(parse_string_simple) {
    const char* input = "\"hello\"";
    edn_result_t result = edn_read(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t length;
    const char* str = edn_string_get(result.value, &length);
    assert(str != NULL);
    assert(length == 5);
    assert(strcmp(str, "hello") == 0);

    edn_free(result.value);
}

TEST(parse_string_empty) {
    const char* input = "\"\"";
    edn_result_t result = edn_read(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t length;
    const char* str = edn_string_get(result.value, &length);
    assert(str != NULL);
    assert(length == 0);
    assert(strcmp(str, "") == 0);

    edn_free(result.value);
}

TEST(parse_string_with_escapes) {
    const char* input = "\"hello\\nworld\"";
    edn_result_t result = edn_read(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t length;
    const char* str = edn_string_get(result.value, &length);
    assert(str != NULL);
    assert(length == 11); /* "hello\nworld" = 11 chars after decoding */
    assert(strcmp(str, "hello\nworld") == 0);

    edn_free(result.value);
}

TEST(parse_string_with_escaped_quote) {
    const char* input = "\"hello \\\" world\"";
    edn_result_t result = edn_read(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t length;
    const char* str = edn_string_get(result.value, &length);
    assert(str != NULL);
    assert(length == 13); /* "hello " world" = 13 chars after decoding */
    assert(strcmp(str, "hello \" world") == 0);

    edn_free(result.value);
}

TEST(parse_string_unterminated) {
    const char* input = "\"hello world";
    edn_result_t result = edn_read(input, 0);

    assert(result.error != EDN_OK);
    assert(result.value == NULL);
}

TEST(parse_string_long) {
    /* Test SIMD path with 50+ character string */
    const char* input = "\"This is a very long string that will test the SIMD path\"";
    edn_result_t result = edn_read(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t length;
    const char* str = edn_string_get(result.value, &length);
    assert(str != NULL);
    assert(length == 55); /* 55 chars */
    assert(strcmp(str, "This is a very long string that will test the SIMD path") == 0);

    edn_free(result.value);
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

TEST(decode_string_basic_escapes) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\\"\\\\\\n\\t\\r";
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert(strcmp(result, "\"\\\n\t\r") == 0);

    edn_arena_destroy(arena);
}

#ifdef EDN_ENABLE_CLOJURE_EXTENSION

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

TEST(decode_string_octal_null) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "hello\\0world";
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    /* \0 = null character (0x00) */
    assert(strncmp(result, "hello", 5) == 0);
    assert(result[5] == '\0'); /* The embedded null */
    /* Note: strcmp won't work here due to embedded null */

    edn_arena_destroy(arena);
}

TEST(decode_string_octal_single_digit) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\7"; /* 7 octal = 7 decimal */
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert((unsigned char) result[0] == 7);
    assert(result[1] == '\0');

    edn_arena_destroy(arena);
}

TEST(decode_string_octal_two_digits) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\77"; /* 77 octal = 63 decimal = '?' */
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert(result[0] == '?'); /* 63 = '?' */
    assert(result[1] == '\0');

    edn_arena_destroy(arena);
}

TEST(decode_string_octal_three_digits) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\101"; /* 101 octal = 65 decimal = 'A' */
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert(result[0] == 'A');
    assert(result[1] == '\0');

    edn_arena_destroy(arena);
}

TEST(decode_string_octal_max_value) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\377"; /* 377 octal = 255 decimal */
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert((unsigned char) result[0] == 255);
    assert(result[1] == '\0');

    edn_arena_destroy(arena);
}

TEST(decode_string_octal_overflow_stops) {
    edn_arena_t* arena = edn_arena_create();
    /* \400 would be 256, which is > 255, so it should parse as \40 (32) followed by '0' */
    const char* input = "\\400";
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert((unsigned char) result[0] == 32); /* \40 = 32 decimal (space) */
    assert(result[1] == '0');                /* literal '0' */
    assert(result[2] == '\0');

    edn_arena_destroy(arena);
}

TEST(decode_string_octal_non_octal_stops) {
    edn_arena_t* arena = edn_arena_create();
    /* \18 should parse as \1 followed by '8' (8 is not octal) */
    const char* input = "\\18";
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert((unsigned char) result[0] == 1); /* \1 = 1 */
    assert(result[1] == '8');               /* literal '8' */
    assert(result[2] == '\0');

    edn_arena_destroy(arena);
}

TEST(decode_string_octal_mixed) {
    edn_arena_t* arena = edn_arena_create();
    const char* input = "\\110\\145\\154\\154\\157"; /* "Hello" in octal */
    char* result = edn_decode_string(arena, input, strlen(input));

    assert(result != NULL);
    assert(strcmp(result, "Hello") == 0);

    edn_arena_destroy(arena);
}

#endif /* EDN_ENABLE_CLOJURE_EXTENSION */

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

    /* Both with and without Clojure extension, \u with not enough digits should fail */
    assert(result == NULL);

    edn_arena_destroy(arena);
}

TEST(string_get_cached_byte_length) {
    const struct {
        const char* input;
        const char* expected;
        size_t length;
    } cases[] = {
        {"\"\"", "", 0},
        {"\"plain\"", "plain", 5},
        {"\"a\\nb\\t\"", "a\nb\t", 4},
        {"\"\\\"\\\\\"", "\"\\", 2},
        {"\"\xc3\xa9\\n\"", "\xc3\xa9\n", 3},
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
        {"\"\\u00e9\\u4e2d\"", "\xc3\xa9\xe4\xb8\xad", 5},
        {"\"a\\u0000b\"", "a\0b", 3},
        {"\"\\0xyz\"", "\0xyz", 4},
        {"\"\\000\"", "\0", 1},
#endif
#ifdef EDN_ENABLE_EXPERIMENTAL_EXTENSION
        {"\"\"\"\nhello\n\"\"\"", "hello\n", 6},
        {"\"\"\"\na\\tb\n\"\"\"", "a\\tb\n", 5},
        {"\"\"\"\na\\\"\"\"b\n\"\"\"", "a\"\"\"b\n", 6},
#endif
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        for (int request_length = 0; request_length <= 1; request_length++) {
            edn_result_t result = edn_read(cases[i].input, 0);
            assert_int_eq(result.error, EDN_OK);
            size_t raw_length = edn_string_get_length(result.value);
            size_t first_length = SIZE_MAX;
            const char* first = edn_string_get(result.value, request_length ? &first_length : NULL);
            assert(first != NULL);
            if (request_length) {
                assert_uint_eq(first_length, cases[i].length);
            }
            for (int repeat = 0; repeat < 3; repeat++) {
                size_t length = SIZE_MAX;
                const char* cached = edn_string_get(result.value, &length);
                assert_ptr_eq(cached, first);
                assert_uint_eq(length, cases[i].length);
                assert(memcmp(cached, cases[i].expected, length) == 0);
                assert(cached[length] == '\0');
                assert_uint_eq(edn_string_get_length(result.value), raw_length);
            }
            edn_free(result.value);
        }
    }
}

TEST(string_get_failed_decode_length) {
    edn_result_t result = edn_read("\"bad\\q\"", 0);
    assert_int_eq(result.error, EDN_OK);
    for (int repeat = 0; repeat < 2; repeat++) {
        size_t length = SIZE_MAX;
        assert(edn_string_get(result.value, &length) == NULL);
        assert_uint_eq(length, 0);
    }
    edn_free(result.value);
}

int main(void) {
    printf("Running string parsing tests...\n");

    /* SIMD function tests */
    run_test_simd_find_quote_simple();
    run_test_simd_find_quote_with_escape();
    run_test_simd_find_quote_not_found();
    run_test_simd_find_quote_backslash_after_quote();
    run_test_simd_find_quote_simd_block_escape();

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
    run_test_decode_string_basic_escapes();
#ifdef EDN_ENABLE_CLOJURE_EXTENSION
    run_test_decode_string_all_escapes();
    run_test_decode_string_unicode_ascii();
    run_test_decode_string_unicode_2byte();
    run_test_decode_string_unicode_3byte();
    run_test_decode_string_unicode_mixed();
    run_test_decode_string_octal_null();
    run_test_decode_string_octal_single_digit();
    run_test_decode_string_octal_two_digits();
    run_test_decode_string_octal_three_digits();
    run_test_decode_string_octal_max_value();
    run_test_decode_string_octal_overflow_stops();
    run_test_decode_string_octal_non_octal_stops();
    run_test_decode_string_octal_mixed();
#endif
    run_test_decode_string_invalid_escape();
    run_test_decode_string_invalid_unicode();
    RUN_TEST(string_get_cached_byte_length);
    RUN_TEST(string_get_failed_decode_length);

    TEST_SUMMARY("string parsing");
}
