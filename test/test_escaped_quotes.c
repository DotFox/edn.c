/**
 * test_escaped_quotes.c - Tests for strings with escaped quotes
 * 
 * This test ensures that strings containing escaped quotes (\") are parsed correctly.
 * Previously, the SIMD string parser had a bug where it would incorrectly terminate
 * strings at escaped quotes.
 */

#include <stdio.h>
#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

TEST(simple_escaped_quote) {
    edn_result_t result = edn_parse("{:key \"\\\"value\\\"\"}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    edn_result_t key = edn_parse(":key", 0);
    edn_value_t* val = edn_map_lookup(result.value, key.value);
    assert(val != NULL);

    size_t len;
    const char* str = edn_string_get(val, &len);
    assert(str != NULL);
    assert_str_eq(str, "\"value\"");
    assert_uint_eq(len, 7);

    edn_free(key.value);
    edn_free(result.value);
}

TEST(multiple_escaped_quotes) {
    edn_result_t result = edn_parse("\"\\\"a\\\" \\\"b\\\" \\\"c\\\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_STRING);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_str_eq(str, "\"a\" \"b\" \"c\"");

    edn_free(result.value);
}

TEST(escaped_quote_at_beginning) {
    edn_result_t result = edn_parse("\"\\\"hello\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_str_eq(str, "\"hello");

    edn_free(result.value);
}

TEST(escaped_quote_at_end) {
    edn_result_t result = edn_parse("\"hello\\\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_str_eq(str, "hello\"");

    edn_free(result.value);
}

TEST(complex_escaped_string) {
    /* This is the actual problematic string from nested_100000.edn */
    edn_result_t result = edn_parse("{:dmV \"\\\"IP$+.o`'82$_1\\\"?\"}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    edn_result_t key = edn_parse(":dmV", 0);
    edn_value_t* val = edn_map_lookup(result.value, key.value);
    assert(val != NULL);

    size_t len;
    const char* str = edn_string_get(val, &len);
    assert(str != NULL);
    assert_str_eq(str, "\"IP$+.o`'82$_1\"?");
    assert_uint_eq(len, 16);

    edn_free(key.value);
    edn_free(result.value);
}

TEST(escaped_backslash_then_quote) {
    edn_result_t result = edn_parse("\"\\\\\\\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_str_eq(str, "\\\"");
    assert_uint_eq(len, 2);

    edn_free(result.value);
}

TEST(mixed_escapes_with_quotes) {
    edn_result_t result = edn_parse("\"line1\\n\\\"quoted\\\"\\tline2\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    /* Should be: line1\n"quoted"\tline2 */
    assert_uint_eq(len, 20);

    edn_free(result.value);
}

TEST(only_escaped_quotes) {
    edn_result_t result = edn_parse("\"\\\"\\\"\\\"\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    assert_str_eq(str, "\"\"\"");
    assert_uint_eq(len, 3);

    edn_free(result.value);
}

TEST(long_string_with_escaped_quotes) {
    /* Test with a string longer than SIMD chunk size (16 bytes) */
    edn_result_t result = edn_parse(
        "\"This is a long string with \\\"escaped quotes\\\" in the middle and at the end\\\"\"",
        0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    size_t len;
    const char* str = edn_string_get(result.value, &len);
    assert(str != NULL);
    /* String has 3 escaped quotes (\"), decoded has 3 literal quotes (") */
    assert_uint_eq(len, 73);

    edn_free(result.value);
}

int main(void) {
    RUN_TEST(simple_escaped_quote);
    RUN_TEST(multiple_escaped_quotes);
    RUN_TEST(escaped_quote_at_beginning);
    RUN_TEST(escaped_quote_at_end);
    RUN_TEST(complex_escaped_string);
    RUN_TEST(escaped_backslash_then_quote);
    RUN_TEST(mixed_escapes_with_quotes);
    RUN_TEST(only_escaped_quotes);
    RUN_TEST(long_string_with_escaped_quotes);

    TEST_SUMMARY("escaped quotes");
}
