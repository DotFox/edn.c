#include <string.h>

#include "../include/edn.h"
#include "../src/edn_internal.h"
#include "test_framework.h"

/* Test parser dispatcher - edn_parser_parse_value */

/* Test parsing nil */
TEST(parse_nil) {
    edn_result_t result = edn_read("nil", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_NIL);
    edn_free(result.value);
}

/* Test parsing booleans */
TEST(parse_true) {
    edn_result_t result = edn_read("true", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_BOOL);
    assert(result.value->as.boolean == true);
    edn_free(result.value);
}

TEST(parse_false) {
    edn_result_t result = edn_read("false", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_BOOL);
    assert(result.value->as.boolean == false);
    edn_free(result.value);
}

/* Test parsing numbers */
TEST(parse_positive_int) {
    edn_result_t result = edn_read("42", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_INT);
    assert(result.value->as.integer == 42);
    edn_free(result.value);
}

TEST(parse_negative_int) {
    edn_result_t result = edn_read("-123", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_INT);
    assert(result.value->as.integer == -123);
    edn_free(result.value);
}

TEST(parse_float) {
    edn_result_t result = edn_read("3.14", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_FLOAT);
    edn_free(result.value);
}

/* Test parsing characters */
TEST(parse_character_single) {
    edn_result_t result = edn_read("\\a", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == 'a');

    edn_free(result.value);
}

TEST(parse_character_named) {
    edn_result_t result = edn_read("\\newline", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_CHARACTER);

    uint32_t codepoint;
    assert(edn_character_get(result.value, &codepoint));
    assert(codepoint == '\n');

    edn_free(result.value);
}

/* Test parsing strings */
TEST(parse_string_simple) {
    edn_result_t result = edn_read("\"hello\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_STRING);

    size_t length;
    const char* str = edn_string_get(result.value, &length);
    assert(str != NULL);
    assert(length == 5);
    assert(strcmp(str, "hello") == 0);

    edn_free(result.value);
}

TEST(parse_string_with_escapes) {
    edn_result_t result = edn_read("\"hello\\nworld\"", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_STRING);

    const char* str = edn_string_get(result.value, NULL);
    assert(str != NULL);
    assert(strcmp(str, "hello\nworld") == 0);

    edn_free(result.value);
}

/* Test parsing symbols */
TEST(parse_symbol_simple) {
    edn_result_t result = edn_read("foo", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_SYMBOL);

    const char* name;
    size_t name_len;
    assert(edn_symbol_get(result.value, NULL, NULL, &name, &name_len));
    assert(name_len == 3);
    assert(memcmp(name, "foo", 3) == 0);

    edn_free(result.value);
}

TEST(parse_symbol_plus) {
    edn_result_t result = edn_read("+", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

TEST(parse_symbol_minus) {
    edn_result_t result = edn_read("-", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_SYMBOL);
    edn_free(result.value);
}

/* Test parsing keywords */
TEST(parse_keyword_simple) {
    edn_result_t result = edn_read(":foo", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_KEYWORD);

    const char* name;
    size_t name_len;
    assert(edn_keyword_get(result.value, NULL, NULL, &name, &name_len));
    assert(name_len == 3);
    assert(memcmp(name, "foo", 3) == 0);

    edn_free(result.value);
}

TEST(parse_keyword_namespaced) {
    edn_result_t result = edn_read(":foo/bar", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_KEYWORD);

    const char* namespace, *name;
    size_t ns_len, name_len;
    assert(edn_keyword_get(result.value, &namespace, &ns_len, &name, &name_len));
    assert(ns_len == 3);
    assert(memcmp(namespace, "foo", 3) == 0);
    assert(name_len == 3);
    assert(memcmp(name, "bar", 3) == 0);

    edn_free(result.value);
}

/* Test whitespace handling */
TEST(parse_with_leading_whitespace) {
    edn_result_t result = edn_read("   42", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_INT);
    assert(result.value->as.integer == 42);
    edn_free(result.value);
}

TEST(parse_with_comment) {
    edn_result_t result = edn_read("; comment\n42", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_INT);
    assert(result.value->as.integer == 42);
    edn_free(result.value);
}

/* Test symbolic values */
TEST(parse_symbolic_inf) {
    edn_result_t result = edn_read("##Inf", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_FLOAT);
    edn_free(result.value);
}

TEST(parse_symbolic_nan) {
    edn_result_t result = edn_read("##NaN", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_FLOAT);
    edn_free(result.value);
}

/* Test collection parsing */
TEST(parse_list_implemented) {
    edn_result_t result = edn_read("(1 2 3)", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_LIST);
    edn_free(result.value);
}

TEST(parse_vector_implemented) {
    edn_result_t result = edn_read("[1 2 3]", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_VECTOR);
    edn_free(result.value);
}

TEST(parse_map_implemented) {
    edn_result_t result = edn_read("{:a 1}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_MAP);
    edn_free(result.value);
}

TEST(parse_set_implemented) {
    edn_result_t result = edn_read("#{1 2 3}", 0);
    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(result.value->type == EDN_TYPE_SET);
    edn_free(result.value);
}

/* Test empty input */
TEST(parse_empty_input) {
    edn_result_t result = edn_read("", 0);
    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
}

TEST(parse_whitespace_only) {
    edn_result_t result = edn_read("   \n\t  ", 0);
    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
}

int main(void) {
    printf("Running parser dispatcher tests...\n");

    /* nil and booleans */
    RUN_TEST(parse_nil);
    RUN_TEST(parse_true);
    RUN_TEST(parse_false);

    /* Numbers */
    RUN_TEST(parse_positive_int);
    RUN_TEST(parse_negative_int);
    RUN_TEST(parse_float);

    /* Characters */
    RUN_TEST(parse_character_single);
    RUN_TEST(parse_character_named);

    /* Strings */
    RUN_TEST(parse_string_simple);
    RUN_TEST(parse_string_with_escapes);

    /* Symbols */
    RUN_TEST(parse_symbol_simple);
    RUN_TEST(parse_symbol_plus);
    RUN_TEST(parse_symbol_minus);

    /* Keywords */
    RUN_TEST(parse_keyword_simple);
    RUN_TEST(parse_keyword_namespaced);

    /* Whitespace handling */
    RUN_TEST(parse_with_leading_whitespace);
    RUN_TEST(parse_with_comment);

    /* Symbolic values */
    RUN_TEST(parse_symbolic_inf);
    RUN_TEST(parse_symbolic_nan);

    /* Collection parsing */
    RUN_TEST(parse_list_implemented);
    RUN_TEST(parse_vector_implemented);
    RUN_TEST(parse_map_implemented);
    RUN_TEST(parse_set_implemented);

    /* Edge cases */
    RUN_TEST(parse_empty_input);
    RUN_TEST(parse_whitespace_only);

    TEST_SUMMARY("parser dispatcher");
}
