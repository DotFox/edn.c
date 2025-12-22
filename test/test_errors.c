/* test_errors.c - Tests for EDN parsing error cases
 */

#include "../include/edn.h"
#include "test_framework.h"

/* ========================================================================
 * Unterminated Collections
 * ======================================================================== */

/* Unterminated List */

TEST(unterminated_list_empty) {
    /* Input: "(" - error_start at '(' (offset 0), error_end at EOF (offset 1) */
    edn_result_t result = edn_read("(", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 1);
}

TEST(unterminated_list_with_elements) {
    /* Input: "(1 2 3" - error_start at '(' (offset 0), error_end at EOF (offset 6) */
    edn_result_t result = edn_read("(1 2 3", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(unterminated_list_nested) {
    /* Input: "(1 (2 3)" - outer list unterminated, error_start at outer '(' (offset 0) */
    edn_result_t result = edn_read("(1 (2 3)", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 8);
}

TEST(unterminated_list_with_whitespace) {
    /* Input: "(   " - error_start at '(' (offset 0), error_end at EOF (offset 4) */
    edn_result_t result = edn_read("(   ", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 4);
}

/* Unterminated Vector */

TEST(unterminated_vector_empty) {
    /* Input: "[" - error_start at '[' (offset 0), error_end at EOF (offset 1) */
    edn_result_t result = edn_read("[", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 1);
}

TEST(unterminated_vector_with_elements) {
    /* Input: "[1 2 3" - error_start at '[' (offset 0), error_end at EOF (offset 6) */
    edn_result_t result = edn_read("[1 2 3", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(unterminated_vector_nested) {
    /* Input: "[1 [2 3]" - outer vector unterminated, error_start at outer '[' (offset 0) */
    edn_result_t result = edn_read("[1 [2 3]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 8);
}

TEST(unterminated_vector_with_whitespace) {
    /* Input: "[   " - error_start at '[' (offset 0), error_end at EOF (offset 4) */
    edn_result_t result = edn_read("[   ", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 4);
}

/* Unterminated Map */

TEST(unterminated_map_empty) {
    /* Input: "{" - error_start at '{' (offset 0), error_end at EOF (offset 1) */
    edn_result_t result = edn_read("{", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 1);
}

TEST(unterminated_map_with_key) {
    /* Input: "{:key" - error_start at '{' (offset 0), error_end at EOF (offset 5) */
    edn_result_t result = edn_read("{:key", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 5);
}

TEST(unterminated_map_with_pairs) {
    /* Input: "{:a 1 :b 2" - error_start at '{' (offset 0), error_end at EOF (offset 10) */
    edn_result_t result = edn_read("{:a 1 :b 2", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 10);
}

TEST(unterminated_map_nested) {
    /* Input: "{:a {:b 1}" - outer map unterminated, error_start at outer '{' (offset 0) */
    edn_result_t result = edn_read("{:a {:b 1}", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 10);
}

TEST(unterminated_map_with_whitespace) {
    /* Input: "{   " - error_start at '{' (offset 0), error_end at EOF (offset 4) */
    edn_result_t result = edn_read("{   ", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 4);
}

/* Unterminated Set */

TEST(unterminated_set_empty) {
    /* Input: "#{" - error_start at '#' (offset 0), error_end at EOF (offset 2) */
    edn_result_t result = edn_read("#{", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 2);
}

TEST(unterminated_set_with_elements) {
    /* Input: "#{1 2 3" - error_start at '#' (offset 0), error_end at EOF (offset 7) */
    edn_result_t result = edn_read("#{1 2 3", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(unterminated_set_nested) {
    /* Input: "#{1 #{2 3}" - outer set unterminated, error_start at outer '#' (offset 0) */
    edn_result_t result = edn_read("#{1 #{2 3}", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 10);
}

TEST(unterminated_set_with_whitespace) {
    /* Input: "#{   " - error_start at '#' (offset 0), error_end at EOF (offset 5) */
    edn_result_t result = edn_read("#{   ", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 5);
}

/* Deeply Nested Unterminated Collections */

TEST(unterminated_deeply_nested_list) {
    edn_result_t result = edn_read("(((", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
}

TEST(unterminated_deeply_nested_vector) {
    edn_result_t result = edn_read("[[[", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
}

TEST(unterminated_deeply_nested_map) {
    edn_result_t result = edn_read("{:a {:b {:c", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
}

TEST(unterminated_deeply_nested_set) {
    edn_result_t result = edn_read("#{#{#{", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
}

/* Mixed Nested Unterminated Collections */
/* Note: Error message reports the innermost unterminated collection */

TEST(unterminated_mixed_list_in_vector) {
    edn_result_t result = edn_read("[(", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated list (missing ')')");
}

TEST(unterminated_mixed_vector_in_map) {
    edn_result_t result = edn_read("{:key [", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
}

TEST(unterminated_mixed_map_in_set) {
    edn_result_t result = edn_read("#{1 {", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated map (missing '}')");
}

TEST(unterminated_mixed_set_in_list) {
    edn_result_t result = edn_read("(1 #{", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated set (missing '}')");
}

/* ========================================================================
 * Mismatched Delimiters
 * 
 * For mismatched delimiters:
 * - error_start = opening delimiter
 * - error_end = wrong closing delimiter + 1
 * ======================================================================== */

TEST(mismatched_vector_with_brace) {
    /* Input: "[1 2 }" - error_start at '[' (offset 0), error_end after '}' (offset 6) */
    edn_result_t result = edn_read("[1 2 }", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in vector");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(mismatched_list_with_bracket) {
    /* Input: "(1 2 ]" - error_start at '(' (offset 0), error_end after ']' (offset 6) */
    edn_result_t result = edn_read("(1 2 ]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in list");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(mismatched_list_with_brace) {
    /* Input: "(1 2 }" - error_start at '(' (offset 0), error_end after '}' (offset 6) */
    edn_result_t result = edn_read("(1 2 }", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in list");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(mismatched_map_with_paren) {
    /* Input: "{:a 1 )" - error_start at '{' (offset 0), error_end after ')' (offset 7) */
    edn_result_t result = edn_read("{:a 1 )", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in map");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(mismatched_map_with_bracket) {
    /* Input: "{:a 1 ]" - error_start at '{' (offset 0), error_end after ']' (offset 7) */
    edn_result_t result = edn_read("{:a 1 ]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in map");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(mismatched_set_with_bracket) {
    /* Input: "#{1 2 ]" - error_start at '#' (offset 0), error_end after ']' (offset 7) */
    edn_result_t result = edn_read("#{1 2 ]", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in set");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(mismatched_set_with_paren) {
    /* Input: "#{1 2 )" - error_start at '#' (offset 0), error_end after ')' (offset 7) */
    edn_result_t result = edn_read("#{1 2 )", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in set");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 7);
}

TEST(mismatched_vector_with_paren) {
    /* Input: "[1 2 )" - error_start at '[' (offset 0), error_end after ')' (offset 6) */
    edn_result_t result = edn_read("[1 2 )", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in vector");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 6);
}

TEST(mismatched_nested_inner) {
    /* Input: "[[1 2] }" - outer vector mismatched, error_start at outer '[' (offset 0) */
    edn_result_t result = edn_read("[[1 2] }", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in vector");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 8);
}

TEST(mismatched_nested_outer) {
    /* Input: "[(1 2) }" - outer vector mismatched, error_start at outer '[' (offset 0) */
    edn_result_t result = edn_read("[(1 2) }", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in vector");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_end.offset, 8);
}

/* ========================================================================
 * Multi-line Error Positions
 * ======================================================================== */

TEST(mismatched_multiline) {
    /* Input: "[1\n2\n}" - mismatched, error_start at '[' on line 1, error_end at '}' on line 3 */
    edn_result_t result = edn_read("[1\n2\n}", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNMATCHED_DELIMITER);
    assert_str_eq(result.error_message, "Mismatched closing delimiter in vector");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_start.line, 1);
    assert_uint_eq(result.error_end.offset, 6);
    assert_uint_eq(result.error_end.line, 3);
}

TEST(unterminated_multiline) {
    /* Input: "[\n1\n2" - error_start at '[' on line 1, error_end at EOF on line 3 */
    edn_result_t result = edn_read("[\n1\n2", 0);
    assert(result.value == NULL);
    assert(result.error == EDN_ERROR_UNTERMINATED_COLLECTION);
    assert_str_eq(result.error_message, "Unterminated vector (missing ']')");
    assert_uint_eq(result.error_start.offset, 0);
    assert_uint_eq(result.error_start.line, 1);
    assert_uint_eq(result.error_end.offset, 5);
    assert_uint_eq(result.error_end.line, 3);
}

int main(void) {
    printf("Running error tests...\n");

    /* Unterminated List */
    RUN_TEST(unterminated_list_empty);
    RUN_TEST(unterminated_list_with_elements);
    RUN_TEST(unterminated_list_nested);
    RUN_TEST(unterminated_list_with_whitespace);

    /* Unterminated Vector */
    RUN_TEST(unterminated_vector_empty);
    RUN_TEST(unterminated_vector_with_elements);
    RUN_TEST(unterminated_vector_nested);
    RUN_TEST(unterminated_vector_with_whitespace);

    /* Unterminated Map */
    RUN_TEST(unterminated_map_empty);
    RUN_TEST(unterminated_map_with_key);
    RUN_TEST(unterminated_map_with_pairs);
    RUN_TEST(unterminated_map_nested);
    RUN_TEST(unterminated_map_with_whitespace);

    /* Unterminated Set */
    RUN_TEST(unterminated_set_empty);
    RUN_TEST(unterminated_set_with_elements);
    RUN_TEST(unterminated_set_nested);
    RUN_TEST(unterminated_set_with_whitespace);

    /* Deeply Nested Unterminated Collections */
    RUN_TEST(unterminated_deeply_nested_list);
    RUN_TEST(unterminated_deeply_nested_vector);
    RUN_TEST(unterminated_deeply_nested_map);
    RUN_TEST(unterminated_deeply_nested_set);

    /* Mixed Nested Unterminated Collections */
    RUN_TEST(unterminated_mixed_list_in_vector);
    RUN_TEST(unterminated_mixed_vector_in_map);
    RUN_TEST(unterminated_mixed_map_in_set);
    RUN_TEST(unterminated_mixed_set_in_list);

    /* Mismatched Delimiters */
    RUN_TEST(mismatched_vector_with_brace);
    RUN_TEST(mismatched_list_with_bracket);
    RUN_TEST(mismatched_list_with_brace);
    RUN_TEST(mismatched_map_with_paren);
    RUN_TEST(mismatched_map_with_bracket);
    RUN_TEST(mismatched_set_with_bracket);
    RUN_TEST(mismatched_set_with_paren);
    RUN_TEST(mismatched_vector_with_paren);
    RUN_TEST(mismatched_nested_inner);
    RUN_TEST(mismatched_nested_outer);

    /* Multi-line Error Positions */
    RUN_TEST(mismatched_multiline);
    RUN_TEST(unterminated_multiline);

    TEST_SUMMARY("errors");
}
