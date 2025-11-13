/**
 * Test that duplicates in sets and maps are rejected (EDN spec requirement)
 */

#include "../include/edn.h"
#include "test_framework.h"

/* Test: Set with duplicate elements should be rejected */
TEST(set_duplicate_elements_rejected) {
    edn_result_t result = edn_parse("#{1 2 1}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Test: Set without duplicates should work */
TEST(set_no_duplicates_allowed) {
    edn_result_t result = edn_parse("#{1 2 3}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_set_count(result.value) == 3);

    edn_free(result.value);
}

/* Test: Map with duplicate keys should be rejected */
TEST(map_duplicate_keys_rejected) {
    edn_result_t result = edn_parse("{:a 1 :b 2 :a 3}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

/* Test: Map without duplicate keys should work */
TEST(map_no_duplicates_allowed) {
    edn_result_t result = edn_parse("{:a 1 :b 2 :c 3}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_map_count(result.value) == 3);

    edn_free(result.value);
}

/* Test: Set with duplicate strings */
TEST(set_duplicate_strings_rejected) {
    edn_result_t result = edn_parse("#{\"foo\" \"bar\" \"foo\"}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Test: Map with duplicate complex keys (vectors) */
TEST(map_duplicate_vector_keys_rejected) {
    edn_result_t result = edn_parse("{[1 2] :a [1 2] :b}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

/* Test: Set with duplicate keywords */
TEST(set_duplicate_keywords_rejected) {
    edn_result_t result = edn_parse("#{:foo :bar :foo}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Test: Map with nested duplicate check */
TEST(map_nested_duplicate_keys_rejected) {
    edn_result_t result = edn_parse("{{:x 1} :a {:x 1} :b}", 0);

    /* Maps as keys, structurally equal */
    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

/* Test: Empty set and map (no duplicates possible) */
TEST(empty_collections_no_error) {
    edn_result_t r1 = edn_parse("#{}", 0);
    assert(r1.error == EDN_OK);
    assert(edn_set_count(r1.value) == 0);
    edn_free(r1.value);

    edn_result_t r2 = edn_parse("{}", 0);
    assert(r2.error == EDN_OK);
    assert(edn_map_count(r2.value) == 0);
    edn_free(r2.value);
}

/* Test: Single element set and map (no duplicates possible) */
TEST(single_element_no_error) {
    edn_result_t r1 = edn_parse("#{42}", 0);
    assert(r1.error == EDN_OK);
    assert(edn_set_count(r1.value) == 1);
    edn_free(r1.value);

    edn_result_t r2 = edn_parse("{:a 1}", 0);
    assert(r2.error == EDN_OK);
    assert(edn_map_count(r2.value) == 1);
    edn_free(r2.value);
}

/* Test: Large set without duplicates (tests sorted algorithm) */
TEST(large_set_no_duplicates) {
    edn_result_t result = edn_parse("#{1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_set_count(result.value) == 20);

    edn_free(result.value);
}

/* Test: Large set with duplicate at end (tests sorted algorithm) */
TEST(large_set_duplicate_at_end) {
    edn_result_t result = edn_parse("#{1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 1}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_ELEMENT);
    assert(result.value == NULL);
}

/* Test: Set with mixed types - no duplicates */
TEST(set_mixed_types_no_duplicates) {
    edn_result_t result = edn_parse("#{1 \"1\" :foo foo true 1.0}", 0);

    /* All different types, no duplicates */
    assert(result.error == EDN_OK);
    assert(edn_set_count(result.value) == 6);

    edn_free(result.value);
}

/* Test: Map with different value for same key still duplicate */
TEST(map_same_key_different_value_rejected) {
    edn_result_t result = edn_parse("{1 :a 2 :b 1 :c}", 0);

    /* Key 1 appears twice, even with different values */
    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

int main(void) {
    printf("Running duplicate detection tests...\n");

    RUN_TEST(set_duplicate_elements_rejected);
    RUN_TEST(set_no_duplicates_allowed);
    RUN_TEST(map_duplicate_keys_rejected);
    RUN_TEST(map_no_duplicates_allowed);
    RUN_TEST(set_duplicate_strings_rejected);
    RUN_TEST(map_duplicate_vector_keys_rejected);
    RUN_TEST(set_duplicate_keywords_rejected);
    RUN_TEST(map_nested_duplicate_keys_rejected);
    RUN_TEST(empty_collections_no_error);
    RUN_TEST(single_element_no_error);
    RUN_TEST(large_set_no_duplicates);
    RUN_TEST(large_set_duplicate_at_end);
    RUN_TEST(set_mixed_types_no_duplicates);
    RUN_TEST(map_same_key_different_value_rejected);

    TEST_SUMMARY("duplicate detection");
}
