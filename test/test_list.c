/**
 * Test list parser
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Empty list */
TEST(parse_empty_list) {
    edn_result_t result = edn_read("()", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);
    assert(edn_list_count(result.value) == 0);

    edn_free(result.value);
}

/* Single element */
TEST(parse_single_element_list) {
    edn_result_t result = edn_read("(42)", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);
    assert(edn_list_count(result.value) == 1);

    edn_value_t* elem = edn_list_get(result.value, 0);
    assert(elem != NULL);
    assert(edn_type(elem) == EDN_TYPE_INT);

    int64_t val;
    assert(edn_int64_get(elem, &val));
    assert(val == 42);

    edn_free(result.value);
}

/* Multiple elements */
TEST(parse_multiple_elements_list) {
    edn_result_t result = edn_read("(1 2 3)", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);
    assert(edn_list_count(result.value) == 3);

    for (int i = 0; i < 3; i++) {
        edn_value_t* elem = edn_list_get(result.value, i);
        assert(elem != NULL);
        assert(edn_type(elem) == EDN_TYPE_INT);

        int64_t val;
        assert(edn_int64_get(elem, &val));
        assert(val == i + 1);
    }

    edn_free(result.value);
}

/* Mixed types */
TEST(parse_mixed_types_list) {
    edn_result_t result = edn_read("(1 \"two\" :three)", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);
    assert(edn_list_count(result.value) == 3);

    /* Check first element (int) */
    edn_value_t* elem0 = edn_list_get(result.value, 0);
    assert(elem0 != NULL);
    assert(edn_type(elem0) == EDN_TYPE_INT);

    /* Check second element (string) */
    edn_value_t* elem1 = edn_list_get(result.value, 1);
    assert(elem1 != NULL);
    assert(edn_type(elem1) == EDN_TYPE_STRING);

    /* Check third element (keyword) */
    edn_value_t* elem2 = edn_list_get(result.value, 2);
    assert(elem2 != NULL);
    assert(edn_type(elem2) == EDN_TYPE_KEYWORD);

    edn_free(result.value);
}

/* Nested lists */
TEST(parse_nested_lists) {
    edn_result_t result = edn_read("((1 2) (3 4))", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);
    assert(edn_list_count(result.value) == 2);

    /* Check first nested list */
    edn_value_t* list0 = edn_list_get(result.value, 0);
    assert(list0 != NULL);
    assert(edn_type(list0) == EDN_TYPE_LIST);
    assert(edn_list_count(list0) == 2);

    /* Check second nested list */
    edn_value_t* list1 = edn_list_get(result.value, 1);
    assert(list1 != NULL);
    assert(edn_type(list1) == EDN_TYPE_LIST);
    assert(edn_list_count(list1) == 2);

    edn_free(result.value);
}

/* Deeply nested */
TEST(parse_deeply_nested_lists) {
    edn_result_t result = edn_read("((((42))))", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);

    /* Navigate through nesting */
    edn_value_t* current = result.value;
    for (int i = 0; i < 4; i++) {
        assert(edn_list_count(current) == 1);
        current = edn_list_get(current, 0);
        assert(current != NULL);
    }

    /* Final value should be 42 */
    assert(edn_type(current) == EDN_TYPE_INT);
    int64_t val;
    assert(edn_int64_get(current, &val));
    assert(val == 42);

    edn_free(result.value);
}

/* With whitespace */
TEST(parse_list_with_whitespace) {
    edn_result_t result = edn_read("(  1   2   3  )", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);
    assert(edn_list_count(result.value) == 3);

    edn_free(result.value);
}

/* With newlines */
TEST(parse_list_with_newlines) {
    edn_result_t result = edn_read("(1\n2\n3)", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);
    assert(edn_list_count(result.value) == 3);

    edn_free(result.value);
}

/* With comments */
TEST(parse_list_with_comments) {
    edn_result_t result = edn_read("(1 ; comment\n 2)", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);
    assert(edn_list_count(result.value) == 2);

    edn_free(result.value);
}

/* Error: unterminated list */
TEST(error_unterminated_list) {
    edn_result_t result = edn_read("(1 2 3", 0);

    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
    assert(result.error_message != NULL);
}

/* Error: unterminated nested list */
TEST(error_unterminated_nested_list) {
    edn_result_t result = edn_read("(1 (2 3)", 0);

    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
}

/* Out of bounds access */
TEST(list_get_out_of_bounds) {
    edn_result_t result = edn_read("(1 2 3)", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_list_count(result.value) == 3);

    /* Valid access */
    assert(edn_list_get(result.value, 0) != NULL);
    assert(edn_list_get(result.value, 2) != NULL);

    /* Out of bounds */
    assert(edn_list_get(result.value, 3) == NULL);
    assert(edn_list_get(result.value, 100) == NULL);

    edn_free(result.value);
}

/* API with wrong type */
TEST(list_api_wrong_type) {
    edn_result_t result = edn_read("42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* Should return 0/NULL for non-list */
    assert(edn_list_count(result.value) == 0);
    assert(edn_list_get(result.value, 0) == NULL);

    edn_free(result.value);
}

/* API with NULL */
TEST(list_api_null) {
    assert(edn_list_count(NULL) == 0);
    assert(edn_list_get(NULL, 0) == NULL);
}

int main(void) {
    printf("Running list tests...\n");

    RUN_TEST(parse_empty_list);
    RUN_TEST(parse_single_element_list);
    RUN_TEST(parse_multiple_elements_list);
    RUN_TEST(parse_mixed_types_list);
    RUN_TEST(parse_nested_lists);
    RUN_TEST(parse_deeply_nested_lists);
    RUN_TEST(parse_list_with_whitespace);
    RUN_TEST(parse_list_with_newlines);
    RUN_TEST(parse_list_with_comments);
    RUN_TEST(error_unterminated_list);
    RUN_TEST(error_unterminated_nested_list);
    RUN_TEST(list_get_out_of_bounds);
    RUN_TEST(list_api_wrong_type);
    RUN_TEST(list_api_null);

    TEST_SUMMARY("list");
}
