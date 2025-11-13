/**
 * Test set parser
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Empty set */
TEST(parse_empty_set) {
    edn_result_t result = edn_parse("#{}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SET);
    assert(edn_set_count(result.value) == 0);

    edn_free(result.value);
}

/* Single element */
TEST(parse_single_element_set) {
    edn_result_t result = edn_parse("#{42}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SET);
    assert(edn_set_count(result.value) == 1);

    edn_value_t* elem = edn_set_get(result.value, 0);
    assert(elem != NULL);
    assert(edn_type(elem) == EDN_TYPE_INT);

    int64_t val;
    assert(edn_int64_get(elem, &val));
    assert(val == 42);

    edn_free(result.value);
}

/* Multiple unique elements */
TEST(parse_multiple_unique_elements_set) {
    edn_result_t result = edn_parse("#{1 2 3}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SET);
    assert(edn_set_count(result.value) == 3);

    edn_free(result.value);
}

/* Mixed types */
TEST(parse_mixed_types_set) {
    edn_result_t result = edn_parse("#{1 \"two\" :three}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SET);
    assert(edn_set_count(result.value) == 3);

    edn_free(result.value);
}

/* Set contains */
TEST(set_contains_element) {
    edn_result_t result = edn_parse("#{1 2 3}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* Create test element */
    edn_result_t elem_result = edn_parse("2", 0);
    assert(elem_result.error == EDN_OK);

    /* Should contain 2 */
    assert(edn_set_contains(result.value, elem_result.value));

    edn_free(elem_result.value);

    /* Create element not in set */
    edn_result_t not_in_result = edn_parse("42", 0);
    assert(not_in_result.error == EDN_OK);

    /* Should not contain 42 */
    assert(!edn_set_contains(result.value, not_in_result.value));

    edn_free(not_in_result.value);
    edn_free(result.value);
}

/* Reject duplicate integers (EDN spec requirement) */
TEST(reject_duplicate_integers) {
    edn_result_t result = edn_parse("#{1 2 1}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Reject duplicate strings (EDN spec requirement) */
TEST(reject_duplicate_strings) {
    edn_result_t result = edn_parse("#{\"foo\" \"bar\" \"foo\"}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Reject duplicate keywords (EDN spec requirement) */
TEST(reject_duplicate_keywords) {
    edn_result_t result = edn_parse("#{:a :b :a}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Reject all duplicates (EDN spec requirement) */
TEST(reject_all_duplicates) {
    edn_result_t result = edn_parse("#{42 42 42}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Nested collections - unique */
TEST(parse_set_with_vectors_unique) {
    edn_result_t result = edn_parse("#{[1 2] [3 4]}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SET);
    assert(edn_set_count(result.value) == 2);

    edn_free(result.value);
}

/* Reject duplicate nested collections (EDN spec requirement) */
TEST(reject_duplicate_nested_collections) {
    edn_result_t result = edn_parse("#{[1 2] [1 2]}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* With whitespace */
TEST(parse_set_with_whitespace) {
    edn_result_t result = edn_parse("#{  1   2   3  }", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_set_count(result.value) == 3);

    edn_free(result.value);
}

/* Error: unterminated set */
TEST(error_unterminated_set) {
    edn_result_t result = edn_parse("#{1 2 3", 0);

    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
}

/* Large set - no duplicates */
TEST(parse_large_set_unique) {
    edn_result_t result = edn_parse("#{1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_set_count(result.value) == 20);

    edn_free(result.value);
}

/* Large set - with duplicates (EDN spec - should reject) */
TEST(parse_large_set_with_duplicates) {
    edn_result_t result = edn_parse("#{1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 1}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Out of bounds access */
TEST(set_get_out_of_bounds) {
    edn_result_t result = edn_parse("#{1 2 3}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_set_count(result.value) == 3);

    /* Valid access */
    assert(edn_set_get(result.value, 0) != NULL);
    assert(edn_set_get(result.value, 2) != NULL);

    /* Out of bounds */
    assert(edn_set_get(result.value, 3) == NULL);
    assert(edn_set_get(result.value, 100) == NULL);

    edn_free(result.value);
}

/* API with wrong type */
TEST(set_api_wrong_type) {
    edn_result_t result = edn_parse("42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* Should return 0/false/NULL for non-set */
    assert(edn_set_count(result.value) == 0);
    assert(edn_set_get(result.value, 0) == NULL);
    assert(!edn_set_contains(result.value, result.value));

    edn_free(result.value);
}

/* API with NULL */
TEST(set_api_null) {
    assert(edn_set_count(NULL) == 0);
    assert(edn_set_get(NULL, 0) == NULL);
    assert(!edn_set_contains(NULL, NULL));
}

int main(void) {
    printf("Running set tests...\n");

    RUN_TEST(parse_empty_set);
    RUN_TEST(parse_single_element_set);
    RUN_TEST(parse_multiple_unique_elements_set);
    RUN_TEST(parse_mixed_types_set);
    RUN_TEST(set_contains_element);
    RUN_TEST(reject_duplicate_integers);
    RUN_TEST(reject_duplicate_strings);
    RUN_TEST(reject_duplicate_keywords);
    RUN_TEST(reject_all_duplicates);
    RUN_TEST(parse_set_with_vectors_unique);
    RUN_TEST(reject_duplicate_nested_collections);
    RUN_TEST(parse_set_with_whitespace);
    RUN_TEST(error_unterminated_set);
    RUN_TEST(parse_large_set_unique);
    RUN_TEST(parse_large_set_with_duplicates);
    RUN_TEST(set_get_out_of_bounds);
    RUN_TEST(set_api_wrong_type);
    RUN_TEST(set_api_null);

    TEST_SUMMARY("set");
}
