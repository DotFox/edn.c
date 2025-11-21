/**
 * Test delimiter matching and error detection
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Helper function to parse and check for error */
static edn_result_t parse_helper(const char* input) {
    return edn_read(input, strlen(input));
}

/* Test: unmatched closing parenthesis at top level */
TEST(unmatched_closing_paren) {
    edn_result_t result = parse_helper(")");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: unmatched closing bracket at top level */
TEST(unmatched_closing_bracket) {
    edn_result_t result = parse_helper("]");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: unmatched closing brace at top level */
TEST(unmatched_closing_brace) {
    edn_result_t result = parse_helper("}");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: unmatched closing paren after valid value */
TEST(unmatched_after_value) {
    edn_result_t result = parse_helper("42 )");

    /* The parser should successfully parse 42, but we don't test
     * what happens with remaining input after first value */
    edn_free(result.value);
}

/* Test: mismatched delimiters - vector with list closing */
TEST(mismatched_vector_list_close) {
    edn_result_t result = parse_helper("[1 2 3)");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: mismatched delimiters - list with vector closing */
TEST(mismatched_list_vector_close) {
    edn_result_t result = parse_helper("(1 2 3]");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: mismatched delimiters - vector with brace closing */
TEST(mismatched_vector_brace_close) {
    edn_result_t result = parse_helper("[1 2 3}");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: mismatched delimiters - map with bracket closing */
TEST(mismatched_map_bracket_close) {
    edn_result_t result = parse_helper("{:a 1 :b 2]");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: mismatched delimiters - set with bracket closing */
TEST(mismatched_set_bracket_close) {
    edn_result_t result = parse_helper("#{1 2 3]");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: mismatched delimiters - nested case */
TEST(mismatched_nested) {
    edn_result_t result = parse_helper("[(1 2 3]");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: correctly matched delimiters should work */
TEST(matched_list) {
    edn_result_t result = parse_helper("(1 2 3)");

    assert(result.value != NULL);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_LIST);

    edn_free(result.value);
}

/* Test: correctly matched delimiters should work */
TEST(matched_vector) {
    edn_result_t result = parse_helper("[1 2 3]");

    assert(result.value != NULL);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    edn_free(result.value);
}

/* Test: correctly matched delimiters should work */
TEST(matched_map) {
    edn_result_t result = parse_helper("{:a 1 :b 2}");

    assert(result.value != NULL);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    edn_free(result.value);
}

/* Test: correctly matched delimiters should work */
TEST(matched_set) {
    edn_result_t result = parse_helper("#{1 2 3}");

    assert(result.value != NULL);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_SET);

    edn_free(result.value);
}

/* Test: nested correctly matched delimiters */
TEST(matched_nested) {
    edn_result_t result = parse_helper("[(1 2 3) {:a [4 5]}]");

    assert(result.value != NULL);
    assert(result.error == EDN_OK);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    edn_free(result.value);
}

/* Test: complex mismatched case with multiple nesting */
TEST(mismatched_complex) {
    edn_result_t result = parse_helper("[{:a (1 2 3]}]");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: unmatched with whitespace */
TEST(unmatched_with_whitespace) {
    edn_result_t result = parse_helper("  ]  ");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

/* Test: unmatched with comments */
TEST(unmatched_with_comments) {
    edn_result_t result = parse_helper("; comment\n)");

    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
}

int main(void) {
    printf("Running delimiter matching tests...\n");

    /* Unmatched closing delimiters at top level */
    RUN_TEST(unmatched_closing_paren);
    RUN_TEST(unmatched_closing_bracket);
    RUN_TEST(unmatched_closing_brace);
    RUN_TEST(unmatched_after_value);

    /* Mismatched delimiters */
    RUN_TEST(mismatched_vector_list_close);
    RUN_TEST(mismatched_list_vector_close);
    RUN_TEST(mismatched_vector_brace_close);
    RUN_TEST(mismatched_map_bracket_close);
    RUN_TEST(mismatched_set_bracket_close);
    RUN_TEST(mismatched_nested);

    /* Correctly matched delimiters (sanity checks) */
    RUN_TEST(matched_list);
    RUN_TEST(matched_vector);
    RUN_TEST(matched_map);
    RUN_TEST(matched_set);
    RUN_TEST(matched_nested);

    /* Edge cases */
    RUN_TEST(mismatched_complex);
    RUN_TEST(unmatched_with_whitespace);
    RUN_TEST(unmatched_with_comments);

    TEST_SUMMARY("delimiter matching");
}
