/**
 * Test comprehensive collection examples
 */

#include <string.h>

#include "../include/edn.h"
#include "test_framework.h"

/* Complex nested structure */
TEST(parse_complex_nested) {
    const char* input = "{"
                        ":name \"Alice\" "
                        ":age 30 "
                        ":languages [:clojure :rust \"C\"] "
                        ":projects #{"
                        "{:name \"edn.c\" :stars 100} "
                        "{:name \"other\" :stars 50}"
                        "} "
                        ":metadata {:created 2024 :updated 2024}"
                        "}";

    edn_result_t result = edn_parse(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert(edn_map_count(result.value) == 5);

    edn_free(result.value);
}

/* List with various types */
TEST(parse_list_various_types) {
    const char* input = "(1 \"two\" :three [4 5] {:key 6} #{7 8})";

    edn_result_t result = edn_parse(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);
    assert(edn_list_count(result.value) == 6);

    /* Check types of elements */
    assert(edn_type(edn_list_get(result.value, 0)) == EDN_TYPE_INT);
    assert(edn_type(edn_list_get(result.value, 1)) == EDN_TYPE_STRING);
    assert(edn_type(edn_list_get(result.value, 2)) == EDN_TYPE_KEYWORD);
    assert(edn_type(edn_list_get(result.value, 3)) == EDN_TYPE_VECTOR);
    assert(edn_type(edn_list_get(result.value, 4)) == EDN_TYPE_MAP);
    assert(edn_type(edn_list_get(result.value, 5)) == EDN_TYPE_SET);

    edn_free(result.value);
}

/* Vector of vectors */
TEST(parse_matrix) {
    const char* input = "[[1 2 3] [4 5 6] [7 8 9]]";

    edn_result_t result = edn_parse(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(result.value) == 3);

    /* Check first row */
    edn_value_t* row0 = edn_vector_get(result.value, 0);
    assert(row0 != NULL);
    assert(edn_type(row0) == EDN_TYPE_VECTOR);
    assert(edn_vector_count(row0) == 3);

    edn_free(result.value);
}

/* Map with nested collections */
TEST(parse_map_with_nested_collections) {
    const char* input = "{"
                        ":list (1 2 3)"
                        ":vector [4 5 6]"
                        ":set #{7 8 9}"
                        ":map {:a 10 :b 20}"
                        "}";

    edn_result_t result = edn_parse(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);
    assert(edn_map_count(result.value) == 4);

    edn_free(result.value);
}

/* Set of keywords */
TEST(parse_set_of_keywords) {
    const char* input = "#{:read :write :execute}";

    edn_result_t result = edn_parse(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_SET);
    assert(edn_set_count(result.value) == 3);

    /* Check that all elements are keywords */
    for (size_t i = 0; i < 3; i++) {
        edn_value_t* elem = edn_set_get(result.value, i);
        assert(elem != NULL);
        assert(edn_type(elem) == EDN_TYPE_KEYWORD);
    }

    edn_free(result.value);
}

/* Empty collections */
TEST(parse_empty_collections) {
    const char* input = "(() [] {} #{})";

    edn_result_t result = edn_parse(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_LIST);
    assert(edn_list_count(result.value) == 4);

    /* Check all are empty */
    assert(edn_list_count(edn_list_get(result.value, 0)) == 0);
    assert(edn_vector_count(edn_list_get(result.value, 1)) == 0);
    assert(edn_map_count(edn_list_get(result.value, 2)) == 0);
    assert(edn_set_count(edn_list_get(result.value, 3)) == 0);

    edn_free(result.value);
}

/* Real-world example: configuration */
TEST(parse_config_example) {
    const char* input = "{"
                        ":server {:host \"localhost\" :port 8080}"
                        ":database {:url \"postgres://localhost/db\" :pool-size 10}"
                        ":features #{:auth :logging :caching}"
                        ":allowed-origins [\"https://example.com\" \"https://app.example.com\"]"
                        "}";

    edn_result_t result = edn_parse(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_MAP);

    /* Look up :features */
    edn_result_t key_result = edn_parse(":features", 0);
    assert(key_result.error == EDN_OK);

    edn_value_t* features = edn_map_lookup(result.value, key_result.value);
    assert(features != NULL);
    assert(edn_type(features) == EDN_TYPE_SET);
    assert(edn_set_count(features) == 3);

    edn_free(key_result.value);
    edn_free(result.value);
}

/* Deeply nested structure */
TEST(parse_deep_nesting) {
    const char* input = "[[[[[[[[[[42]]]]]]]]]]";

    edn_result_t result = edn_parse(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);

    /* Navigate through nesting */
    edn_value_t* current = result.value;
    for (int i = 0; i < 10; i++) {
        assert(edn_type(current) == EDN_TYPE_VECTOR);
        assert(edn_vector_count(current) == 1);
        current = edn_vector_get(current, 0);
        assert(current != NULL);
    }

    /* Final value should be 42 */
    assert(edn_type(current) == EDN_TYPE_INT);
    int64_t val;
    assert(edn_int64_get(current, &val));
    assert(val == 42);

    edn_free(result.value);
}

/* Mixed collection types nested */
TEST(parse_mixed_nesting) {
    const char* input = "[{:list (1 2 #{3})}]";

    edn_result_t result = edn_parse(input, 0);

    assert(result.error == EDN_OK);
    assert(result.value != NULL);
    assert(edn_type(result.value) == EDN_TYPE_VECTOR);

    edn_value_t* map = edn_vector_get(result.value, 0);
    assert(map != NULL);
    assert(edn_type(map) == EDN_TYPE_MAP);

    edn_value_t* list = edn_map_get_value(map, 0);
    assert(list != NULL);
    assert(edn_type(list) == EDN_TYPE_LIST);
    assert(edn_list_count(list) == 3);

    edn_value_t* set = edn_list_get(list, 2);
    assert(set != NULL);
    assert(edn_type(set) == EDN_TYPE_SET);
    assert(edn_set_count(set) == 1);

    edn_free(result.value);
}

int main(void) {
    printf("Running collection integration tests...\n");

    RUN_TEST(parse_complex_nested);
    RUN_TEST(parse_list_various_types);
    RUN_TEST(parse_matrix);
    RUN_TEST(parse_map_with_nested_collections);
    RUN_TEST(parse_set_of_keywords);
    RUN_TEST(parse_empty_collections);
    RUN_TEST(parse_config_example);
    RUN_TEST(parse_deep_nesting);
    RUN_TEST(parse_mixed_nesting);

    TEST_SUMMARY("collection integration");
}
