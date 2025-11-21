/**
 * Test map parser
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Empty map */
TEST(parse_empty_map) {
    edn_result_t result = edn_read("{}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert(edn_map_count(result.value) == 0);

    edn_free(result.value);
}

/* Single pair */
TEST(parse_single_pair_map) {
    edn_result_t result = edn_read("{:a 1}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert(edn_map_count(result.value) == 1);

    /* Get key and value at index 0 */
    edn_value_t* key = edn_map_get_key(result.value, 0);
    edn_value_t* val = edn_map_get_value(result.value, 0);

    assert(key != NULL);
    assert(val != NULL);
    assert(edn_type(key) == EDN_TYPE_KEYWORD);
    assert(edn_type(val) == EDN_TYPE_INT);

    edn_free(result.value);
}

/* Multiple pairs */
TEST(parse_multiple_pairs_map) {
    edn_result_t result = edn_read("{:a 1 :b 2 :c 3}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert(edn_map_count(result.value) == 3);

    edn_free(result.value);
}

/* Lookup by key */
TEST(map_lookup) {
    edn_result_t result = edn_read("{:foo 10 :bar 20 :baz 30}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* Create lookup key */
    edn_result_t key_result = edn_read(":bar", 0);
    assert(key_result.error == EDN_OK);

    /* Lookup value */
    edn_value_t* val = edn_map_lookup(result.value, key_result.value);
    assert(val != NULL);
    assert(edn_type(val) == EDN_TYPE_INT);

    int64_t num;
    assert(edn_int64_get(val, &num));
    assert(num == 20);

    edn_free(key_result.value);
    edn_free(result.value);
}

/* Lookup non-existent key */
TEST(map_lookup_not_found) {
    edn_result_t result = edn_read("{:foo 10 :bar 20}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* Create lookup key not in map */
    edn_result_t key_result = edn_read(":baz", 0);
    assert(key_result.error == EDN_OK);

    /* Lookup should return NULL */
    edn_value_t* val = edn_map_lookup(result.value, key_result.value);
    assert(val == NULL);

    edn_free(key_result.value);
    edn_free(result.value);
}

/* Contains key */
TEST(map_contains_key) {
    edn_result_t result = edn_read("{:foo 10 :bar 20}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* Key exists */
    edn_result_t key1_result = edn_read(":foo", 0);
    assert(key1_result.error == EDN_OK);
    assert(edn_map_contains_key(result.value, key1_result.value));
    edn_free(key1_result.value);

    /* Key doesn't exist */
    edn_result_t key2_result = edn_read(":baz", 0);
    assert(key2_result.error == EDN_OK);
    assert(!edn_map_contains_key(result.value, key2_result.value));
    edn_free(key2_result.value);

    edn_free(result.value);
}

/* Mixed types for keys and values */
TEST(parse_mixed_types_map) {
    edn_result_t result = edn_read("{1 :one \"two\" 2 :three [3]}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_map_count(result.value) == 3);

    edn_free(result.value);
}

/* Nested maps */
TEST(parse_nested_maps) {
    edn_result_t result = edn_read("{:a {:b 1} :c {:d 2}}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_map_count(result.value) == 2);

    /* First value should be a map */
    edn_value_t* val0 = edn_map_get_value(result.value, 0);
    assert(val0 != NULL);
    assert(edn_type(val0) == EDN_TYPE_MAP);

    edn_free(result.value);
}

/* Map with vector values */
TEST(parse_map_with_vectors) {
    edn_result_t result = edn_read("{:a [1 2] :b [3 4]}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_map_count(result.value) == 2);

    edn_free(result.value);
}

/* Map rejects duplicate keys (EDN spec requirement) */
TEST(reject_duplicate_keys) {
    edn_result_t result = edn_read("{:a 1 :a 2}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

/* Map rejects duplicate keys (different values) */
TEST(reject_duplicate_integer_keys) {
    edn_result_t result = edn_read("{1 \"one\" 1 \"ONE\"}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

/* Error: odd number of elements */
TEST(error_odd_elements) {
    edn_result_t result = edn_read("{:a 1 :b}", 0);

    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
    assert(result.error_message != NULL);
}

/* Error: single key without value */
TEST(error_single_key_no_value) {
    edn_result_t result = edn_read("{:a}", 0);

    assert(result.error == EDN_ERROR_INVALID_SYNTAX);
    assert(result.value == NULL);
}

/* Error: unterminated map */
TEST(error_unterminated_map) {
    edn_result_t result = edn_read("{:a 1", 0);

    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
}

/* Error: key without value at EOF */
TEST(error_key_without_value_eof) {
    edn_result_t result = edn_read("{:a 1 :b", 0);

    assert(result.error == EDN_ERROR_UNEXPECTED_EOF);
    assert(result.value == NULL);
}

/* With whitespace */
TEST(parse_map_with_whitespace) {
    edn_result_t result = edn_read("{  :a   1  :b  2  }", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_map_count(result.value) == 2);

    edn_free(result.value);
}

/* With newlines */
TEST(parse_map_with_newlines) {
    edn_result_t result = edn_read("{\n:a 1\n:b 2\n}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_map_count(result.value) == 2);

    edn_free(result.value);
}

/* With comments */
TEST(parse_map_with_comments) {
    edn_result_t result = edn_read("{:a 1 ; comment\n :b 2}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_map_count(result.value) == 2);

    edn_free(result.value);
}

/* Large map - no duplicates */
TEST(parse_large_map_unique) {
    edn_result_t result =
        edn_read("{:k1 1 :k2 2 :k3 3 :k4 4 :k5 5 :k6 6 :k7 7 :k8 8 :k9 9 :k10 10}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_map_count(result.value) == 10);

    edn_free(result.value);
}

/* Large map - rejects duplicates (EDN spec requirement) */
TEST(parse_large_map_with_duplicate) {
    edn_result_t result =
        edn_read("{:k1 1 :k2 2 :k3 3 :k4 4 :k5 5 :k6 6 :k7 7 :k8 8 :k9 9 :k1 10}", 0);

    assert(result.error == EDN_ERROR_DUPLICATE_KEY);
    assert(result.value == NULL);
}

/* Out of bounds access */
TEST(map_get_out_of_bounds) {
    edn_result_t result = edn_read("{:a 1 :b 2}", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_map_count(result.value) == 2);

    /* Valid access */
    assert(edn_map_get_key(result.value, 0) != NULL);
    assert(edn_map_get_value(result.value, 0) != NULL);
    assert(edn_map_get_key(result.value, 1) != NULL);
    assert(edn_map_get_value(result.value, 1) != NULL);

    /* Out of bounds */
    assert(edn_map_get_key(result.value, 2) == NULL);
    assert(edn_map_get_value(result.value, 2) == NULL);
    assert(edn_map_get_key(result.value, 100) == NULL);
    assert(edn_map_get_value(result.value, 100) == NULL);

    edn_free(result.value);
}

/* API with wrong type */
TEST(map_api_wrong_type) {
    edn_result_t result = edn_read("42", 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* Should return 0/false/NULL for non-map */
    assert(edn_map_count(result.value) == 0);
    assert(edn_map_get_key(result.value, 0) == NULL);
    assert(edn_map_get_value(result.value, 0) == NULL);
    assert(edn_map_lookup(result.value, result.value) == NULL);
    assert(!edn_map_contains_key(result.value, result.value));

    edn_free(result.value);
}

/* API with NULL */
TEST(map_api_null) {
    assert(edn_map_count(NULL) == 0);
    assert(edn_map_get_key(NULL, 0) == NULL);
    assert(edn_map_get_value(NULL, 0) == NULL);
    assert(edn_map_lookup(NULL, NULL) == NULL);
    assert(!edn_map_contains_key(NULL, NULL));
}

int main(void) {
    printf("Running map tests...\n");

    RUN_TEST(parse_empty_map);
    RUN_TEST(parse_single_pair_map);
    RUN_TEST(parse_multiple_pairs_map);
    RUN_TEST(map_lookup);
    RUN_TEST(map_lookup_not_found);
    RUN_TEST(map_contains_key);
    RUN_TEST(parse_mixed_types_map);
    RUN_TEST(parse_nested_maps);
    RUN_TEST(parse_map_with_vectors);
    RUN_TEST(reject_duplicate_keys);
    RUN_TEST(reject_duplicate_integer_keys);
    RUN_TEST(error_odd_elements);
    RUN_TEST(error_single_key_no_value);
    RUN_TEST(error_unterminated_map);
    RUN_TEST(error_key_without_value_eof);
    RUN_TEST(parse_map_with_whitespace);
    RUN_TEST(parse_map_with_newlines);
    RUN_TEST(parse_map_with_comments);
    RUN_TEST(parse_large_map_unique);
    RUN_TEST(parse_large_map_with_duplicate);
    RUN_TEST(map_get_out_of_bounds);
    RUN_TEST(map_api_wrong_type);
    RUN_TEST(map_api_null);

    TEST_SUMMARY("map");
}
