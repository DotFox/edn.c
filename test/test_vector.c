/**
 * Test vector parser
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Empty vector */
TEST(parse_empty_vector) {
    edn_result_t result = edn_parse("[]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 0);

    edn_free(result.value);
}

/* Single element */
TEST(parse_single_element_vector) {
    edn_result_t result = edn_parse("[42]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 1);

    edn_value_t* elem = edn_vector_get(result.value, 0);
    assert(elem != NULL);
    assert(edn_type(elem) == EDN_TYPE_INT);

    int64_t val;
    assert(edn_int64_get(elem, &val));
    assert(val == 42);

    edn_free(result.value);
}

/* Multiple elements */
TEST(parse_multiple_elements_vector) {
    edn_result_t result = edn_parse("[1 2 3]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    for (int i = 0; i < 3; i++) {
        edn_value_t* elem = edn_vector_get(result.value, i);
        assert(elem != NULL);
        assert(edn_type(elem) == EDN_TYPE_INT);

        int64_t val;
        assert(edn_int64_get(elem, &val));
        assert(val == i + 1);
    }

    edn_free(result.value);
}

/* Mixed types */
TEST(parse_mixed_types_vector) {
    edn_result_t result = edn_parse("[1 \"two\" :three]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    /* Check first element (int) */
    edn_value_t* elem0 = edn_vector_get(result.value, 0);
    assert(elem0 != NULL);
    assert(edn_type(elem0) == EDN_TYPE_INT);

    /* Check second element (string) */
    edn_value_t* elem1 = edn_vector_get(result.value, 1);
    assert(elem1 != NULL);
    assert(edn_type(elem1) == EDN_TYPE_STRING);

    /* Check third element (keyword) */
    edn_value_t* elem2 = edn_vector_get(result.value, 2);
    assert(elem2 != NULL);
    assert(edn_type(elem2) == EDN_TYPE_KEYWORD);

    edn_free(result.value);
}

/* Nested vectors */
TEST(parse_nested_vectors) {
    edn_result_t result = edn_parse("[[1 2] [3 4]]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    /* Check first nested vector */
    edn_value_t* vec0 = edn_vector_get(result.value, 0);
    assert(vec0 != NULL);
    assert(edn_type(vec0) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(vec0) == 2);

    /* Check second nested vector */
    edn_value_t* vec1 = edn_vector_get(result.value, 1);
    assert(vec1 != NULL);
    assert(edn_type(vec1) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(vec1) == 2);

    edn_free(result.value);
}

/* Mixed with lists */
TEST(parse_vector_with_list) {
    edn_result_t result = edn_parse("[(1 2) [3 4]]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 2);

    /* First element is list */
    edn_value_t* elem0 = edn_vector_get(result.value, 0);
    assert(elem0 != NULL);
    assert(edn_type(elem0) == EDN_TYPE_LIST);

    /* Second element is vector */
    edn_value_t* elem1 = edn_vector_get(result.value, 1);
    assert(elem1 != NULL);
    assert(edn_type(elem1) == EDN_TYPE_VECTOR);

    edn_free(result.value);
}

/* With whitespace */
TEST(parse_vector_with_whitespace) {
    edn_result_t result = edn_parse("[  1   2   3  ]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    edn_free(result.value);
}

/* With newlines */
TEST(parse_vector_with_newlines) {
    edn_result_t result = edn_parse("[1\n2\n3]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    edn_free(result.value);
}

/* Error: unterminated vector */
TEST(error_unterminated_vector) {
    edn_result_t result = edn_parse("[1 2 3", 0);

    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
    assert(result.error_message != NULL);
}

/* Out of bounds access */
TEST(vector_get_out_of_bounds) {
    edn_result_t result = edn_parse("[1 2 3]", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_vector_count(result.value) == 3);

    /* Valid access */
    assert(edn_vector_get(result.value, 0) != NULL);
    assert(edn_vector_get(result.value, 2) != NULL);

    /* Out of bounds */
    assert(edn_vector_get(result.value, 3) == NULL);
    assert(edn_vector_get(result.value, 100) == NULL);

    edn_free(result.value);
}

/* API with wrong type */
TEST(vector_api_wrong_type) {
    edn_result_t result = edn_parse("42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* Should return 0/NULL for non-vector */
    assert(edn_vector_count(result.value) == 0);
    assert(edn_vector_get(result.value, 0) == NULL);

    edn_free(result.value);
}

/* API with NULL */
TEST(vector_api_null) {
    assert(edn_vector_count(NULL) == 0);
    assert(edn_vector_get(NULL, 0) == NULL);
}

int main(void) {
    printf("Running vector tests...\n");

    RUN_TEST(parse_empty_vector);
    RUN_TEST(parse_single_element_vector);
    RUN_TEST(parse_multiple_elements_vector);
    RUN_TEST(parse_mixed_types_vector);
    RUN_TEST(parse_nested_vectors);
    RUN_TEST(parse_vector_with_list);
    RUN_TEST(parse_vector_with_whitespace);
    RUN_TEST(parse_vector_with_newlines);
    RUN_TEST(error_unterminated_vector);
    RUN_TEST(vector_get_out_of_bounds);
    RUN_TEST(vector_api_wrong_type);
    RUN_TEST(vector_api_null);

    TEST_SUMMARY("vector");
}
